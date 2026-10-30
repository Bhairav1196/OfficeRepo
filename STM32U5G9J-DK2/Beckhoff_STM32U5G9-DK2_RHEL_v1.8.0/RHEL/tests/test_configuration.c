/**
 * @file test_configuration.c
 * @brief Host test for the gateway configuration and combined process-image layout.
 * @author Prathamesh Chipkar
 *
 * The production master ACL is intentionally runtime-mutable. This test validates
 * that the configured ACL file can be parsed, but it does not require a fixed
 * entry count or a particular master name/IP. This allows the user to add,
 * remove, or clear Modbus TCP masters without making the build test fail.
 */
#include "ead/config.h"
#include "ead/layout.h"
#include "gateway/gateway_config.h"
#include "modbus_tcp/modbus_master_acl.h"

#include <stdio.h>
#include <string.h>

#ifndef TEST_CONFIG_DIR
#define TEST_CONFIG_DIR "config"
#endif

static int fn_CheckCoupler(const char *pc_File,
                           size_t sz_ExpectedInput,
                           size_t sz_ExpectedOutput,
                           size_t sz_ExpectedTerminals)
{
    ead_system_config_t st_Config;
    ead_layout_t st_Layout;
    char ac_Error[256];
    char ac_Path[512];
    ead_result_t e_Result;

    (void)snprintf(ac_Path, sizeof(ac_Path), "%s/%s", TEST_CONFIG_DIR, pc_File);
    (void)memset(ac_Error, 0, sizeof(ac_Error));
    e_Result = fn_EadConfigLoad(ac_Path, &st_Config, ac_Error, sizeof(ac_Error));
    if (e_Result != EAD_OK)
    {
        (void)fprintf(stderr, "%s: %s\n", pc_File, ac_Error);
        return 1;
    }
    e_Result = fn_EadBuildLayout(&st_Config, &st_Layout,
                                 ac_Error, sizeof(ac_Error));
    if (e_Result != EAD_OK)
    {
        (void)fprintf(stderr, "%s layout: %s\n", pc_File, ac_Error);
        return 1;
    }
    if ((st_Layout.input_size != sz_ExpectedInput) ||
        (st_Layout.output_size != sz_ExpectedOutput) ||
        (st_Layout.terminal_count != sz_ExpectedTerminals))
    {
        (void)fprintf(stderr,
                      "%s expected %zu/%zu/%zu, observed %zu/%zu/%zu\n",
                      pc_File, sz_ExpectedInput, sz_ExpectedOutput,
                      sz_ExpectedTerminals, st_Layout.input_size,
                      st_Layout.output_size, st_Layout.terminal_count);
        return 1;
    }
    return 0;
}

int main(void)
{
    S_GW_CONFIG st_Gateway;
    S_MODB_MASTER_ACL st_Acl;
    char ac_Error[256];
    char ac_Path[512];
    E_GW_RESULT e_Result;

    (void)memset(ac_Error, 0, sizeof(ac_Error));
    (void)snprintf(ac_Path, sizeof(ac_Path), "%s/gateway.conf", TEST_CONFIG_DIR);
    e_Result = fn_GW_ConfigLoad(ac_Path, &st_Gateway,
                                ac_Error, sizeof(ac_Error));
    if (e_Result != GW_OK)
    {
        (void)fprintf(stderr, "gateway.conf: %s\n", ac_Error);
        return 1;
    }

    if ((st_Gateway.ast_Routes[0].ui8_UnitId != 1U) ||
        (st_Gateway.ui8_MaxRequestsPerEventPerClient != 16U) ||
        (st_Gateway.ui16_SecondaryPort != 502U) ||
        (st_Gateway.ac_MasterAclPath[0] == '\0') ||
        (st_Gateway.ac_PidFilePath[0] == '\0'))
    {
        (void)fprintf(stderr, "gateway.conf: expected fixed route/client settings were not loaded\n");
        return 1;
    }

    (void)memset(&st_Acl, 0, sizeof(st_Acl));
    (void)memset(ac_Error, 0, sizeof(ac_Error));
    e_Result = fn_MODB_MasterAclLoad(st_Gateway.ac_MasterAclPath, &st_Acl,
                                    ac_Error, sizeof(ac_Error));
    if (e_Result != GW_OK)
    {
        (void)fprintf(stderr, "masters.conf: %s\n", ac_Error);
        return 1;
    }

    /*
     * No assertion is made on st_Acl.ui8_Count. An empty ACL is valid and means
     * that no remote Modbus TCP master is currently permitted. Likewise, one or
     * more entries are valid after masterctl.sh add operations.
     */

    if (fn_CheckCoupler("coupler.conf", 30U, 22U, 4U) != 0)
    {
        return 1;
    }
    return 0;
}
