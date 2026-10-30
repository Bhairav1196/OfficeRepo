/**
 * @file cip.c
 * @brief Common CIP path and service encoding/decoding helpers.
 * @author Prathamesh Chipkar
 */
#include "cip.h"

#include "eip_endian.h"

#include <string.h>

static eip_result_t fn_AppendLogicalSegment(uint8_t segment8,
                                            uint8_t segment16,
                                            uint16_t value,
                                            uint8_t *buffer,
                                            size_t capacity,
                                            size_t *cursor)
{
    if (value <= 0xFFU)
    {
        if ((*cursor + 2U) > capacity) return EIP_ERR_OVERFLOW;
        buffer[(*cursor)++] = segment8;
        buffer[(*cursor)++] = (uint8_t)value;
    }
    else
    {
        if ((*cursor + 4U) > capacity) return EIP_ERR_OVERFLOW;
        buffer[(*cursor)++] = segment16;
        buffer[(*cursor)++] = 0U;
        fn_WriteLe16(&buffer[*cursor], value);
        *cursor += 2U;
    }
    return EIP_OK;
}

eip_result_t fn_CipBuildRequest(uint8_t service,
                                uint16_t class_id,
                                uint16_t instance_id,
                                uint16_t attribute_id,
                                const uint8_t *request_data,
                                size_t request_data_length,
                                uint8_t *buffer,
                                size_t capacity,
                                size_t *length)
{
    uint8_t path[EIP_MAX_PATH_SIZE];
    size_t path_length = 0U;
    size_t cursor = 0U;
    eip_result_t result;

    if ((buffer == NULL) || (length == NULL) ||
        ((request_data_length > 0U) && (request_data == NULL)))
    {
        return EIP_ERR_ARGUMENT;
    }

    result = fn_AppendLogicalSegment(0x20U, 0x21U, class_id,
                                     path, sizeof(path), &path_length);
    if (result != EIP_OK) return result;

    result = fn_AppendLogicalSegment(0x24U, 0x25U, instance_id,
                                     path, sizeof(path), &path_length);
    if (result != EIP_OK) return result;

    if (attribute_id != 0U)
    {
        result = fn_AppendLogicalSegment(0x30U, 0x31U, attribute_id,
                                         path, sizeof(path), &path_length);
        if (result != EIP_OK) return result;
    }

    if ((path_length & 1U) != 0U)
    {
        if (path_length >= sizeof(path)) return EIP_ERR_OVERFLOW;
        path[path_length++] = 0U;
    }

    if ((2U + path_length + request_data_length) > capacity)
    {
        return EIP_ERR_OVERFLOW;
    }

    buffer[cursor++] = service;
    buffer[cursor++] = (uint8_t)(path_length / 2U);
    (void)memcpy(&buffer[cursor], path, path_length);
    cursor += path_length;
    if (request_data_length > 0U)
    {
        (void)memcpy(&buffer[cursor], request_data, request_data_length);
        cursor += request_data_length;
    }

    *length = cursor;
    return EIP_OK;
}

eip_result_t fn_CipParseResponse(const uint8_t *buffer,
                                 size_t length,
                                 cip_response_view_t *response)
{
    size_t additional_words;
    size_t cursor;

    if ((buffer == NULL) || (response == NULL) || (length < 4U))
    {
        return EIP_ERR_ARGUMENT;
    }

    (void)memset(response, 0, sizeof(*response));
    response->response_service = buffer[0];
    response->general_status = buffer[2];
    additional_words = buffer[3];
    cursor = 4U;

    if ((cursor + (additional_words * 2U)) > length)
    {
        return EIP_ERR_PROTOCOL;
    }

    response->additional_status_count = additional_words;
    if (response->additional_status_count > 8U)
    {
        response->additional_status_count = 8U;
    }

    for (size_t index = 0U; index < response->additional_status_count; ++index)
    {
        response->additional_status[index] = fn_ReadLe16(&buffer[cursor + (index * 2U)]);
    }

    cursor += additional_words * 2U;
    response->data = &buffer[cursor];
    response->data_length = length - cursor;
    return EIP_OK;
}

const char *fn_CipStatusText(uint8_t general_status)
{
    switch (general_status)
    {
        case 0x00U: return "Success";
        case 0x01U: return "Connection failure";
        case 0x02U: return "Resource unavailable";
        case 0x03U: return "Invalid parameter value";
        case 0x04U: return "Path segment error";
        case 0x05U: return "Path destination unknown";
        case 0x06U: return "Partial transfer";
        case 0x08U: return "Service not supported";
        case 0x09U: return "Invalid attribute value";
        case 0x0CU: return "Object state conflict";
        case 0x0EU: return "Attribute not settable";
        case 0x13U: return "Insufficient request data";
        case 0x14U: return "Attribute not supported";
        case 0x15U: return "Too much data";
        case 0x1FU: return "Vendor-specific error";
        default:    return "Unknown CIP status";
    }
}
