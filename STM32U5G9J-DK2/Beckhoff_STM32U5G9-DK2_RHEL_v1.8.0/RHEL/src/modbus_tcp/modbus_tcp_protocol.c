/**
 * @file modbus_tcp_protocol.c
 * @brief Modbus TCP framing, validation, unit routing and register mapping.
 * @author Prathamesh Chipkar
 */
#include "modbus_tcp/modbus_tcp_protocol.h"
#include "modbus_tcp/modbus_coils.h"
#include "modbus_tcp/modbus_discrete_inputs.h"
#include "modbus_tcp/modbus_holding_registers.h"
#include "modbus_tcp/modbus_input_registers.h"

#include <string.h>

/** Maximum Modbus read-register quantity. */
#define MODB_MAX_READ_REGISTERS (125U)
/** Maximum Modbus write-register quantity. */
#define MODB_MAX_WRITE_REGISTERS (123U)
/** Maximum Modbus read-bit quantity. */
#define MODB_MAX_READ_BITS (2000U)
/** Maximum Modbus write-bit quantity. */
#define MODB_MAX_WRITE_BITS (1968U)

/** @brief Read a big-endian 16-bit field. */
static uint16_t fn_GetBe16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8U) | (uint16_t)data[1]);
}

/** @brief Write a big-endian 16-bit field. */
static void fn_PutBe16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8U);
    data[1] = (uint8_t)(value & 0xFFU);
}

E_GW_RESULT fn_MODB_ProtocolInitialize(S_MODB_CONTEXT *context,
                                       S_GW_ROUTE_MANAGER *route_manager,
                                       const S_GW_CONFIG *config)
{
    E_GW_RESULT result;
    if ((context == NULL) || (route_manager == NULL) || (config == NULL))
    {
        return GW_ERR_ARGUMENT;
    }
    result = fn_MODB_DataModelInitialize(&context->st_DataModel, route_manager);
    if (result != GW_OK)
    {
        return result;
    }
    context->pst_Config = config;
    return GW_OK;
}

/** @brief Map runtime failures to Modbus application exceptions. */
static E_MODB_EXCEPTION fn_MapRuntime(E_GW_RESULT result)
{
    if (result == GW_OK) return MODB_EXCEPTION_NONE;
    if (result == GW_ERR_ARGUMENT) return MODB_EXCEPTION_ILLEGAL_VALUE;
    if (result == GW_ERR_RANGE) return MODB_EXCEPTION_ILLEGAL_ADDRESS;
    if ((result == GW_ERR_BUSY) || (result == GW_ERR_STATE) ||
        (result == GW_ERR_TIMEOUT)) return MODB_EXCEPTION_SERVER_BUSY;
    if (result == GW_ERR_NOT_FOUND) return MODB_EXCEPTION_GATEWAY_TARGET_FAILED;
    return MODB_EXCEPTION_SERVER_FAILURE;
}

/** @brief Build an exception PDU. */
static size_t fn_BuildException(uint8_t function,
                                E_MODB_EXCEPTION exception,
                                uint8_t *response)
{
    response[0] = (uint8_t)(function | 0x80U);
    response[1] = (uint8_t)exception;
    return 2U;
}

/** @brief Process function 01 or 02. */
static E_MODB_EXCEPTION fn_ReadBits(S_MODB_CONTEXT *context,
                                    uint8_t unit_id,
                                    uint8_t function,
                                    const uint8_t *request,
                                    size_t request_length,
                                    uint8_t *response,
                                    size_t *response_length)
{
    uint16_t start;
    uint16_t quantity;
    size_t byte_count;
    E_GW_RESULT result;
    if (request_length != 5U) return MODB_EXCEPTION_ILLEGAL_VALUE;
    start = fn_GetBe16(&request[1]);
    quantity = fn_GetBe16(&request[3]);
    if ((quantity == 0U) || (quantity > MODB_MAX_READ_BITS))
        return MODB_EXCEPTION_ILLEGAL_VALUE;
    byte_count = ((size_t)quantity + 7U) / 8U;
    response[0] = function;
    response[1] = (uint8_t)byte_count;
    result = (function == 1U ?
              fn_MODB_CoilsRead(&context->st_DataModel, unit_id, start,
                                quantity, &response[2], byte_count) :
              fn_MODB_DiscreteInputsRead(&context->st_DataModel, unit_id, start,
                                         quantity, &response[2], byte_count));
    if (result != GW_OK) return fn_MapRuntime(result);
    *response_length = 2U + byte_count;
    return MODB_EXCEPTION_NONE;
}

/** @brief Process function 03 or 04. */
static E_MODB_EXCEPTION fn_ReadRegisters(S_MODB_CONTEXT *context,
                                         uint8_t unit_id,
                                         uint8_t function,
                                         const uint8_t *request,
                                         size_t request_length,
                                         uint8_t *response,
                                         size_t *response_length)
{
    uint16_t start;
    uint16_t quantity;
    uint16_t values[MODB_MAX_READ_REGISTERS];
    uint16_t index;
    E_GW_RESULT result;
    if (request_length != 5U) return MODB_EXCEPTION_ILLEGAL_VALUE;
    start = fn_GetBe16(&request[1]);
    quantity = fn_GetBe16(&request[3]);
    if ((quantity == 0U) || (quantity > MODB_MAX_READ_REGISTERS))
        return MODB_EXCEPTION_ILLEGAL_VALUE;
    result = function == 3U ?
             fn_MODB_HoldingRegistersRead(&context->st_DataModel, unit_id,
                                          start, quantity, values) :
             fn_MODB_InputRegistersRead(&context->st_DataModel, unit_id,
                                        start, quantity, values);
    if (result != GW_OK) return fn_MapRuntime(result);
    response[0] = function;
    response[1] = (uint8_t)(quantity * 2U);
    for (index = 0U; index < quantity; ++index)
        fn_PutBe16(&response[2U + ((size_t)index * 2U)], values[index]);
    *response_length = 2U + ((size_t)quantity * 2U);
    return MODB_EXCEPTION_NONE;
}

/** @brief Process function 05. */
static E_MODB_EXCEPTION fn_WriteSingleCoil(S_MODB_CONTEXT *context,
                                           uint8_t unit_id,
                                           const uint8_t *request,
                                           size_t request_length,
                                           uint8_t *response,
                                           size_t *response_length,
                                           uint32_t peer_ipv4)
{
    uint16_t address;
    uint16_t value;
    uint8_t packed = 0U;
    E_GW_RESULT result;
    if (request_length != 5U) return MODB_EXCEPTION_ILLEGAL_VALUE;
    address = fn_GetBe16(&request[1]);
    value = fn_GetBe16(&request[3]);
    if ((value != 0x0000U) && (value != 0xFF00U))
        return MODB_EXCEPTION_ILLEGAL_VALUE;
    if (value == 0xFF00U) packed = 1U;
    result = fn_MODB_CoilsWrite(&context->st_DataModel, unit_id, address, 1U,
                              &packed, peer_ipv4);
    if (result != GW_OK) return fn_MapRuntime(result);
    (void)memcpy(response, request, 5U);
    *response_length = 5U;
    return MODB_EXCEPTION_NONE;
}

/** @brief Process function 15. */
static E_MODB_EXCEPTION fn_WriteMultipleCoils(S_MODB_CONTEXT *context,
                                              uint8_t unit_id,
                                              const uint8_t *request,
                                              size_t request_length,
                                              uint8_t *response,
                                              size_t *response_length,
                                              uint32_t peer_ipv4)
{
    uint16_t start;
    uint16_t quantity;
    uint8_t byte_count;
    size_t expected;
    E_GW_RESULT result;
    if (request_length < 6U) return MODB_EXCEPTION_ILLEGAL_VALUE;
    start = fn_GetBe16(&request[1]);
    quantity = fn_GetBe16(&request[3]);
    byte_count = request[5];
    expected = ((size_t)quantity + 7U) / 8U;
    if ((quantity == 0U) || (quantity > MODB_MAX_WRITE_BITS) ||
        ((size_t)byte_count != expected) ||
        (request_length != (6U + (size_t)byte_count)))
        return MODB_EXCEPTION_ILLEGAL_VALUE;
    result = fn_MODB_CoilsWrite(&context->st_DataModel, unit_id, start, quantity,
                              &request[6], peer_ipv4);
    if (result != GW_OK) return fn_MapRuntime(result);
    response[0] = 15U;
    (void)memcpy(&response[1], &request[1], 4U);
    *response_length = 5U;
    return MODB_EXCEPTION_NONE;
}

/** @brief Process function 06. */
static E_MODB_EXCEPTION fn_WriteSingleRegister(S_MODB_CONTEXT *context,
                                               uint8_t unit_id,
                                               const uint8_t *request,
                                               size_t request_length,
                                               uint8_t *response,
                                               size_t *response_length,
                                               uint32_t peer_ipv4)
{
    uint16_t address;
    uint16_t value;
    E_GW_RESULT result;
    if (request_length != 5U) return MODB_EXCEPTION_ILLEGAL_VALUE;
    address = fn_GetBe16(&request[1]);
    value = fn_GetBe16(&request[3]);
    result = fn_MODB_HoldingRegistersWrite(&context->st_DataModel, unit_id,
                                         address, 1U, &value, peer_ipv4);
    if (result != GW_OK) return fn_MapRuntime(result);
    (void)memcpy(response, request, 5U);
    *response_length = 5U;
    return MODB_EXCEPTION_NONE;
}

/** @brief Process function 16. */
static E_MODB_EXCEPTION fn_WriteMultipleRegisters(S_MODB_CONTEXT *context,
                                                  uint8_t unit_id,
                                                  const uint8_t *request,
                                                  size_t request_length,
                                                  uint8_t *response,
                                                  size_t *response_length,
                                                  uint32_t peer_ipv4)
{
    uint16_t start;
    uint16_t quantity;
    uint8_t byte_count;
    uint16_t values[MODB_MAX_WRITE_REGISTERS];
    uint16_t index;
    E_GW_RESULT result;
    if (request_length < 6U) return MODB_EXCEPTION_ILLEGAL_VALUE;
    start = fn_GetBe16(&request[1]);
    quantity = fn_GetBe16(&request[3]);
    byte_count = request[5];
    if ((quantity == 0U) || (quantity > MODB_MAX_WRITE_REGISTERS) ||
        ((uint16_t)byte_count != (uint16_t)(quantity * 2U)) ||
        (request_length != (6U + (size_t)byte_count)))
        return MODB_EXCEPTION_ILLEGAL_VALUE;
    for (index = 0U; index < quantity; ++index)
        values[index] = fn_GetBe16(&request[6U + ((size_t)index * 2U)]);
    result = fn_MODB_HoldingRegistersWrite(&context->st_DataModel, unit_id,
                                         start, quantity, values, peer_ipv4);
    if (result != GW_OK) return fn_MapRuntime(result);
    response[0] = 16U;
    (void)memcpy(&response[1], &request[1], 4U);
    *response_length = 5U;
    return MODB_EXCEPTION_NONE;
}

/** @brief Process function 23. */
static E_MODB_EXCEPTION fn_ReadWriteRegisters(S_MODB_CONTEXT *context,
                                              uint8_t unit_id,
                                              const uint8_t *request,
                                              size_t request_length,
                                              uint8_t *response,
                                              size_t *response_length,
                                              uint32_t peer_ipv4)
{
    uint16_t read_start;
    uint16_t read_quantity;
    uint16_t write_start;
    uint16_t write_quantity;
    uint8_t write_bytes;
    uint16_t write_values[121U];
    uint16_t read_values[MODB_MAX_READ_REGISTERS];
    uint16_t index;
    E_GW_RESULT result;
    if (request_length < 10U) return MODB_EXCEPTION_ILLEGAL_VALUE;
    read_start = fn_GetBe16(&request[1]);
    read_quantity = fn_GetBe16(&request[3]);
    write_start = fn_GetBe16(&request[5]);
    write_quantity = fn_GetBe16(&request[7]);
    write_bytes = request[9];
    if ((read_quantity == 0U) || (read_quantity > MODB_MAX_READ_REGISTERS) ||
        (write_quantity == 0U) || (write_quantity > 121U) ||
        ((uint16_t)write_bytes != (uint16_t)(write_quantity * 2U)) ||
        (request_length != (10U + (size_t)write_bytes)))
        return MODB_EXCEPTION_ILLEGAL_VALUE;
    S_GW_ROUTE_INFO info;
    if (fn_GW_RouteGetInfo(context->st_DataModel.pst_RouteManager, unit_id, &info) != GW_OK)
        return MODB_EXCEPTION_GATEWAY_TARGET_FAILED;
    if ((uint32_t)read_start + read_quantity > info.sz_AnalogOutputCount)
        return MODB_EXCEPTION_ILLEGAL_ADDRESS;
    for (index = 0U; index < write_quantity; ++index)
        write_values[index] = fn_GetBe16(&request[10U + ((size_t)index * 2U)]);
    result = fn_MODB_HoldingRegistersWrite(&context->st_DataModel, unit_id,
                                         write_start, write_quantity,
                                         write_values, peer_ipv4);
    if (result != GW_OK) return fn_MapRuntime(result);
    result = fn_MODB_HoldingRegistersRead(&context->st_DataModel, unit_id,
                                        read_start, read_quantity,
                                        read_values);
    if (result != GW_OK) return fn_MapRuntime(result);
    response[0] = 23U;
    response[1] = (uint8_t)(read_quantity * 2U);
    for (index = 0U; index < read_quantity; ++index)
        fn_PutBe16(&response[2U + ((size_t)index * 2U)], read_values[index]);
    *response_length = 2U + ((size_t)read_quantity * 2U);
    return MODB_EXCEPTION_NONE;
}

/** @brief Dispatch one PDU after Unit-ID routing has been validated. */
static E_MODB_EXCEPTION fn_ProcessPduLocked(S_MODB_CONTEXT *context,
                                      uint8_t unit_id,
                                      const uint8_t *request,
                                      size_t request_length,
                                      uint8_t *response,
                                      size_t *response_length,
                                      uint32_t peer_ipv4)
{
    S_GW_ROUTE_INFO info;
    E_GW_RESULT route_result;
    uint8_t function;
    E_MODB_EXCEPTION exception;
    if ((request == NULL) || (request_length == 0U))
        return MODB_EXCEPTION_ILLEGAL_VALUE;
    function = request[0];
    route_result = fn_GW_RouteGetInfo(context->st_DataModel.pst_RouteManager, unit_id, &info);
    if (route_result != GW_OK) return MODB_EXCEPTION_GATEWAY_TARGET_FAILED;
    switch (function)
    {
        case 1U:
        case 2U:
            exception = fn_ReadBits(context, unit_id, function, request,
                                    request_length, response, response_length);
            break;
        case 3U:
        case 4U:
            exception = fn_ReadRegisters(context, unit_id, function, request,
                                         request_length, response, response_length);
            break;
        case 5U:
            exception = fn_WriteSingleCoil(context, unit_id, request,
                                           request_length, response,
                                           response_length, peer_ipv4);
            break;
        case 6U:
            exception = fn_WriteSingleRegister(context, unit_id, request,
                                               request_length, response,
                                               response_length, peer_ipv4);
            break;
        case 15U:
            exception = fn_WriteMultipleCoils(context, unit_id, request,
                                              request_length, response,
                                              response_length, peer_ipv4);
            break;
        case 16U:
            exception = fn_WriteMultipleRegisters(context, unit_id, request,
                                                  request_length, response,
                                                  response_length, peer_ipv4);
            break;
        case 23U:
            exception = fn_ReadWriteRegisters(context, unit_id, request,
                                              request_length, response,
                                              response_length, peer_ipv4);
            break;
        default:
            exception = MODB_EXCEPTION_ILLEGAL_FUNCTION;
            break;
    }
    (void)info;
    return exception;
}

/** @brief Serialize a complete PDU with cyclic image updates. Never serve stale values. */
static E_MODB_EXCEPTION fn_ProcessPdu(S_MODB_CONTEXT *context, uint8_t unit_id,
    const uint8_t *request, size_t request_length, uint8_t *response,
    size_t *response_length, uint32_t peer_ipv4)
{
    S_GW_ROUTE_MANAGER *manager = context->st_DataModel.pst_RouteManager;
    S_GW_ROUTE_INFO info;
    E_MODB_EXCEPTION result;
    if (fn_GW_RouteTransactionBegin(manager, unit_id) != GW_OK)
        return MODB_EXCEPTION_GATEWAY_TARGET_FAILED;
    if (fn_GW_RouteGetInfo(manager, unit_id, &info) != GW_OK)
        result = MODB_EXCEPTION_GATEWAY_TARGET_FAILED;
    else if (!info.b_IoOpen || !info.b_InputValid ||
             info.ui64_InputAgeMs > context->pst_Config->ui32_InputStaleTimeoutMs)
        result = MODB_EXCEPTION_SERVER_BUSY;
    else
        result = fn_ProcessPduLocked(context, unit_id, request, request_length,
                                    response, response_length, peer_ipv4);
    fn_GW_RouteTransactionEnd(manager, unit_id);
    return result;
}

E_MODB_RESULT fn_MODB_ProcessTcpRequest(S_MODB_CONTEXT *context,
                                        const uint8_t *stream_data,
                                        size_t stream_length,
                                        uint8_t *response,
                                        size_t response_capacity,
                                        size_t *consumed,
                                        size_t *response_length,
                                        uint32_t peer_ipv4)
{
    uint16_t protocol_id;
    uint16_t length_field;
    size_t adu_length;
    size_t pdu_response_length = 0U;
    E_MODB_EXCEPTION exception;
    uint8_t function;
    if ((context == NULL) || (stream_data == NULL) || (response == NULL) ||
        (consumed == NULL) || (response_length == NULL))
        return MODB_RESULT_INVALID_ARGUMENT;
    *consumed = 0U;
    *response_length = 0U;
    if (stream_length < 7U) return MODB_RESULT_NEED_MORE_DATA;
    protocol_id = fn_GetBe16(&stream_data[2]);
    length_field = fn_GetBe16(&stream_data[4]);
    if ((protocol_id != 0U) || (length_field < 2U) ||
        (length_field > (GW_MODBUS_TCP_ADU_MAX - 6U)))
    {
        *consumed = 7U;
        return MODB_RESULT_FRAME_ERROR;
    }
    adu_length = 6U + (size_t)length_field;
    if (stream_length < adu_length) return MODB_RESULT_NEED_MORE_DATA;
    *consumed = adu_length;
    if (response_capacity < GW_MODBUS_TCP_ADU_MAX)
        return MODB_RESULT_BUFFER_TOO_SMALL;
    (void)memcpy(response, stream_data, 7U);
    function = stream_data[7];
    exception = fn_ProcessPdu(context, stream_data[6], &stream_data[7],
                              adu_length - 7U, &response[7],
                              &pdu_response_length, peer_ipv4);
    if (exception != MODB_EXCEPTION_NONE)
        pdu_response_length = fn_BuildException(function, exception, &response[7]);
    fn_PutBe16(&response[4], (uint16_t)(pdu_response_length + 1U));
    *response_length = 7U + pdu_response_length;
    fn_GW_RecordRequest(context->st_DataModel.pst_RouteManager,
                        exception != MODB_EXCEPTION_NONE);
    return MODB_RESULT_RESPONSE_READY;
}
