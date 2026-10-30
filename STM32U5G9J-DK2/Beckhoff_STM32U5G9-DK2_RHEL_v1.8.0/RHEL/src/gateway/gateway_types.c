/**
 * @file gateway_types.c
 * @brief Stable text for gateway result identifiers.
 * @author Prathamesh Chipkar
 */
#include "gateway/gateway_types.h"

const char *fn_GW_ResultText(E_GW_RESULT e_Result)
{
    switch (e_Result)
    {
        case GW_OK: return "success";
        case GW_ERR_ARGUMENT: return "invalid argument";
        case GW_ERR_CONFIG: return "configuration error";
        case GW_ERR_RANGE: return "out of range";
        case GW_ERR_IO: return "I/O error";
        case GW_ERR_PROTOCOL: return "protocol error";
        case GW_ERR_TIMEOUT: return "timeout";
        case GW_ERR_STATE: return "invalid state";
        case GW_ERR_NOT_FOUND: return "not found";
        case GW_ERR_REMOTE: return "remote device error";
        case GW_ERR_BUSY: return "busy or stale";
        default: return "unknown error";
    }
}
