/**
 * @file eip_session.c
 * @brief EtherNet/IP TCP encapsulation session registration and teardown.
 * @author Prathamesh Chipkar
 */
#define _POSIX_C_SOURCE 200809L

#include "eip_session.h"

#include "eip_endian.h"
#include "logger.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static int fn_WaitSocket(int fd, short events, uint64_t deadline_ms)
{
    for (;;)
    {
        struct timespec now;
        (void)clock_gettime(CLOCK_MONOTONIC, &now);
        const uint64_t ms = (uint64_t)now.tv_sec * 1000U + (uint64_t)now.tv_nsec / 1000000U;
        if (ms >= deadline_ms) return 0;
        struct pollfd descriptor = { .fd = fd, .events = events, .revents = 0 };
        int ready = poll(&descriptor, 1U, (int)(deadline_ms - ms));
        if (ready < 0 && errno == EINTR) continue;
        return ready;
    }
}
static uint64_t fn_Deadline(int fd, int option)
{
    struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };
    socklen_t size = sizeof(timeout);
    struct timespec now;
    (void)getsockopt(fd, SOL_SOCKET, option, &timeout, &size);
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t duration = (uint64_t)timeout.tv_sec * 1000U + (uint64_t)timeout.tv_usec / 1000U;
    if (duration == 0U) duration = 3000U;
    return (uint64_t)now.tv_sec * 1000U + (uint64_t)now.tv_nsec / 1000000U + duration;
}

static eip_result_t fn_SendAll(int fd, const uint8_t *data, size_t length)
{
    size_t sent = 0U;
    const uint64_t deadline = fn_Deadline(fd, SO_SNDTIMEO);

    while (sent < length)
    {
        if (fn_WaitSocket(fd, POLLOUT, deadline) <= 0) return EIP_ERR_TIMEOUT;
        ssize_t result = send(fd, data + sent, length - sent, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (result > 0)
        {
            sent += (size_t)result;
        }
        else if ((result < 0) && (errno == EINTR))
        {
            continue;
        }
        else if ((result < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
        {
            LOG_WARN("EtherNet/IP TCP send timed out after %zu of %zu bytes",
                     sent, length);
            return EIP_ERR_TIMEOUT;
        }
        else
        {
            LOG_WARN("EtherNet/IP TCP send failed after %zu of %zu bytes: %s",
                     sent, length, strerror(errno));
            return EIP_ERR_SOCKET;
        }
    }
    return EIP_OK;
}

static eip_result_t fn_RecvAll(int fd, uint8_t *data, size_t length)
{
    size_t received = 0U;
    const uint64_t deadline = fn_Deadline(fd, SO_RCVTIMEO);

    while (received < length)
    {
        if (fn_WaitSocket(fd, POLLIN, deadline) <= 0) return EIP_ERR_TIMEOUT;
        ssize_t result = recv(fd, data + received, length - received, MSG_DONTWAIT);
        if (result > 0)
        {
            received += (size_t)result;
        }
        else if (result == 0)
        {
            LOG_WARN("EtherNet/IP target closed the TCP session after %zu of "
                     "%zu expected bytes", received, length);
            return EIP_ERR_SOCKET;
        }
        else if (errno == EINTR)
        {
            continue;
        }
        else if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
        {
            LOG_WARN("EtherNet/IP TCP receive timed out after %zu of %zu bytes",
                     received, length);
            return EIP_ERR_TIMEOUT;
        }
        else
        {
            LOG_WARN("EtherNet/IP TCP receive failed after %zu of %zu bytes: %s",
                     received, length, strerror(errno));
            return EIP_ERR_SOCKET;
        }
    }
    return EIP_OK;
}

static eip_result_t fn_SendEncapsulation(eip_session_t *session,
                                         uint16_t command,
                                         const uint8_t *payload,
                                         size_t payload_length,
                                         bool expect_response,
                                         uint8_t *response_payload,
                                         size_t response_capacity,
                                         size_t *response_length,
                                         uint16_t *response_command,
                                         uint32_t *response_session_handle)
{
    uint8_t packet[EIP_MAX_PACKET_SIZE];
    uint8_t response_header[EIP_ENCAP_HEADER_SIZE];
    uint16_t encoded_length;
    uint32_t status;
    eip_result_t result;

    if ((session == NULL) || (session->tcp_fd < 0) ||
        (payload_length > (EIP_MAX_PACKET_SIZE - EIP_ENCAP_HEADER_SIZE)) ||
        ((payload_length > 0U) && (payload == NULL)))
    {
        return EIP_ERR_ARGUMENT;
    }

    encoded_length = (uint16_t)payload_length;
    (void)memset(packet, 0, EIP_ENCAP_HEADER_SIZE);
    fn_WriteLe16(&packet[0], command);
    fn_WriteLe16(&packet[2], encoded_length);
    fn_WriteLe32(&packet[4], session->session_handle);
    fn_WriteLe32(&packet[8], 0U);
    ++session->sender_context_counter;
    fn_WriteLe64(&packet[12], session->sender_context_counter);
    fn_WriteLe32(&packet[20], 0U);
    if (payload_length > 0U)
    {
        (void)memcpy(&packet[EIP_ENCAP_HEADER_SIZE], payload, payload_length);
    }

    fn_LogHex(LOG_LEVEL_TRACE, "EtherNet/IP encapsulation request", packet,
              EIP_ENCAP_HEADER_SIZE + payload_length);

    result = fn_SendAll(session->tcp_fd, packet, EIP_ENCAP_HEADER_SIZE + payload_length);
    if (result != EIP_OK)
    {
        return result;
    }

    if (!expect_response)
    {
        if (response_length != NULL) *response_length = 0U;
        return EIP_OK;
    }

    result = fn_RecvAll(session->tcp_fd, response_header, sizeof(response_header));
    if (result != EIP_OK)
    {
        return result;
    }

    if (fn_ReadLe16(response_header) != command ||
        memcmp(&response_header[12], &packet[12], 8U) != 0 ||
        fn_ReadLe32(&response_header[20]) != 0U ||
        (command != EIP_ENCAP_CMD_REGISTER_SESSION &&
         fn_ReadLe32(&response_header[4]) != session->session_handle))
    {
        LOG_ERROR("EtherNet/IP response command, session or sender context mismatch");
        return EIP_ERR_PROTOCOL;
    }
    encoded_length = fn_ReadLe16(&response_header[2]);
    status = fn_ReadLe32(&response_header[8]);
    if (response_command != NULL)
    {
        *response_command = fn_ReadLe16(&response_header[0]);
    }
    if (response_session_handle != NULL)
    {
        *response_session_handle = fn_ReadLe32(&response_header[4]);
    }

    if ((size_t)encoded_length > response_capacity)
    {
        uint8_t discard[256];
        size_t remaining = encoded_length;
        while (remaining > 0U)
        {
            size_t chunk = (remaining > sizeof(discard)) ? sizeof(discard) : remaining;
            result = fn_RecvAll(session->tcp_fd, discard, chunk);
            if (result != EIP_OK) return result;
            remaining -= chunk;
        }
        return EIP_ERR_OVERFLOW;
    }

    if (encoded_length > 0U)
    {
        if (response_payload == NULL)
        {
            return EIP_ERR_ARGUMENT;
        }
        result = fn_RecvAll(session->tcp_fd, response_payload, encoded_length);
        if (result != EIP_OK)
        {
            return result;
        }
    }

    if (response_length != NULL)
    {
        *response_length = encoded_length;
    }

    fn_LogHex(LOG_LEVEL_TRACE, "EtherNet/IP encapsulation response header",
              response_header, sizeof(response_header));
    if (encoded_length > 0U)
    {
        fn_LogHex(LOG_LEVEL_TRACE, "EtherNet/IP encapsulation response payload",
                  response_payload, encoded_length);
    }

    if (status != 0U)
    {
        LOG_ERROR("EtherNet/IP encapsulation status 0x%08X", status);
        return EIP_ERR_REMOTE;
    }

    return EIP_OK;
}

static eip_result_t fn_ConnectTcp(const char *target_ip, uint16_t port, int timeout_ms, int *fd_out)
{
    int fd;
    struct sockaddr_in remote;
    struct timeval timeout;
    int flag = 1;

    if ((target_ip == NULL) || (fd_out == NULL))
    {
        return EIP_ERR_ARGUMENT;
    }

    fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
    if (fd < 0)
    {
        LOG_ERROR("socket(TCP) failed: %s", strerror(errno));
        return EIP_ERR_SOCKET;
    }

    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
    (void)setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    (void)memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_port = htons(port);
    if (inet_pton(AF_INET, target_ip, &remote.sin_addr) != 1)
    {
        LOG_ERROR("Invalid IPv4 address: %s", target_ip);
        (void)close(fd);
        return EIP_ERR_ARGUMENT;
    }

    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0)
    {
        (void)close(fd);
        return EIP_ERR_SOCKET;
    }
    int connected = connect(fd, (const struct sockaddr *)&remote, sizeof(remote));
    if (connected != 0 && errno == EINPROGRESS)
    {
        struct pollfd pending = { .fd = fd, .events = POLLOUT, .revents = 0 };
        connected = poll(&pending, 1U, timeout_ms);
        int socket_error = 0;
        socklen_t error_size = sizeof(socket_error);
        if (connected <= 0 || getsockopt(fd, SOL_SOCKET, SO_ERROR,
                                         &socket_error, &error_size) != 0 || socket_error != 0)
        {
            LOG_WARN("TCP connection to %s:%u failed or timed out", target_ip, port);
            (void)close(fd);
            return connected == 0 ? EIP_ERR_TIMEOUT : EIP_ERR_SOCKET;
        }
    }
    else if (connected != 0)
    {
        (void)close(fd);
        return EIP_ERR_SOCKET;
    }
    if (fcntl(fd, F_SETFL, flags) != 0)
    {
        (void)close(fd);
        return EIP_ERR_SOCKET;
    }

    *fd_out = fd;
    return EIP_OK;
}

eip_result_t fn_EipSessionOpen(eip_session_t *session,
                               const char *target_ip,
                               uint16_t target_tcp_port,
                               int timeout_ms)
{
    uint8_t request[4];
    uint8_t response[32];
    size_t response_length = 0U;
    uint16_t response_command = 0U;
    uint32_t response_session = 0U;
    eip_result_t result;

    if ((session == NULL) || (target_ip == NULL))
    {
        return EIP_ERR_ARGUMENT;
    }

    (void)memset(session, 0, sizeof(*session));
    session->tcp_fd = -1;
    session->target_tcp_port = target_tcp_port;
    session->tcp_timeout_ms = timeout_ms;
    (void)snprintf(session->target_ip, sizeof(session->target_ip), "%s", target_ip);

    result = fn_ConnectTcp(target_ip, target_tcp_port, timeout_ms, &session->tcp_fd);
    if (result != EIP_OK)
    {
        return result;
    }

    fn_WriteLe16(&request[0], 1U);
    fn_WriteLe16(&request[2], 0U);

    result = fn_SendEncapsulation(session, EIP_ENCAP_CMD_REGISTER_SESSION,
                                  request, sizeof(request), true,
                                  response, sizeof(response), &response_length,
                                  &response_command, &response_session);
    if ((result != EIP_OK) || (response_command != EIP_ENCAP_CMD_REGISTER_SESSION) ||
        (response_length < 4U) || (response_session == 0U))
    {
        LOG_ERROR("Register Session failed");
        fn_EipSessionClose(session);
        return (result == EIP_OK) ? EIP_ERR_PROTOCOL : result;
    }

    session->session_handle = response_session;
    LOG_INFO("Registered EtherNet/IP session 0x%08X", session->session_handle);
    return EIP_OK;
}

void fn_EipSessionClose(eip_session_t *session)
{
    if (session == NULL)
    {
        return;
    }

    if (session->tcp_fd >= 0)
    {
        if (session->session_handle != 0U)
        {
            (void)fn_SendEncapsulation(session, EIP_ENCAP_CMD_UNREGISTER_SESSION,
                                       NULL, 0U, false, NULL, 0U, NULL, NULL, NULL);
        }
        (void)shutdown(session->tcp_fd, SHUT_RDWR);
        (void)close(session->tcp_fd);
    }

    session->tcp_fd = -1;
    session->session_handle = 0U;
}

eip_result_t fn_EipSessionKeepAlive(eip_session_t *session)
{
    /* Encapsulation NOP has no response by design. TCP keepalive is also enabled. */
    return fn_SendEncapsulation(session, EIP_ENCAP_CMD_NOP,
                                NULL, 0U, false,
                                NULL, 0U, NULL, NULL, NULL);
}

eip_result_t fn_EipSendRRData(eip_session_t *session,
                              const uint8_t *cip_request,
                              size_t cip_request_length,
                              uint8_t *response_buffer,
                              size_t response_buffer_capacity,
                              size_t *response_length,
                              eip_cpf_item_t *additional_items,
                              size_t additional_items_capacity,
                              size_t *additional_items_count)
{
    uint8_t payload[EIP_MAX_PACKET_SIZE];
    uint8_t response_payload[EIP_MAX_PACKET_SIZE];
    size_t payload_length;
    size_t received_length = 0U;
    uint16_t response_command = 0U;
    uint16_t item_count;
    size_t cursor;
    size_t found_additional = 0U;
    bool found_cip = false;
    eip_result_t result;

    if ((session == NULL) || (cip_request == NULL) || (cip_request_length > 65535U) ||
        (response_buffer == NULL) || (response_length == NULL))
    {
        return EIP_ERR_ARGUMENT;
    }

    payload_length = 16U + cip_request_length;
    if (payload_length > sizeof(payload))
    {
        return EIP_ERR_OVERFLOW;
    }

    fn_WriteLe32(&payload[0], 0U);     /* Interface handle */
    fn_WriteLe16(&payload[4], 0U);     /* Timeout */
    fn_WriteLe16(&payload[6], 2U);     /* Item count */
    fn_WriteLe16(&payload[8], EIP_CPF_NULL_ADDRESS);
    fn_WriteLe16(&payload[10], 0U);
    fn_WriteLe16(&payload[12], EIP_CPF_UNCONNECTED_DATA);
    fn_WriteLe16(&payload[14], (uint16_t)cip_request_length);
    (void)memcpy(&payload[16], cip_request, cip_request_length);

    result = fn_SendEncapsulation(session, EIP_ENCAP_CMD_SEND_RR_DATA,
                                  payload, payload_length, true,
                                  response_payload, sizeof(response_payload),
                                  &received_length, &response_command, NULL);
    if (result != EIP_OK)
    {
        return result;
    }
    if ((response_command != EIP_ENCAP_CMD_SEND_RR_DATA) || (received_length < 8U))
    {
        return EIP_ERR_PROTOCOL;
    }

    item_count = fn_ReadLe16(&response_payload[6]);
    cursor = 8U;
    *response_length = 0U;
    if (additional_items_count != NULL) *additional_items_count = 0U;

    for (uint16_t item_index = 0U; item_index < item_count; ++item_index)
    {
        uint16_t type_id;
        uint16_t item_length;
        if ((cursor + 4U) > received_length)
        {
            return EIP_ERR_PROTOCOL;
        }
        type_id = fn_ReadLe16(&response_payload[cursor]);
        item_length = fn_ReadLe16(&response_payload[cursor + 2U]);
        cursor += 4U;
        if ((cursor + item_length) > received_length)
        {
            return EIP_ERR_PROTOCOL;
        }

        if ((type_id == EIP_CPF_UNCONNECTED_DATA) || (type_id == EIP_CPF_CONNECTED_DATA))
        {
            if ((size_t)item_length > response_buffer_capacity)
            {
                return EIP_ERR_OVERFLOW;
            }
            (void)memcpy(response_buffer, &response_payload[cursor], item_length);
            *response_length = item_length;
            found_cip = true;
        }
        else if ((type_id != EIP_CPF_NULL_ADDRESS) && (additional_items != NULL) &&
                 (found_additional < additional_items_capacity))
        {
            size_t copy_length = item_length;
            if (copy_length > sizeof(additional_items[found_additional].data))
            {
                copy_length = sizeof(additional_items[found_additional].data);
            }
            additional_items[found_additional].type_id = type_id;
            additional_items[found_additional].length = (uint16_t)copy_length;
            (void)memcpy(additional_items[found_additional].data,
                         &response_payload[cursor], copy_length);
            ++found_additional;
        }
        cursor += item_length;
    }

    if (additional_items_count != NULL)
    {
        *additional_items_count = found_additional;
    }

    return found_cip ? EIP_OK : EIP_ERR_PROTOCOL;
}
