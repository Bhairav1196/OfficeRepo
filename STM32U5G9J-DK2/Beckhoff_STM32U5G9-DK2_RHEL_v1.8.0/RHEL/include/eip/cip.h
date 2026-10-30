/** @file cip.h
 * @author Prathamesh Chipkar
 *  @brief Generic CIP request construction and Message Router response parsing.
 */
#ifndef CIP_H
#define CIP_H

#include <stddef.h>
#include <stdint.h>

#include "eip_types.h"

/** @brief Build a CIP service request with logical Class/Instance/Attribute path. */
eip_result_t fn_CipBuildRequest(uint8_t service,
                                uint16_t class_id,
                                uint16_t instance_id,
                                uint16_t attribute_id,
                                const uint8_t *request_data,
                                size_t request_data_length,
                                uint8_t *buffer,
                                size_t capacity,
                                size_t *length);

/** @brief Parse a CIP Message Router response into a non-owning view. */
eip_result_t fn_CipParseResponse(const uint8_t *buffer,
                                 size_t length,
                                 cip_response_view_t *response);

/** @brief Return a readable description of a CIP general status code. */
const char *fn_CipStatusText(uint8_t general_status);

#endif
