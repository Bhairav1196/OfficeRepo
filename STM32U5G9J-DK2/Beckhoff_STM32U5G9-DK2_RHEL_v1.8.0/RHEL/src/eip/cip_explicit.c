/**
 * @file cip_explicit.c
 * @brief CIP explicit-message request construction and response validation.
 * @author Prathamesh Chipkar
 */
#include "cip_explicit.h"

#include "cip.h"
#include "eip_session.h"
#include "logger.h"

#include <string.h>

static eip_result_t fn_SendExplicit(eip_session_t *session,
                                    uint8_t service,
                                    uint16_t class_id,
                                    uint16_t instance_id,
                                    uint16_t attribute_id,
                                    const uint8_t *request_data,
                                    size_t request_data_length,
                                    uint8_t *data,
                                    size_t capacity,
                                    size_t *data_length,
                                    cip_response_view_t *response_view,
                                    bool log_remote_error)
{
    uint8_t request[4096];
    uint8_t response_buffer[EIP_MAX_CIP_RESPONSE_SIZE];
    size_t request_length = 0U;
    size_t response_length = 0U;
    cip_response_view_t parsed;
    eip_result_t result;

    result = fn_CipBuildRequest(service, class_id, instance_id, attribute_id,
                                request_data, request_data_length,
                                request, sizeof(request), &request_length);
    if (result != EIP_OK)
    {
        return result;
    }

    fn_LogHex(LOG_LEVEL_TRACE, "CIP explicit request", request, request_length);
    result = fn_EipSendRRData(session, request, request_length,
                              response_buffer, sizeof(response_buffer),
                              &response_length, NULL, 0U, NULL);
    if (result != EIP_OK)
    {
        return result;
    }

    result = fn_CipParseResponse(response_buffer, response_length, &parsed);
    if (result != EIP_OK)
    {
        return result;
    }

    if (response_view != NULL)
    {
        /* parsed.data points into this function's stack buffer. Preserve only
           the status metadata until any payload is copied into caller-owned
           storage below. */
        *response_view = parsed;
        response_view->data = NULL;
        response_view->data_length = 0U;
    }

    if (parsed.general_status != CIP_GENERAL_STATUS_SUCCESS)
    {
        if (log_remote_error)
        {
            LOG_ERROR("CIP service 0x%02X class 0x%04X instance %u attribute %u failed: "
                      "general status 0x%02X (%s)",
                      service, class_id, instance_id, attribute_id,
                      parsed.general_status, fn_CipStatusText(parsed.general_status));
            if (parsed.additional_status_count > 0U)
            {
                for (size_t index = 0U; index < parsed.additional_status_count; ++index)
                {
                    LOG_ERROR("  Additional status[%zu] = 0x%04X",
                              index, parsed.additional_status[index]);
                }
            }
        }
        return EIP_ERR_REMOTE;
    }

    if (data_length != NULL)
    {
        *data_length = parsed.data_length;
    }
    if ((data != NULL) && (parsed.data_length > 0U))
    {
        if (parsed.data_length > capacity)
        {
            return EIP_ERR_OVERFLOW;
        }
        (void)memcpy(data, parsed.data, parsed.data_length);
        if (response_view != NULL)
        {
            response_view->data = data;
            response_view->data_length = parsed.data_length;
        }
    }
    return EIP_OK;
}

eip_result_t fn_CipGetAttributeSingle(eip_session_t *session,
                                      uint16_t class_id,
                                      uint16_t instance_id,
                                      uint16_t attribute_id,
                                      uint8_t *data,
                                      size_t capacity,
                                      size_t *data_length,
                                      cip_response_view_t *response_view)
{
    return fn_SendExplicit(session, CIP_SERVICE_GET_ATTRIBUTE_SINGLE,
                           class_id, instance_id, attribute_id,
                           NULL, 0U, data, capacity, data_length, response_view,
                           true);
}

eip_result_t fn_CipGetAttributeSingleQuiet(eip_session_t *session,
                                           uint16_t class_id,
                                           uint16_t instance_id,
                                           uint16_t attribute_id,
                                           uint8_t *data,
                                           size_t capacity,
                                           size_t *data_length,
                                           cip_response_view_t *response_view)
{
    return fn_SendExplicit(session, CIP_SERVICE_GET_ATTRIBUTE_SINGLE,
                           class_id, instance_id, attribute_id,
                           NULL, 0U, data, capacity, data_length, response_view,
                           false);
}

eip_result_t fn_CipSetAttributeSingle(eip_session_t *session,
                                      uint16_t class_id,
                                      uint16_t instance_id,
                                      uint16_t attribute_id,
                                      const uint8_t *data,
                                      size_t data_length,
                                      cip_response_view_t *response_view)
{
    size_t response_data_length = 0U;
    return fn_SendExplicit(session, CIP_SERVICE_SET_ATTRIBUTE_SINGLE,
                           class_id, instance_id, attribute_id,
                           data, data_length, NULL, 0U,
                           &response_data_length, response_view, true);
}

eip_result_t fn_CipGetAttributesAll(eip_session_t *session,
                                    uint16_t class_id,
                                    uint16_t instance_id,
                                    uint8_t *data,
                                    size_t capacity,
                                    size_t *data_length,
                                    cip_response_view_t *response_view)
{
    return fn_SendExplicit(session, CIP_SERVICE_GET_ATTRIBUTES_ALL,
                           class_id, instance_id, 0U,
                           NULL, 0U, data, capacity, data_length, response_view,
                           true);
}
