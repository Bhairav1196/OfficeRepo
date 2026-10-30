/**
 * @file gateway_types.h
 * @brief Shared limits, states, and error identifiers for the single EK9500 gateway.
 * @author Prathamesh Chipkar
 */
#ifndef GW_GATEWAY_TYPES_H
#define GW_GATEWAY_TYPES_H

#include "ead/types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GW_COUPLER_COUNT                 (1U)
#define GW_MAX_MODBUS_CLIENTS            (32U)
#define GW_MODBUS_TCP_ADU_MAX            (260U)
#define GW_MODBUS_STREAM_CAPACITY        (4160U)
#define GW_MODBUS_TX_CAPACITY            (4160U)
#define GW_MAX_DIGITAL_CHANNELS          (EAD_MAX_TERMINALS * 16U)
#define GW_MAX_ANALOG_CHANNELS           (EAD_MAX_TERMINALS * 4U)
#define GW_PATH_LENGTH                   (512U)
#define GW_NAME_LENGTH                   (64U)
#define GW_IPV4_LENGTH                   (16U)

/** @brief Stable gateway result codes. */
typedef enum
{
    GW_OK = 0,
    GW_ERR_ARGUMENT = -1,
    GW_ERR_CONFIG = -2,
    GW_ERR_RANGE = -3,
    GW_ERR_IO = -4,
    GW_ERR_PROTOCOL = -5,
    GW_ERR_TIMEOUT = -6,
    GW_ERR_STATE = -7,
    GW_ERR_NOT_FOUND = -8,
    GW_ERR_REMOTE = -9,
    GW_ERR_BUSY = -10
} E_GW_RESULT;

/** @brief EtherNet/IP route state. */
typedef enum
{
    GW_ROUTE_STOPPED = 0,
    GW_ROUTE_CONNECTING = 1,
    GW_ROUTE_ONLINE = 2,
    GW_ROUTE_DEGRADED = 3,
    GW_ROUTE_FAULT = 4
} E_GW_ROUTE_STATE;

/** @brief Convert a result identifier to stable text. */
const char *fn_GW_ResultText(E_GW_RESULT e_Result);

#ifdef __cplusplus
}
#endif

#endif
