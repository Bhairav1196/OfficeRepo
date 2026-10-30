/**
 * @file gateway_config.h
 * @brief Startup configuration for up to two Modbus TCP listeners and one EK9500 coupler.
 * @author Prathamesh Chipkar
 */
#ifndef GW_GATEWAY_CONFIG_H
#define GW_GATEWAY_CONFIG_H

#include "gateway/gateway_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief One Modbus Unit-ID to EK9500 route. */
typedef struct
{
    char ac_Name[GW_NAME_LENGTH];
    char ac_CouplerConfigPath[GW_PATH_LENGTH];
    uint8_t ui8_UnitId;
    uint32_t ui32_WorkerPeriodMs;
    bool b_Enabled;
} S_GW_ROUTE_CONFIG;

/** @brief Complete startup-only configuration. */
typedef struct
{
    char ac_BindIp[GW_IPV4_LENGTH];
    char ac_MasterAclPath[GW_PATH_LENGTH];
    char ac_PidFilePath[GW_PATH_LENGTH];
    uint16_t ui16_Port;
    uint16_t ui16_SecondaryPort; /**< 0 disables the secondary listener. */
    uint8_t ui8_MaxClients;
    uint8_t ui8_MaxRequestsPerEventPerClient;
    uint32_t ui32_IdleTimeoutMs;
    uint32_t ui32_MaxRequestsPerSecondPerClient;
    uint32_t ui32_InputStaleTimeoutMs;
    bool b_EnableTcpKeepalive;
    bool b_RejectWritesWhenStale;
    S_GW_ROUTE_CONFIG ast_Routes[GW_COUPLER_COUNT];
} S_GW_CONFIG;

/** @brief Load safe project defaults. */
void fn_GW_ConfigDefaults(S_GW_CONFIG *pst_Config);

/** @brief Load and validate the gateway configuration file. */
E_GW_RESULT fn_GW_ConfigLoad(const char *pc_Path,
                             S_GW_CONFIG *pst_Config,
                             char *pc_Error,
                             size_t sz_ErrorSize);

/** @brief Validate listener, master-ACL, Unit-ID, and coupler paths. */
E_GW_RESULT fn_GW_ConfigValidate(const S_GW_CONFIG *pst_Config,
                                 char *pc_Error,
                                 size_t sz_ErrorSize);

#ifdef __cplusplus
}
#endif

#endif
