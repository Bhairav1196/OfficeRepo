/** @file cip_explicit.h
 * @author Prathamesh Chipkar
 *  @brief Unconnected explicit CIP attribute services over SendRRData.
 */
#ifndef CIP_EXPLICIT_H
#define CIP_EXPLICIT_H

#include <stddef.h>
#include <stdint.h>

#include "eip_types.h"

/** @brief Invoke Get_Attribute_Single (0x0E). */
eip_result_t fn_CipGetAttributeSingle(eip_session_t *session,
                                      uint16_t class_id,
                                      uint16_t instance_id,
                                      uint16_t attribute_id,
                                      uint8_t *data,
                                      size_t capacity,
                                      size_t *data_length,
                                      cip_response_view_t *response_view);

/**
 * @brief Invoke Get_Attribute_Single without logging an expected remote miss.
 *
 * @details Discovery uses this variant while probing the first terminal slot
 * that is not populated and while testing optional CoE PDO objects.
 */
eip_result_t fn_CipGetAttributeSingleQuiet(eip_session_t *session,
                                           uint16_t class_id,
                                           uint16_t instance_id,
                                           uint16_t attribute_id,
                                           uint8_t *data,
                                           size_t capacity,
                                           size_t *data_length,
                                           cip_response_view_t *response_view);

/** @brief Invoke Set_Attribute_Single (0x10). */
eip_result_t fn_CipSetAttributeSingle(eip_session_t *session,
                                      uint16_t class_id,
                                      uint16_t instance_id,
                                      uint16_t attribute_id,
                                      const uint8_t *data,
                                      size_t data_length,
                                      cip_response_view_t *response_view);

/** @brief Invoke Get_Attributes_All (0x01). */
eip_result_t fn_CipGetAttributesAll(eip_session_t *session,
                                    uint16_t class_id,
                                    uint16_t instance_id,
                                    uint8_t *data,
                                    size_t capacity,
                                    size_t *data_length,
                                    cip_response_view_t *response_view);

#endif
