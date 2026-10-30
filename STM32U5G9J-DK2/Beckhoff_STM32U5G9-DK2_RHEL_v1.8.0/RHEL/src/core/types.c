/**
 * @file types.c
 * @brief Text conversion helpers for core public enumerations.
 * @author Prathamesh Chipkar
 */
#include "ead/types.h"

const char *fn_EadResultText(ead_result_t result)
{
    switch (result)
    {
        case EAD_OK: return "success";
        case EAD_ERR_ARGUMENT: return "invalid argument";
        case EAD_ERR_CONFIG: return "configuration error";
        case EAD_ERR_RANGE: return "range error";
        case EAD_ERR_IO: return "I/O error";
        case EAD_ERR_PROTOCOL: return "protocol error";
        case EAD_ERR_TIMEOUT: return "timeout";
        case EAD_ERR_STATE: return "invalid state";
        case EAD_ERR_NOT_FOUND: return "not found";
        case EAD_ERR_REMOTE: return "remote device error";
        default: return "unknown error";
    }
}

const char *fn_EadTerminalKindText(ead_terminal_kind_t kind)
{
    switch (kind)
    {
        case EAD_TERMINAL_EL1409: return "EL1409";
        case EAD_TERMINAL_EL2409: return "EL2409";
        case EAD_TERMINAL_EL3074: return "EL3074";
        case EAD_TERMINAL_EL4074: return "EL4074";
        case EAD_TERMINAL_RAW:    return "RAW";
        default: return "UNKNOWN";
    }
}

const char *fn_EadDomainText(ead_address_domain_t domain)
{
    switch (domain)
    {
        case EAD_DOMAIN_DI: return "DI";
        case EAD_DOMAIN_DO: return "DO";
        case EAD_DOMAIN_AI: return "AI";
        case EAD_DOMAIN_AO: return "AO";
        case EAD_DOMAIN_RAW: return "RAW";
        default: return "UNKNOWN";
    }
}

const char *fn_EadOwnershipText(ead_ownership_mode_t mode)
{
    switch (mode)
    {
        case EAD_OWNERSHIP_RUNNING: return "RUNNING";
        case EAD_OWNERSHIP_RELEASED: return "RELEASED";
        case EAD_OWNERSHIP_FAULTED: return "FAULTED";
        default: return "UNKNOWN";
    }
}
