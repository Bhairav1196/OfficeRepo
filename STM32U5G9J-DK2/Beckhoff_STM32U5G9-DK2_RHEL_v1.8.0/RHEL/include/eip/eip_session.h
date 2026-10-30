/** @file eip_session.h
 * @author Prathamesh Chipkar
 *  @brief EtherNet/IP TCP encapsulation session and SendRRData transport.
 */
#ifndef EIP_SESSION_H
#define EIP_SESSION_H

#include <stddef.h>
#include <stdint.h>

#include "eip_types.h"

/** @brief Connect to TCP 44818 and issue Register Session. */
eip_result_t fn_EipSessionOpen(eip_session_t *session,
                               const char *target_ip,
                               uint16_t target_tcp_port,
                               int timeout_ms);
/** @brief Unregister and close an encapsulation session. */
void fn_EipSessionClose(eip_session_t *session);
/** @brief Send an EtherNet/IP encapsulation NOP. */
eip_result_t fn_EipSessionKeepAlive(eip_session_t *session);

/** @brief Send an unconnected CIP request using SendRRData and parse CPF items. */
eip_result_t fn_EipSendRRData(eip_session_t *session,
                              const uint8_t *cip_request,
                              size_t cip_request_length,
                              uint8_t *response_buffer,
                              size_t response_buffer_capacity,
                              size_t *response_length,
                              eip_cpf_item_t *additional_items,
                              size_t additional_items_capacity,
                              size_t *additional_items_count);

#endif
