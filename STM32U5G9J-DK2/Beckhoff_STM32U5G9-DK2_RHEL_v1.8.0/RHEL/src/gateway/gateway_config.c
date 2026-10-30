/**
 * @file gateway_config.c
 * @brief Minimal INI parser for the single-EK9500 Modbus TCP gateway.
 * @author Prathamesh Chipkar
 */
#define _POSIX_C_SOURCE 200809L
#include "gateway/gateway_config.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define GW_CONFIG_LINE_LENGTH (1024U)

static void fn_SetError(char *pc_Error, size_t sz_ErrorSize, const char *pc_Text)
{
    if ((pc_Error != NULL) && (sz_ErrorSize > 0U))
    {
        (void)snprintf(pc_Error, sz_ErrorSize, "%s",
                       (pc_Text != NULL) ? pc_Text : "configuration error");
    }
}

static char *fn_Trim(char *pc_Text)
{
    char *pc_End;
    while ((*pc_Text != '\0') && (isspace((unsigned char)*pc_Text) != 0))
    {
        ++pc_Text;
    }
    pc_End = pc_Text + strlen(pc_Text);
    while ((pc_End > pc_Text) && (isspace((unsigned char)pc_End[-1]) != 0))
    {
        --pc_End;
    }
    *pc_End = '\0';
    return pc_Text;
}

static bool fn_ParseBool(const char *pc_Text, bool *pb_Value)
{
    if ((pc_Text == NULL) || (pb_Value == NULL))
    {
        return false;
    }
    if ((strcasecmp(pc_Text, "true") == 0) || (strcmp(pc_Text, "1") == 0) ||
        (strcasecmp(pc_Text, "yes") == 0) || (strcasecmp(pc_Text, "on") == 0))
    {
        *pb_Value = true;
        return true;
    }
    if ((strcasecmp(pc_Text, "false") == 0) || (strcmp(pc_Text, "0") == 0) ||
        (strcasecmp(pc_Text, "no") == 0) || (strcasecmp(pc_Text, "off") == 0))
    {
        *pb_Value = false;
        return true;
    }
    return false;
}

static bool fn_ParseU32(const char *pc_Text, uint32_t *pui32_Value)
{
    char *pc_End = NULL;
    unsigned long ul_Parsed;
    if ((pc_Text == NULL) || (pui32_Value == NULL))
    {
        return false;
    }
    errno = 0;
    ul_Parsed = strtoul(pc_Text, &pc_End, 0);
    if ((errno != 0) || (pc_End == pc_Text) || (*pc_End != '\0') ||
        (ul_Parsed > UINT32_MAX))
    {
        return false;
    }
    *pui32_Value = (uint32_t)ul_Parsed;
    return true;
}

static bool fn_Copy(char *pc_Destination, size_t sz_Capacity, const char *pc_Source)
{
    int i32_Written;
    if ((pc_Destination == NULL) || (sz_Capacity == 0U) || (pc_Source == NULL))
    {
        return false;
    }
    i32_Written = snprintf(pc_Destination, sz_Capacity, "%s", pc_Source);
    return (i32_Written >= 0) && ((size_t)i32_Written < sz_Capacity);
}

void fn_GW_ConfigDefaults(S_GW_CONFIG *pst_Config)
{
    if (pst_Config == NULL)
    {
        return;
    }
    (void)memset(pst_Config, 0, sizeof(*pst_Config));
    (void)fn_Copy(pst_Config->ac_BindIp, sizeof(pst_Config->ac_BindIp), "0.0.0.0");
    (void)fn_Copy(pst_Config->ac_MasterAclPath,
                  sizeof(pst_Config->ac_MasterAclPath), "masters.conf");
    (void)fn_Copy(pst_Config->ac_PidFilePath,
                  sizeof(pst_Config->ac_PidFilePath), "../runtime/gateway.pid");
    pst_Config->ui16_Port = 1502U;
    pst_Config->ui16_SecondaryPort = 0U;
    pst_Config->ui8_MaxClients = 16U;
    pst_Config->ui8_MaxRequestsPerEventPerClient = 16U;
    pst_Config->ui32_IdleTimeoutMs = 60000U;
    pst_Config->ui32_MaxRequestsPerSecondPerClient = 200U;
    pst_Config->ui32_InputStaleTimeoutMs = 500U;
    pst_Config->b_EnableTcpKeepalive = true;
    pst_Config->b_RejectWritesWhenStale = true;

    (void)fn_Copy(pst_Config->ast_Routes[0].ac_Name,
                  sizeof(pst_Config->ast_Routes[0].ac_Name), "coupler");
    (void)fn_Copy(pst_Config->ast_Routes[0].ac_CouplerConfigPath,
                  sizeof(pst_Config->ast_Routes[0].ac_CouplerConfigPath),
                  "coupler.conf");
    pst_Config->ast_Routes[0].ui8_UnitId = 1U;
    pst_Config->ast_Routes[0].ui32_WorkerPeriodMs = 5U;
    pst_Config->ast_Routes[0].b_Enabled = true;

}

static S_GW_ROUTE_CONFIG *fn_RouteBySection(S_GW_CONFIG *pst_Config,
                                             const char *pc_Section)
{
    if (strcmp(pc_Section, "route.coupler") == 0)
    {
        return &pst_Config->ast_Routes[0];
    }
    return NULL;
}

static E_GW_RESULT fn_ApplyGlobal(S_GW_CONFIG *pst_Config,
                                  const char *pc_Key,
                                  const char *pc_Value)
{
    uint32_t ui32_Parsed = 0U;
    bool b_Parsed = false;
    if (strcmp(pc_Key, "bind_ip") == 0)
    {
        return fn_Copy(pst_Config->ac_BindIp, sizeof(pst_Config->ac_BindIp),
                       pc_Value) ? GW_OK : GW_ERR_RANGE;
    }
    if (strcmp(pc_Key, "master_acl_file") == 0)
    {
        return fn_Copy(pst_Config->ac_MasterAclPath,
                       sizeof(pst_Config->ac_MasterAclPath), pc_Value) ?
               GW_OK : GW_ERR_RANGE;
    }
    if (strcmp(pc_Key, "pid_file") == 0)
    {
        return fn_Copy(pst_Config->ac_PidFilePath,
                       sizeof(pst_Config->ac_PidFilePath), pc_Value) ?
               GW_OK : GW_ERR_RANGE;
    }
    if (strcmp(pc_Key, "port") == 0)
    {
        if ((!fn_ParseU32(pc_Value, &ui32_Parsed)) || (ui32_Parsed == 0U) ||
            (ui32_Parsed > UINT16_MAX))
        {
            return GW_ERR_RANGE;
        }
        pst_Config->ui16_Port = (uint16_t)ui32_Parsed;
        return GW_OK;
    }
    if (strcmp(pc_Key, "secondary_port") == 0)
    {
        if ((!fn_ParseU32(pc_Value, &ui32_Parsed)) ||
            (ui32_Parsed > UINT16_MAX))
        {
            return GW_ERR_RANGE;
        }
        pst_Config->ui16_SecondaryPort = (uint16_t)ui32_Parsed;
        return GW_OK;
    }
    if (strcmp(pc_Key, "max_clients") == 0)
    {
        if ((!fn_ParseU32(pc_Value, &ui32_Parsed)) || (ui32_Parsed == 0U) ||
            (ui32_Parsed > GW_MAX_MODBUS_CLIENTS))
        {
            return GW_ERR_RANGE;
        }
        pst_Config->ui8_MaxClients = (uint8_t)ui32_Parsed;
        return GW_OK;
    }
    if (strcmp(pc_Key, "max_requests_per_event_per_client") == 0)
    {
        if ((!fn_ParseU32(pc_Value, &ui32_Parsed)) || (ui32_Parsed == 0U) ||
            (ui32_Parsed > 64U))
        {
            return GW_ERR_RANGE;
        }
        pst_Config->ui8_MaxRequestsPerEventPerClient = (uint8_t)ui32_Parsed;
        return GW_OK;
    }
    if (strcmp(pc_Key, "idle_timeout_ms") == 0)
    {
        return fn_ParseU32(pc_Value, &pst_Config->ui32_IdleTimeoutMs) ?
               GW_OK : GW_ERR_RANGE;
    }
    if (strcmp(pc_Key, "max_requests_per_second_per_client") == 0)
    {
        return fn_ParseU32(pc_Value,
                           &pst_Config->ui32_MaxRequestsPerSecondPerClient) ?
               GW_OK : GW_ERR_RANGE;
    }
    if (strcmp(pc_Key, "input_stale_timeout_ms") == 0)
    {
        return fn_ParseU32(pc_Value, &pst_Config->ui32_InputStaleTimeoutMs) ?
               GW_OK : GW_ERR_RANGE;
    }
    if (strcmp(pc_Key, "enable_tcp_keepalive") == 0)
    {
        if (!fn_ParseBool(pc_Value, &b_Parsed))
        {
            return GW_ERR_RANGE;
        }
        pst_Config->b_EnableTcpKeepalive = b_Parsed;
        return GW_OK;
    }
    if (strcmp(pc_Key, "reject_writes_when_stale") == 0)
    {
        if (!fn_ParseBool(pc_Value, &b_Parsed))
        {
            return GW_ERR_RANGE;
        }
        pst_Config->b_RejectWritesWhenStale = b_Parsed;
        return GW_OK;
    }
    return GW_ERR_CONFIG;
}

static E_GW_RESULT fn_ApplyRoute(S_GW_ROUTE_CONFIG *pst_Route,
                                 const char *pc_Key,
                                 const char *pc_Value)
{
    uint32_t ui32_Parsed = 0U;
    bool b_Parsed = false;
    if (strcmp(pc_Key, "unit_id") == 0)
    {
        if ((!fn_ParseU32(pc_Value, &ui32_Parsed)) || (ui32_Parsed == 0U) ||
            (ui32_Parsed > UINT8_MAX))
        {
            return GW_ERR_RANGE;
        }
        pst_Route->ui8_UnitId = (uint8_t)ui32_Parsed;
        return GW_OK;
    }
    if (strcmp(pc_Key, "coupler_config") == 0)
    {
        return fn_Copy(pst_Route->ac_CouplerConfigPath,
                       sizeof(pst_Route->ac_CouplerConfigPath), pc_Value) ?
               GW_OK : GW_ERR_RANGE;
    }
    if (strcmp(pc_Key, "worker_period_ms") == 0)
    {
        return fn_ParseU32(pc_Value, &pst_Route->ui32_WorkerPeriodMs) ?
               GW_OK : GW_ERR_RANGE;
    }
    if (strcmp(pc_Key, "enabled") == 0)
    {
        if (!fn_ParseBool(pc_Value, &b_Parsed))
        {
            return GW_ERR_RANGE;
        }
        pst_Route->b_Enabled = b_Parsed;
        return GW_OK;
    }
    return GW_ERR_CONFIG;
}

static E_GW_RESULT fn_ResolvePaths(const char *pc_GatewayPath,
                                   S_GW_CONFIG *pst_Config,
                                   char *pc_Error,
                                   size_t sz_ErrorSize)
{
    char ac_Directory[GW_PATH_LENGTH];
    const char *pc_Separator = strrchr(pc_GatewayPath, '/');
    size_t sz_Index;
    if (pc_Separator == NULL)
    {
        (void)fn_Copy(ac_Directory, sizeof(ac_Directory), ".");
    }
    else
    {
        size_t sz_Length = (size_t)(pc_Separator - pc_GatewayPath);
        if (sz_Length == 0U)
        {
            sz_Length = 1U;
        }
        if (sz_Length >= sizeof(ac_Directory))
        {
            return GW_ERR_RANGE;
        }
        (void)memcpy(ac_Directory, pc_GatewayPath, sz_Length);
        ac_Directory[sz_Length] = '\0';
    }
    {
        char *apc_PathFields[2] =
        {
            pst_Config->ac_MasterAclPath,
            pst_Config->ac_PidFilePath
        };
        size_t sz_FieldIndex;
        for (sz_FieldIndex = 0U; sz_FieldIndex < 2U; ++sz_FieldIndex)
        {
            char ac_Resolved[GW_PATH_LENGTH];
            int i32_Written;
            if (apc_PathFields[sz_FieldIndex][0] == '/')
            {
                continue;
            }
            i32_Written = snprintf(ac_Resolved, sizeof(ac_Resolved), "%s/%s",
                                   ac_Directory, apc_PathFields[sz_FieldIndex]);
            if ((i32_Written < 0) ||
                ((size_t)i32_Written >= sizeof(ac_Resolved)) ||
                (!fn_Copy(apc_PathFields[sz_FieldIndex], GW_PATH_LENGTH,
                          ac_Resolved)))
            {
                fn_SetError(pc_Error, sz_ErrorSize,
                            "gateway support path is too long");
                return GW_ERR_RANGE;
            }
        }
    }
    for (sz_Index = 0U; sz_Index < GW_COUPLER_COUNT; ++sz_Index)
    {
        char ac_Resolved[GW_PATH_LENGTH];
        int i32_Written;
        S_GW_ROUTE_CONFIG *pst_Route = &pst_Config->ast_Routes[sz_Index];
        if ((!pst_Route->b_Enabled) ||
            (pst_Route->ac_CouplerConfigPath[0] == '/'))
        {
            continue;
        }
        i32_Written = snprintf(ac_Resolved, sizeof(ac_Resolved), "%s/%s",
                               ac_Directory, pst_Route->ac_CouplerConfigPath);
        if ((i32_Written < 0) || ((size_t)i32_Written >= sizeof(ac_Resolved)) ||
            (!fn_Copy(pst_Route->ac_CouplerConfigPath,
                      sizeof(pst_Route->ac_CouplerConfigPath), ac_Resolved)))
        {
            fn_SetError(pc_Error, sz_ErrorSize, "coupler config path is too long");
            return GW_ERR_RANGE;
        }
    }
    return GW_OK;
}

E_GW_RESULT fn_GW_ConfigLoad(const char *pc_Path,
                             S_GW_CONFIG *pst_Config,
                             char *pc_Error,
                             size_t sz_ErrorSize)
{
    FILE *pst_File;
    char ac_Line[GW_CONFIG_LINE_LENGTH];
    char ac_Section[64] = "modbus";
    S_GW_ROUTE_CONFIG *pst_ActiveRoute = NULL;
    unsigned long ul_Line = 0UL;
    if ((pc_Path == NULL) || (pst_Config == NULL))
    {
        return GW_ERR_ARGUMENT;
    }
    fn_GW_ConfigDefaults(pst_Config);
    pst_File = fopen(pc_Path, "r");
    if (pst_File == NULL)
    {
        fn_SetError(pc_Error, sz_ErrorSize, strerror(errno));
        return GW_ERR_IO;
    }
    while (fgets(ac_Line, (int)sizeof(ac_Line), pst_File) != NULL)
    {
        char *pc_Text;
        char *pc_Equals;
        E_GW_RESULT e_Result;
        ++ul_Line;
        pc_Text = fn_Trim(ac_Line);
        if ((*pc_Text == '\0') || (*pc_Text == '#') || (*pc_Text == ';'))
        {
            continue;
        }
        if (*pc_Text == '[')
        {
            char *pc_Close = strchr(pc_Text, ']');
            if ((pc_Close == NULL) || (pc_Close[1] != '\0'))
            {
                (void)snprintf(pc_Error, sz_ErrorSize,
                               "line %lu: invalid section", ul_Line);
                (void)fclose(pst_File);
                return GW_ERR_CONFIG;
            }
            *pc_Close = '\0';
            if (!fn_Copy(ac_Section, sizeof(ac_Section), &pc_Text[1]))
            {
                (void)fclose(pst_File);
                return GW_ERR_RANGE;
            }
            if (strcmp(ac_Section, "modbus") == 0)
            {
                pst_ActiveRoute = NULL;
            }
            else
            {
                pst_ActiveRoute = fn_RouteBySection(pst_Config, ac_Section);
                if (pst_ActiveRoute == NULL)
                {
                    (void)snprintf(pc_Error, sz_ErrorSize,
                                   "line %lu: unknown section %s",
                                   ul_Line, ac_Section);
                    (void)fclose(pst_File);
                    return GW_ERR_CONFIG;
                }
            }
            continue;
        }
        pc_Equals = strchr(pc_Text, '=');
        if (pc_Equals == NULL)
        {
            (void)snprintf(pc_Error, sz_ErrorSize,
                           "line %lu: expected key=value", ul_Line);
            (void)fclose(pst_File);
            return GW_ERR_CONFIG;
        }
        *pc_Equals = '\0';
        e_Result = (pst_ActiveRoute == NULL) ?
                   fn_ApplyGlobal(pst_Config, fn_Trim(pc_Text),
                                  fn_Trim(pc_Equals + 1)) :
                   fn_ApplyRoute(pst_ActiveRoute, fn_Trim(pc_Text),
                                 fn_Trim(pc_Equals + 1));
        if (e_Result != GW_OK)
        {
            (void)snprintf(pc_Error, sz_ErrorSize,
                           "line %lu: invalid setting", ul_Line);
            (void)fclose(pst_File);
            return e_Result;
        }
    }
    if (ferror(pst_File) != 0)
    {
        (void)fclose(pst_File);
        fn_SetError(pc_Error, sz_ErrorSize, "configuration read failed");
        return GW_ERR_IO;
    }
    (void)fclose(pst_File);
    {
        E_GW_RESULT e_Result = fn_ResolvePaths(pc_Path, pst_Config,
                                               pc_Error, sz_ErrorSize);
        if (e_Result != GW_OK)
        {
            return e_Result;
        }
    }
    return fn_GW_ConfigValidate(pst_Config, pc_Error, sz_ErrorSize);
}

E_GW_RESULT fn_GW_ConfigValidate(const S_GW_CONFIG *pst_Config,
                                 char *pc_Error,
                                 size_t sz_ErrorSize)
{
    struct in_addr st_Address;
    size_t sz_Index;
    uint8_t ui8_Enabled = 0U;
    if (pst_Config == NULL)
    {
        return GW_ERR_ARGUMENT;
    }
    if (inet_pton(AF_INET, pst_Config->ac_BindIp, &st_Address) != 1)
    {
        fn_SetError(pc_Error, sz_ErrorSize, "invalid Modbus bind_ip");
        return GW_ERR_CONFIG;
    }
    if ((pst_Config->ui16_Port == 0U) ||
        ((pst_Config->ui16_SecondaryPort != 0U) &&
         (pst_Config->ui16_SecondaryPort == pst_Config->ui16_Port)) ||
        (pst_Config->ui8_MaxClients == 0U) ||
        (pst_Config->ui8_MaxClients > GW_MAX_MODBUS_CLIENTS) ||
        (pst_Config->ui8_MaxRequestsPerEventPerClient == 0U) ||
        (pst_Config->ui8_MaxRequestsPerEventPerClient > 64U) ||
        (pst_Config->ui32_IdleTimeoutMs < 100U) ||
        (pst_Config->ui32_MaxRequestsPerSecondPerClient == 0U) ||
        (pst_Config->ui32_InputStaleTimeoutMs < 10U))
    {
        fn_SetError(pc_Error, sz_ErrorSize, "invalid Modbus client limits");
        return GW_ERR_CONFIG;
    }
    if ((pst_Config->ac_MasterAclPath[0] == '\0') ||
        (access(pst_Config->ac_MasterAclPath, R_OK) != 0))
    {
        fn_SetError(pc_Error, sz_ErrorSize,
                    "master_acl_file is missing or unreadable");
        return GW_ERR_CONFIG;
    }
    if (pst_Config->ac_PidFilePath[0] == '\0')
    {
        fn_SetError(pc_Error, sz_ErrorSize, "pid_file is empty");
        return GW_ERR_CONFIG;
    }
    for (sz_Index = 0U; sz_Index < GW_COUPLER_COUNT; ++sz_Index)
    {
        const S_GW_ROUTE_CONFIG *pst_Route = &pst_Config->ast_Routes[sz_Index];
        if (!pst_Route->b_Enabled)
        {
            continue;
        }
        ++ui8_Enabled;
        if ((pst_Route->ac_Name[0] == '\0') ||
            (pst_Route->ac_CouplerConfigPath[0] == '\0') ||
            (pst_Route->ui8_UnitId == 0U) ||
            (pst_Route->ui32_WorkerPeriodMs == 0U) ||
            (access(pst_Route->ac_CouplerConfigPath, R_OK) != 0))
        {
            (void)snprintf(pc_Error, sz_ErrorSize,
                           "invalid route %s", pst_Route->ac_Name);
            return GW_ERR_CONFIG;
        }
    }
    if (ui8_Enabled == 0U)
    {
        fn_SetError(pc_Error, sz_ErrorSize, "no enabled EK9500 route");
        return GW_ERR_CONFIG;
    }
    return GW_OK;
}
