/**
 * @file cip_connection.c
 * @brief Forward Open/Forward Close and Class 1 connection parameter handling.
 * @author Prathamesh Chipkar
 */
#define _GNU_SOURCE

#include "cip_connection.h"

#include "cip.h"
#include "eip_endian.h"
#include "eip_session.h"
#include "logger.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/random.h>
#include <time.h>
#include <unistd.h>

static uint32_t fn_RandomU32(void)
{
    uint32_t value = 0U;
    ssize_t result = getrandom(&value, sizeof(value), GRND_NONBLOCK);
    if (result != (ssize_t)sizeof(value))
    {
        struct timespec now;
        (void)clock_gettime(CLOCK_REALTIME, &now);
        value = (uint32_t)now.tv_nsec ^ (uint32_t)now.tv_sec ^ (uint32_t)getpid();
    }
    if (value == 0U) value = 1U;
    return value;
}

static eip_result_t fn_Append8Or16(uint8_t segment8,
                                  uint8_t segment16,
                                  uint16_t value,
                                  uint8_t *path,
                                  size_t capacity,
                                  size_t *cursor)
{
    if (value <= 0xFFU)
    {
        if ((*cursor + 2U) > capacity) return EIP_ERR_OVERFLOW;
        path[(*cursor)++] = segment8;
        path[(*cursor)++] = (uint8_t)value;
    }
    else
    {
        if ((*cursor + 4U) > capacity) return EIP_ERR_OVERFLOW;
        path[(*cursor)++] = segment16;
        path[(*cursor)++] = 0U;
        fn_WriteLe16(&path[*cursor], value);
        *cursor += 2U;
    }
    return EIP_OK;
}

static eip_result_t fn_BuildConnectionPath(const app_config_t *config,
                                           uint8_t *path,
                                           size_t capacity,
                                           size_t *path_length)
{
    size_t cursor = 0U;
    eip_result_t result;

    result = fn_Append8Or16(0x20U, 0x21U, CIP_CLASS_ASSEMBLY,
                            path, capacity, &cursor);
    if (result != EIP_OK) return result;
    result = fn_Append8Or16(0x24U, 0x25U, config->config_assembly,
                            path, capacity, &cursor);
    if (result != EIP_OK) return result;
    result = fn_Append8Or16(0x2CU, 0x2DU, config->output_assembly,
                            path, capacity, &cursor);
    if (result != EIP_OK) return result;
    result = fn_Append8Or16(0x2CU, 0x2DU, config->input_assembly,
                            path, capacity, &cursor);
    if (result != EIP_OK) return result;

    if ((cursor & 1U) != 0U)
    {
        if (cursor >= capacity) return EIP_ERR_OVERFLOW;
        path[cursor++] = 0U;
    }

    *path_length = cursor;
    return EIP_OK;
}

static void fn_ParseSockaddrItem(const eip_cpf_item_t *item,
                                 const char *fallback_ip,
                                 uint16_t fallback_port,
                                 char *ip_out,
                                 size_t ip_out_size,
                                 uint16_t *port_out)
{
    struct in_addr address;

    (void)snprintf(ip_out, ip_out_size, "%s", fallback_ip);
    *port_out = fallback_port;

    if ((item == NULL) || (item->length < 8U))
    {
        return;
    }

    *port_out = fn_ReadBe16(&item->data[2]);
    (void)memcpy(&address.s_addr, &item->data[4], sizeof(address.s_addr));
    if (address.s_addr != htonl(INADDR_ANY))
    {
        const char *text = inet_ntop(AF_INET, &address, ip_out, (socklen_t)ip_out_size);
        if (text == NULL)
        {
            (void)snprintf(ip_out, ip_out_size, "%s", fallback_ip);
        }
    }
    if (*port_out == 0U) *port_out = fallback_port;
}

eip_result_t fn_CipForwardOpen(eip_session_t *session,
                               const app_config_t *config,
                               uint16_t o2t_application_size,
                               uint16_t t2o_application_size,
                               eip_io_connection_t *connection)
{
    uint8_t request_data[256];
    uint8_t cip_request[512];
    uint8_t cip_response[EIP_MAX_CIP_RESPONSE_SIZE];
    eip_cpf_item_t additional_items[EIP_MAX_CPF_ITEMS];
    size_t request_data_length = 0U;
    size_t cip_request_length = 0U;
    size_t cip_response_length = 0U;
    size_t additional_item_count = 0U;
    uint16_t connection_serial;
    uint32_t originator_serial;
    uint32_t t2o_connection_id;
    uint16_t o2t_wire_size;
    uint16_t t2o_wire_size;
    uint16_t o2t_ncp;
    uint16_t t2o_ncp;
    uint8_t path[EIP_MAX_PATH_SIZE];
    size_t path_length = 0U;
    cip_response_view_t parsed;
    eip_result_t result;

    if ((session == NULL) || (config == NULL) || (connection == NULL))
    {
        return EIP_ERR_ARGUMENT;
    }

    o2t_wire_size = (uint16_t)(o2t_application_size + 2U +
                               (config->o2t_run_idle_header ? 4U : 0U));
    t2o_wire_size = (uint16_t)(t2o_application_size + 2U +
                               (config->t2o_run_idle_header ? 4U : 0U));

    if ((o2t_wire_size > 511U) || (t2o_wire_size > 511U))
    {
        LOG_ERROR("Standard Forward Open supports at most 511 wire bytes. "
                  "Calculated O->T=%u, T->O=%u. Large Forward Open is not enabled in this build.",
                  o2t_wire_size, t2o_wire_size);
        return EIP_ERR_NOT_SUPPORTED;
    }

    result = fn_BuildConnectionPath(config, path, sizeof(path), &path_length);
    if (result != EIP_OK) return result;

    connection_serial = (uint16_t)(fn_RandomU32() & 0xFFFFU);
    if (connection_serial == 0U) connection_serial = 1U;
    originator_serial = (config->originator_serial != 0U) ?
                        config->originator_serial : fn_RandomU32();
    t2o_connection_id = fn_RandomU32();

    o2t_ncp = (uint16_t)(CIP_NCP_CONNECTION_P2P |
                         CIP_NCP_PRIORITY_SCHEDULED |
                         o2t_wire_size);
    t2o_ncp = (uint16_t)(CIP_NCP_CONNECTION_P2P |
                         CIP_NCP_PRIORITY_SCHEDULED |
                         t2o_wire_size);

    request_data[request_data_length++] = 0x0AU; /* Priority / Time Tick */
    request_data[request_data_length++] = 0x0EU; /* Timeout Ticks */
    fn_WriteLe32(&request_data[request_data_length], 0U); request_data_length += 4U;
    fn_WriteLe32(&request_data[request_data_length], t2o_connection_id); request_data_length += 4U;
    fn_WriteLe16(&request_data[request_data_length], connection_serial); request_data_length += 2U;
    fn_WriteLe16(&request_data[request_data_length], config->originator_vendor_id); request_data_length += 2U;
    fn_WriteLe32(&request_data[request_data_length], originator_serial); request_data_length += 4U;
    request_data[request_data_length++] = config->timeout_multiplier;
    request_data[request_data_length++] = 0U;
    request_data[request_data_length++] = 0U;
    request_data[request_data_length++] = 0U;
    fn_WriteLe32(&request_data[request_data_length], config->o2t_rpi_us); request_data_length += 4U;
    fn_WriteLe16(&request_data[request_data_length], o2t_ncp); request_data_length += 2U;
    fn_WriteLe32(&request_data[request_data_length], config->t2o_rpi_us); request_data_length += 4U;
    fn_WriteLe16(&request_data[request_data_length], t2o_ncp); request_data_length += 2U;
    request_data[request_data_length++] = (uint8_t)(CIP_TRIGGER_CYCLIC | CIP_TRANSPORT_CLASS_1);
    request_data[request_data_length++] = (uint8_t)(path_length / 2U);
    (void)memcpy(&request_data[request_data_length], path, path_length);
    request_data_length += path_length;

    result = fn_CipBuildRequest(CIP_SERVICE_FORWARD_OPEN,
                                CIP_CLASS_CONNECTION_MANAGER, 1U, 0U,
                                request_data, request_data_length,
                                cip_request, sizeof(cip_request), &cip_request_length);
    if (result != EIP_OK) return result;

    fn_LogHex(LOG_LEVEL_DEBUG, "Forward Open CIP request", cip_request, cip_request_length);

    result = fn_EipSendRRData(session, cip_request, cip_request_length,
                              cip_response, sizeof(cip_response), &cip_response_length,
                              additional_items, EIP_MAX_CPF_ITEMS, &additional_item_count);
    if (result != EIP_OK) return result;

    result = fn_CipParseResponse(cip_response, cip_response_length, &parsed);
    if (result != EIP_OK) return result;

    if (parsed.general_status != CIP_GENERAL_STATUS_SUCCESS)
    {
        LOG_ERROR("Forward Open failed: status 0x%02X (%s)",
                  parsed.general_status, fn_CipStatusText(parsed.general_status));
        for (size_t index = 0U; index < parsed.additional_status_count; ++index)
        {
            LOG_ERROR("  Additional status[%zu] = 0x%04X", index,
                      parsed.additional_status[index]);
        }
        return EIP_ERR_REMOTE;
    }

    if (parsed.data_length < 26U)
    {
        LOG_ERROR("Forward Open response too short: %zu bytes", parsed.data_length);
        return EIP_ERR_PROTOCOL;
    }
    if ((fn_ReadLe32(&parsed.data[0]) == 0U) ||
        (fn_ReadLe32(&parsed.data[4]) == 0U) ||
        (fn_ReadLe16(&parsed.data[8]) != connection_serial) ||
        (fn_ReadLe16(&parsed.data[10]) != config->originator_vendor_id) ||
        (fn_ReadLe32(&parsed.data[12]) != originator_serial))
    {
        LOG_ERROR("Forward Open response does not match the requested connection identity");
        return EIP_ERR_PROTOCOL;
    }

    (void)memset(connection, 0, sizeof(*connection));
    connection->o2t_connection_id = fn_ReadLe32(&parsed.data[0]);
    connection->t2o_connection_id = fn_ReadLe32(&parsed.data[4]);
    connection->connection_serial = fn_ReadLe16(&parsed.data[8]);
    connection->originator_vendor_id = fn_ReadLe16(&parsed.data[10]);
    connection->originator_serial = fn_ReadLe32(&parsed.data[12]);
    connection->o2t_actual_packet_interval_us = fn_ReadLe32(&parsed.data[16]);
    connection->t2o_actual_packet_interval_us = fn_ReadLe32(&parsed.data[20]);
    connection->o2t_application_size = o2t_application_size;
    connection->t2o_application_size = t2o_application_size;
    connection->o2t_wire_size = o2t_wire_size;
    connection->t2o_wire_size = t2o_wire_size;
    connection->o2t_run_idle_header = config->o2t_run_idle_header;
    connection->t2o_run_idle_header = config->t2o_run_idle_header;
    connection->timeout_multiplier = config->timeout_multiplier;
    connection->transport_type_trigger = (uint8_t)(CIP_TRIGGER_CYCLIC | CIP_TRANSPORT_CLASS_1);
    connection->connection_path_length = path_length;
    (void)memcpy(connection->connection_path, path, path_length);
    (void)snprintf(connection->o2t_ip, sizeof(connection->o2t_ip), "%s", config->target_ip);
    connection->o2t_port = config->target_udp_port;
    (void)snprintf(connection->t2o_ip, sizeof(connection->t2o_ip), "%s", config->local_ip);
    connection->t2o_port = config->local_udp_port;

    for (size_t index = 0U; index < additional_item_count; ++index)
    {
        if (additional_items[index].type_id == EIP_CPF_O2T_SOCKADDR)
        {
            fn_ParseSockaddrItem(&additional_items[index], config->target_ip,
                                 config->target_udp_port,
                                 connection->o2t_ip, sizeof(connection->o2t_ip),
                                 &connection->o2t_port);
        }
        else if (additional_items[index].type_id == EIP_CPF_T2O_SOCKADDR)
        {
            fn_ParseSockaddrItem(&additional_items[index], config->local_ip,
                                 config->local_udp_port,
                                 connection->t2o_ip, sizeof(connection->t2o_ip),
                                 &connection->t2o_port);
        }
    }

    LOG_INFO("Forward Open accepted: O->T ID=0x%08X T->O ID=0x%08X serial=%u",
             connection->o2t_connection_id, connection->t2o_connection_id,
             connection->connection_serial);
    LOG_INFO("Actual packet intervals: O->T=%u us T->O=%u us",
             connection->o2t_actual_packet_interval_us,
             connection->t2o_actual_packet_interval_us);
    LOG_INFO("UDP: send to %s:%u, receive on %s:%u",
             connection->o2t_ip, connection->o2t_port,
             connection->t2o_ip, connection->t2o_port);

    return EIP_OK;
}

eip_result_t fn_CipForwardClose(eip_session_t *session,
                                const eip_io_connection_t *connection)
{
    uint8_t request_data[128];
    uint8_t cip_request[256];
    uint8_t cip_response[512];
    size_t request_data_length = 0U;
    size_t cip_request_length = 0U;
    size_t cip_response_length = 0U;
    cip_response_view_t parsed;
    eip_result_t result;

    if ((session == NULL) || (connection == NULL))
    {
        return EIP_ERR_ARGUMENT;
    }

    request_data[request_data_length++] = 0x0AU;
    request_data[request_data_length++] = 0x0EU;
    fn_WriteLe16(&request_data[request_data_length], connection->connection_serial);
    request_data_length += 2U;
    fn_WriteLe16(&request_data[request_data_length], connection->originator_vendor_id);
    request_data_length += 2U;
    fn_WriteLe32(&request_data[request_data_length], connection->originator_serial);
    request_data_length += 4U;
    request_data[request_data_length++] = (uint8_t)(connection->connection_path_length / 2U);
    request_data[request_data_length++] = 0U;
    (void)memcpy(&request_data[request_data_length], connection->connection_path,
                 connection->connection_path_length);
    request_data_length += connection->connection_path_length;

    result = fn_CipBuildRequest(CIP_SERVICE_FORWARD_CLOSE,
                                CIP_CLASS_CONNECTION_MANAGER, 1U, 0U,
                                request_data, request_data_length,
                                cip_request, sizeof(cip_request), &cip_request_length);
    if (result != EIP_OK) return result;

    result = fn_EipSendRRData(session, cip_request, cip_request_length,
                              cip_response, sizeof(cip_response), &cip_response_length,
                              NULL, 0U, NULL);
    if (result != EIP_OK) return result;

    result = fn_CipParseResponse(cip_response, cip_response_length, &parsed);
    if (result != EIP_OK) return result;
    if (parsed.general_status != CIP_GENERAL_STATUS_SUCCESS)
    {
        LOG_WARN("Forward Close returned status 0x%02X (%s)",
                 parsed.general_status, fn_CipStatusText(parsed.general_status));
        return EIP_ERR_REMOTE;
    }

    LOG_INFO("Forward Close accepted");
    return EIP_OK;
}
