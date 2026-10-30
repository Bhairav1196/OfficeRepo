/**
 * @file eip_transport.c
 * @brief POSIX TCP/UDP EtherNet/IP transport and cyclic I/O exchange.
 * @author Prathamesh Chipkar
 */
#define _GNU_SOURCE
#include "ead/eip_transport.h"
#include "ead/compiler.h"
#include "ead/logger.h"

#include "app_config.h"
#include "cip_connection.h"
#include "cip_explicit.h"
#include "eip_endian.h"
#include "eip_session.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

struct ead_eip_transport
{
    ead_scanner_config_t config;
    eip_session_t session;
    bool session_open;
    eip_io_connection_t connection;
    bool io_open;
    int udp_fd;
    struct sockaddr_in remote;
    uint8_t output[EAD_MAX_ASSEMBLY_SIZE];
    size_t output_size;
    size_t input_size;
    uint32_t address_sequence;
    uint16_t transport_sequence;
    uint32_t input_sequence;
    uint32_t last_address_sequence;
    bool have_address_sequence;
    uint64_t open_ns;
    uint64_t last_tx_ns;
    uint64_t last_rx_ns;
    uint64_t last_keepalive_ns;
    ead_eip_input_callback_t callback;
    void *callback_context;
};

static uint64_t fn_NowNs(void)
{
    struct timespec now;
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return ((uint64_t)now.tv_sec * UINT64_C(1000000000)) + (uint64_t)now.tv_nsec;
}

static void fn_SetError(char *error, size_t error_size, const char *format, ...) EAD_PRINTF_FORMAT(3, 4);

static void fn_SetError(char *error, size_t error_size, const char *format, ...)
{
    va_list arguments;
    if ((error == NULL) || (error_size == 0U)) return;
    va_start(arguments, format);
    (void)vsnprintf(error, error_size, format, arguments);
    va_end(arguments);
}

static ead_result_t fn_MapResult(eip_result_t result)
{
    switch (result)
    {
        case EIP_OK: return EAD_OK;
        case EIP_ERR_ARGUMENT: return EAD_ERR_ARGUMENT;
        case EIP_ERR_TIMEOUT: return EAD_ERR_TIMEOUT;
        case EIP_ERR_PROTOCOL: return EAD_ERR_PROTOCOL;
        case EIP_ERR_REMOTE: return EAD_ERR_REMOTE;
        case EIP_ERR_OVERFLOW: return EAD_ERR_RANGE;
        case EIP_ERR_CONFIG: return EAD_ERR_CONFIG;
        case EIP_ERR_SOCKET:
        case EIP_ERR_IO: return EAD_ERR_IO;
        case EIP_ERR_NOT_SUPPORTED: return EAD_ERR_STATE;
        default: return EAD_ERR_IO;
    }
}

static void fn_FillStackConfig(const ead_scanner_config_t *source,
                               app_config_t *destination)
{
    (void)memset(destination, 0, sizeof(*destination));
    (void)snprintf(destination->target_ip, sizeof(destination->target_ip), "%s", source->adapter_ip);
    (void)snprintf(destination->local_ip, sizeof(destination->local_ip), "%s", source->local_ip);
    destination->target_udp_port = source->target_udp_port;
    destination->local_udp_port = source->local_udp_port;
    destination->config_assembly = source->config_assembly;
    destination->input_assembly = source->input_assembly;
    destination->output_assembly = source->output_assembly;
    destination->o2t_rpi_us = source->o2t_rpi_us;
    destination->t2o_rpi_us = source->t2o_rpi_us;
    destination->timeout_multiplier = source->timeout_multiplier;
    destination->originator_vendor_id = source->originator_vendor_id;
    destination->originator_serial = source->originator_serial_number;
    destination->o2t_run_idle_header = source->o2t_run_idle_header;
    destination->t2o_run_idle_header = source->t2o_run_idle_header;
}

static bool fn_IsMulticast(const char *ip)
{
    struct in_addr address;
    uint32_t host;
    if ((ip == NULL) || (inet_pton(AF_INET, ip, &address) != 1)) return false;
    host = ntohl(address.s_addr);
    return (host >= UINT32_C(0xE0000000)) && (host <= UINT32_C(0xEFFFFFFF));
}

static ead_result_t fn_CreateUdp(ead_eip_transport_t *transport,
                                 char *error,
                                 size_t error_size)
{
    struct sockaddr_in local;
    int flags;
    int reuse = 1;
    transport->udp_fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
    if (transport->udp_fd < 0)
    {
        fn_SetError(error, error_size, "UDP socket: %s", strerror(errno));
        return EAD_ERR_IO;
    }
    (void)setsockopt(transport->udp_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    flags = fcntl(transport->udp_fd, F_GETFL, 0);
    if ((flags < 0) || (fcntl(transport->udp_fd, F_SETFL, flags | O_NONBLOCK) != 0))
    {
        fn_SetError(error, error_size, "UDP nonblocking mode: %s", strerror(errno));
        return EAD_ERR_IO;
    }
    (void)memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(transport->connection.t2o_port);
    if (inet_pton(AF_INET, transport->config.local_ip, &local.sin_addr) != 1)
    {
        fn_SetError(error, error_size, "invalid local_ip '%s'", transport->config.local_ip);
        return EAD_ERR_CONFIG;
    }
    if (bind(transport->udp_fd, (const struct sockaddr *)&local, sizeof(local)) != 0)
    {
        fn_SetError(error, error_size, "bind UDP %s:%u: %s", transport->config.local_ip,
                    transport->connection.t2o_port, strerror(errno));
        return EAD_ERR_IO;
    }
    if (fn_IsMulticast(transport->connection.t2o_ip))
    {
        struct ip_mreq membership;
        (void)memset(&membership, 0, sizeof(membership));
        if ((inet_pton(AF_INET, transport->connection.t2o_ip, &membership.imr_multiaddr) != 1) ||
            (inet_pton(AF_INET, transport->config.local_ip, &membership.imr_interface) != 1) ||
            (setsockopt(transport->udp_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                        &membership, sizeof(membership)) != 0))
        {
            fn_SetError(error, error_size, "join multicast %s: %s",
                        transport->connection.t2o_ip, strerror(errno));
            return EAD_ERR_IO;
        }
    }
    (void)memset(&transport->remote, 0, sizeof(transport->remote));
    transport->remote.sin_family = AF_INET;
    transport->remote.sin_port = htons(transport->connection.o2t_port);
    if (inet_pton(AF_INET, transport->connection.o2t_ip, &transport->remote.sin_addr) != 1)
    {
        fn_SetError(error, error_size, "invalid O-to-T address '%s'", transport->connection.o2t_ip);
        return EAD_ERR_CONFIG;
    }
    /*
     * A connected datagram socket still uses UDP.  It restricts received
     * datagrams to this EK9500 peer and gives Linux an unambiguous four-tuple
     * when multiple scanner processes share local port 2222 on one host.
     */
    if (connect(transport->udp_fd,
                (const struct sockaddr *)&transport->remote,
                sizeof(transport->remote)) != 0)
    {
        fn_SetError(error, error_size, "connect UDP peer %s:%u: %s",
                    transport->connection.o2t_ip,
                    transport->connection.o2t_port,
                    strerror(errno));
        return EAD_ERR_IO;
    }
    return EAD_OK;
}

static ead_result_t fn_SendIo(ead_eip_transport_t *transport,
                              char *error,
                              size_t error_size)
{
    uint8_t packet[EIP_MAX_PACKET_SIZE];
    size_t cursor = 0U;
    size_t connected_length;
    ssize_t sent;
    connected_length = 2U +
                       (transport->connection.o2t_run_idle_header ? 4U : 0U) +
                       transport->output_size;
    if ((20U + connected_length) > sizeof(packet)) return EAD_ERR_RANGE;
    fn_WriteLe16(&packet[cursor], 2U); cursor += 2U;
    fn_WriteLe16(&packet[cursor], EIP_CPF_SEQUENCED_ADDRESS); cursor += 2U;
    fn_WriteLe16(&packet[cursor], 8U); cursor += 2U;
    fn_WriteLe32(&packet[cursor], transport->connection.o2t_connection_id); cursor += 4U;
    fn_WriteLe32(&packet[cursor], ++transport->address_sequence); cursor += 4U;
    fn_WriteLe16(&packet[cursor], EIP_CPF_CONNECTED_DATA); cursor += 2U;
    fn_WriteLe16(&packet[cursor], (uint16_t)connected_length); cursor += 2U;
    fn_WriteLe16(&packet[cursor], ++transport->transport_sequence); cursor += 2U;
    if (transport->connection.o2t_run_idle_header)
    {
        fn_WriteLe32(&packet[cursor], 1U);
        cursor += 4U;
    }
    (void)memcpy(&packet[cursor], transport->output, transport->output_size);
    cursor += transport->output_size;
    sent = send(transport->udp_fd, packet, cursor, 0);
    if (sent != (ssize_t)cursor)
    {
        if ((sent < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) return EAD_ERR_TIMEOUT;
        fn_SetError(error, error_size, "O-to-T UDP send: %s", strerror(errno));
        return EAD_ERR_IO;
    }
    transport->last_tx_ns = fn_NowNs();
    fn_EadLogHex(EAD_LOG_TRACE, "O-to-T UDP", packet, cursor);
    return EAD_OK;
}

static ead_result_t fn_ParseIo(ead_eip_transport_t *transport,
                               const uint8_t *packet,
                               size_t packet_length,
                               char *error,
                               size_t error_size)
{
    uint16_t item_count;
    size_t cursor = 2U;
    uint32_t connection_id = 0U;
    uint32_t address_sequence = 0U;
    bool have_address = false;
    const uint8_t *connected_data = NULL;
    size_t connected_length = 0U;
    size_t application_offset = 2U;
    size_t application_length;
    if ((packet == NULL) || (packet_length < 2U)) return EAD_ERR_PROTOCOL;
    item_count = fn_ReadLe16(packet);
    for (uint16_t index = 0U; index < item_count; ++index)
    {
        uint16_t type_id;
        uint16_t item_length;
        if ((cursor + 4U) > packet_length) return EAD_ERR_PROTOCOL;
        type_id = fn_ReadLe16(&packet[cursor]);
        item_length = fn_ReadLe16(&packet[cursor + 2U]);
        cursor += 4U;
        if ((cursor + item_length) > packet_length) return EAD_ERR_PROTOCOL;
        if ((type_id == EIP_CPF_SEQUENCED_ADDRESS) && (item_length == 8U))
        {
            if (have_address) return EAD_ERR_PROTOCOL;
            have_address = true;
            connection_id = fn_ReadLe32(&packet[cursor]);
            address_sequence = fn_ReadLe32(&packet[cursor + 4U]);
        }
        else if (type_id == EIP_CPF_CONNECTED_DATA)
        {
            if (connected_data != NULL) return EAD_ERR_PROTOCOL;
            connected_data = &packet[cursor];
            connected_length = item_length;
        }
        cursor += item_length;
    }
    if (!have_address || item_count != 2U || cursor != packet_length ||
        (connection_id != transport->connection.t2o_connection_id) ||
        (connected_data == NULL) || (connected_length < 2U)) return EAD_ERR_PROTOCOL;
    if (transport->connection.t2o_run_idle_header) application_offset += 4U;
    if (connected_length < application_offset) return EAD_ERR_PROTOCOL;
    application_length = connected_length - application_offset;
    if (application_length != transport->input_size)
    {
        fn_SetError(error, error_size, "T-to-O size mismatch: expected %zu, received %zu",
                    transport->input_size, application_length);
        return EAD_ERR_PROTOCOL;
    }
    if (transport->have_address_sequence)
    {
        const uint32_t delta = address_sequence - transport->last_address_sequence;
        if (delta == 0U || delta >= UINT32_C(0x80000000)) return EAD_ERR_PROTOCOL;
    }
    transport->have_address_sequence = true;
    transport->last_address_sequence = address_sequence;
    ++transport->input_sequence;
    transport->last_rx_ns = fn_NowNs();
    if (transport->callback != NULL)
    {
        transport->callback(&connected_data[application_offset], application_length,
                            transport->input_sequence, transport->callback_context);
    }
    fn_EadLogHex(EAD_LOG_TRACE, "T-to-O UDP", packet, packet_length);
    return EAD_OK;
}

ead_eip_transport_t *fn_EadEipCreate(void)
{
    ead_eip_transport_t *transport = (ead_eip_transport_t *)calloc(1U, sizeof(*transport));
    if (transport != NULL)
    {
        transport->session.tcp_fd = -1;
        transport->udp_fd = -1;
    }
    return transport;
}

void fn_EadEipDestroy(ead_eip_transport_t *transport)
{
    if (transport == NULL) return;
    fn_EadEipDisconnect(transport);
    free(transport);
}

ead_result_t fn_EadEipConnect(ead_eip_transport_t *transport,
                              const ead_scanner_config_t *config,
                              size_t input_size,
                              size_t output_size,
                              ead_eip_input_callback_t callback,
                              void *callback_context,
                              char *error,
                              size_t error_size)
{
    eip_result_t result;
    if ((transport == NULL) || (config == NULL) ||
        (input_size > EAD_MAX_ASSEMBLY_SIZE) ||
        (output_size > EAD_MAX_ASSEMBLY_SIZE) ||
        (input_size > UINT16_MAX) || (output_size > UINT16_MAX)) return EAD_ERR_ARGUMENT;
    fn_EadEipDisconnect(transport);
    transport->config = *config;
    transport->input_size = input_size;
    transport->output_size = output_size;
    transport->callback = callback;
    transport->callback_context = callback_context;
    result = fn_EipSessionOpen(&transport->session, config->adapter_ip,
                               config->tcp_port, 3000);
    if (result != EIP_OK)
    {
        fn_SetError(error, error_size, "Register Session failed (%d)", result);
        return fn_MapResult(result);
    }
    transport->session_open = true;
    return EAD_OK;
}

ead_result_t fn_EadEipReadAssembly(ead_eip_transport_t *transport,
                                   uint16_t instance,
                                   uint8_t *data,
                                   size_t capacity,
                                   size_t *size,
                                   char *error,
                                   size_t error_size)
{
    cip_response_view_t response;
    eip_result_t result;
    if ((transport == NULL) || !transport->session_open || (data == NULL) || (size == NULL))
        return EAD_ERR_ARGUMENT;
    result = fn_CipGetAttributeSingle(&transport->session, CIP_CLASS_ASSEMBLY,
                                      instance, 3U, data, capacity, size, &response);
    if (result != EIP_OK)
    {
        fn_SetError(error, error_size, "read Assembly %u failed (%d)", instance, result);
        return fn_MapResult(result);
    }
    return EAD_OK;
}

ead_result_t fn_EadEipWriteAssembly(ead_eip_transport_t *transport,
                                    uint16_t instance,
                                    const uint8_t *data,
                                    size_t size,
                                    char *error,
                                    size_t error_size)
{
    cip_response_view_t response;
    eip_result_t result;
    if ((transport == NULL) || !transport->session_open || (data == NULL))
        return EAD_ERR_ARGUMENT;
    result = fn_CipSetAttributeSingle(&transport->session, CIP_CLASS_ASSEMBLY,
                                      instance, 3U, data, size, &response);
    if (result != EIP_OK)
    {
        fn_SetError(error, error_size, "write Assembly %u failed (%d)", instance, result);
        return fn_MapResult(result);
    }
    return EAD_OK;
}

ead_result_t fn_EadEipOpenIo(ead_eip_transport_t *transport,
                             const uint8_t *initial_output,
                             size_t size,
                             char *error,
                             size_t error_size)
{
    app_config_t stack_config;
    eip_result_t result;
    ead_result_t ead_result;
    if ((transport == NULL) || !transport->session_open ||
        (initial_output == NULL) || (size != transport->output_size)) return EAD_ERR_ARGUMENT;
    if (transport->io_open) return EAD_ERR_STATE;
    fn_FillStackConfig(&transport->config, &stack_config);
    result = fn_CipForwardOpen(&transport->session, &stack_config,
                               (uint16_t)transport->output_size,
                               (uint16_t)transport->input_size,
                               &transport->connection);
    if (result != EIP_OK)
    {
        fn_SetError(error, error_size, "Forward Open failed (%d)", result);
        return fn_MapResult(result);
    }
    (void)memcpy(transport->output, initial_output, size);
    ead_result = fn_CreateUdp(transport, error, error_size);
    if (ead_result != EAD_OK)
    {
        (void)fn_CipForwardClose(&transport->session, &transport->connection);
        if (transport->udp_fd >= 0) (void)close(transport->udp_fd);
        transport->udp_fd = -1;
        return ead_result;
    }
    transport->address_sequence = 0U;
    transport->transport_sequence = 0U;
    transport->input_sequence = 0U;
    transport->have_address_sequence = false;
    transport->open_ns = fn_NowNs();
    transport->last_tx_ns = 0U;
    transport->last_rx_ns = 0U;
    transport->last_keepalive_ns = transport->open_ns;
    transport->io_open = true;
    ead_result = fn_SendIo(transport, error, error_size);
    if ((ead_result != EAD_OK) && (ead_result != EAD_ERR_TIMEOUT))
    {
        (void)fn_EadEipCloseIo(transport, NULL, 0U);
        return ead_result;
    }
    return EAD_OK;
}

ead_result_t fn_EadEipCloseIo(ead_eip_transport_t *transport,
                              char *error,
                              size_t error_size)
{
    eip_result_t result = EIP_OK;
    if (transport == NULL) return EAD_ERR_ARGUMENT;
    if (!transport->io_open) return EAD_OK;
    if (transport->session_open)
        result = fn_CipForwardClose(&transport->session, &transport->connection);
    if (transport->udp_fd >= 0) (void)close(transport->udp_fd);
    transport->udp_fd = -1;
    transport->io_open = false;
    if (result != EIP_OK)
    {
        fn_SetError(error, error_size, "Forward Close failed (%d)", result);
        return fn_MapResult(result);
    }
    return EAD_OK;
}

ead_result_t fn_EadEipSetOutput(ead_eip_transport_t *transport,
                                const uint8_t *data,
                                size_t size)
{
    if ((transport == NULL) || (data == NULL) || (size != transport->output_size))
        return EAD_ERR_ARGUMENT;
    (void)memcpy(transport->output, data, size);
    return EAD_OK;
}

ead_result_t fn_EadEipPoll(ead_eip_transport_t *transport,
                           uint32_t timeout_ms,
                           char *error,
                           size_t error_size)
{
    uint64_t start_ns;
    uint64_t deadline_ns;
    uint64_t tx_period_ns;
    uint64_t watchdog_ns;
    if ((transport == NULL) || !transport->io_open) return EAD_ERR_STATE;
    start_ns = fn_NowNs();
    deadline_ns = start_ns + ((uint64_t)timeout_ms * UINT64_C(1000000));
    tx_period_ns = (uint64_t)((transport->connection.o2t_actual_packet_interval_us != 0U) ?
                              transport->connection.o2t_actual_packet_interval_us :
                              transport->config.o2t_rpi_us) * UINT64_C(1000);
    watchdog_ns = (uint64_t)((transport->connection.t2o_actual_packet_interval_us != 0U) ?
                             transport->connection.t2o_actual_packet_interval_us :
                             transport->config.t2o_rpi_us) *
                  (uint64_t)(4U << transport->config.timeout_multiplier) * UINT64_C(1000);
    for (;;)
    {
        uint64_t now_ns = fn_NowNs();
        int wait_ms;
        struct pollfd descriptor;
        if ((transport->last_tx_ns == 0U) || ((now_ns - transport->last_tx_ns) >= tx_period_ns))
        {
            ead_result_t send_result = fn_SendIo(transport, error, error_size);
            if ((send_result != EAD_OK) && (send_result != EAD_ERR_TIMEOUT)) return send_result;
        }
        if ((now_ns - transport->last_keepalive_ns) >= UINT64_C(10000000000))
        {
            if (fn_EipSessionKeepAlive(&transport->session) != EIP_OK)
                EAD_LOGW("EtherNet/IP TCP keepalive NOP failed");
            transport->last_keepalive_ns = now_ns;
        }
        if ((transport->last_rx_ns == 0U) && ((now_ns - transport->open_ns) > watchdog_ns))
        {
            fn_SetError(error, error_size, "no T-to-O packets before watchdog timeout");
            return EAD_ERR_TIMEOUT;
        }
        if ((transport->last_rx_ns != 0U) && ((now_ns - transport->last_rx_ns) > watchdog_ns))
        {
            fn_SetError(error, error_size, "T-to-O cyclic I/O watchdog timeout");
            return EAD_ERR_TIMEOUT;
        }
        if (now_ns >= deadline_ns) return EAD_OK;
        {
            uint64_t next_tx_ns = transport->last_tx_ns + tx_period_ns;
            uint64_t wake_ns = (next_tx_ns < deadline_ns) ? next_tx_ns : deadline_ns;
            uint64_t remaining_ns = (wake_ns > now_ns) ? (wake_ns - now_ns) : 0U;
            wait_ms = (int)((remaining_ns + UINT64_C(999999)) / UINT64_C(1000000));
        }
        descriptor.fd = transport->udp_fd;
        descriptor.events = POLLIN;
        descriptor.revents = 0;
        {
            int ready = poll(&descriptor, 1, wait_ms);
            if (ready < 0)
            {
                if (errno == EINTR) continue;
                fn_SetError(error, error_size, "UDP poll: %s", strerror(errno));
                return EAD_ERR_IO;
            }
            if ((ready > 0) && ((descriptor.revents & POLLIN) != 0))
            {
                for (unsigned int drained = 0U; drained < 32U; ++drained)
                {
                    uint8_t packet[EIP_MAX_PACKET_SIZE];
                    ssize_t length = recv(transport->udp_fd, packet, sizeof(packet), 0);
                    if (length > 0)
                    {
                        ead_result_t parse_result = fn_ParseIo(transport, packet, (size_t)length,
                                                              error, error_size);
                        if (parse_result != EAD_OK)
                            EAD_LOGD("Rejected T-to-O packet: %s", fn_EadResultText(parse_result));
                    }
                    else if ((length < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) break;
                    else if ((length < 0) && (errno == EINTR)) continue;
                    else if (length == 0) break;
                    else
                    {
                        fn_SetError(error, error_size, "UDP receive: %s", strerror(errno));
                        return EAD_ERR_IO;
                    }
                }
            }
            if ((ready > 0) && ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0))
            {
                fn_SetError(error, error_size, "UDP socket poll error 0x%X", descriptor.revents);
                return EAD_ERR_IO;
            }
        }
    }
}

bool fn_EadEipIsIoOpen(const ead_eip_transport_t *transport)
{
    return (transport != NULL) && transport->io_open;
}

void fn_EadEipDisconnect(ead_eip_transport_t *transport)
{
    if (transport == NULL) return;
    if (transport->io_open) (void)fn_EadEipCloseIo(transport, NULL, 0U);
    if (transport->udp_fd >= 0) (void)close(transport->udp_fd);
    transport->udp_fd = -1;
    if (transport->session_open) fn_EipSessionClose(&transport->session);
    transport->session_open = false;
    transport->session.tcp_fd = -1;
}
