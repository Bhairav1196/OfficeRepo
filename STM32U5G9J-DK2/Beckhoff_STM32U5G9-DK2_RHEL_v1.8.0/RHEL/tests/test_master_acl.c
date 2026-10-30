/**
 * @file test_master_acl.c
 * @brief Unit tests for the bounded Modbus master allowlist parser.
 * @author Prathamesh Chipkar
 */
#define _POSIX_C_SOURCE 200809L
#include "modbus_tcp/modbus_master_acl.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    return 1; } } while (0)

/** @brief Replace a test file with exact text. */
static bool fn_WriteText(const char *pc_Path, const char *pc_Text)
{
    const int i32_File = open(pc_Path, O_WRONLY | O_TRUNC | O_CLOEXEC);
    const size_t sz_Length = strlen(pc_Text);
    ssize_t sz_Written;

    if (i32_File < 0)
    {
        return false;
    }
    sz_Written = write(i32_File, pc_Text, sz_Length);
    if (close(i32_File) != 0)
    {
        return false;
    }
    return sz_Written == (ssize_t)sz_Length;
}

int main(void)
{
    char ac_Path[] = "/tmp/ek9500-master-acl-XXXXXX";
    char ac_Error[256];
    S_MODB_MASTER_ACL st_Acl;
    struct in_addr st_Address;
    int i32_File = mkstemp(ac_Path);

    TEST_CHECK(i32_File >= 0);
    TEST_CHECK(close(i32_File) == 0);

    TEST_CHECK(fn_WriteText(ac_Path,
        "# valid list\n"
        "ModScan_PC|192.168.1.10|8\n"
        "Beijer_HMI|192.168.1.50|2\n"
        "STM32F407_LAN8720|192.168.1.19|1|500\n"));
    TEST_CHECK(fn_MODB_MasterAclLoad(ac_Path, &st_Acl,
                                     ac_Error, sizeof(ac_Error)) == GW_OK);
    TEST_CHECK(st_Acl.ui8_Count == 3U);
    TEST_CHECK(strcmp(st_Acl.ast_Entries[0].ac_Name, "ModScan_PC") == 0);
    TEST_CHECK(st_Acl.ast_Entries[0].ui8_MaxConnections == 8U);
    TEST_CHECK(st_Acl.ast_Entries[0].ui32_MaxRequestsPerSecond == 0U);
    TEST_CHECK(fn_MODB_MasterAclEffectiveRate(&st_Acl.ast_Entries[0], 200U) == 200U);
    TEST_CHECK(st_Acl.ast_Entries[2].ui32_MaxRequestsPerSecond == 500U);
    TEST_CHECK(fn_MODB_MasterAclEffectiveRate(&st_Acl.ast_Entries[2], 200U) == 500U);
    TEST_CHECK(inet_pton(AF_INET, "192.168.1.50", &st_Address) == 1);
    TEST_CHECK(fn_MODB_MasterAclFind(&st_Acl, st_Address.s_addr) ==
               &st_Acl.ast_Entries[1]);

    TEST_CHECK(fn_WriteText(ac_Path,
        "A|192.168.1.10|1\n"
        "B|192.168.1.10|1\n"));
    TEST_CHECK(fn_MODB_MasterAclLoad(ac_Path, &st_Acl,
                                     ac_Error, sizeof(ac_Error)) != GW_OK);

    TEST_CHECK(fn_WriteText(ac_Path, "Bad Name|192.168.1.70|1\n"));
    TEST_CHECK(fn_MODB_MasterAclLoad(ac_Path, &st_Acl,
                                     ac_Error, sizeof(ac_Error)) != GW_OK);

    TEST_CHECK(fn_WriteText(ac_Path, "STM32|192.168.1.19|1|not-a-rate\n"));
    TEST_CHECK(fn_MODB_MasterAclLoad(ac_Path, &st_Acl,
                                     ac_Error, sizeof(ac_Error)) != GW_OK);

    TEST_CHECK(fn_WriteText(ac_Path, "STM32|192.168.1.19|1|500|extra\n"));
    TEST_CHECK(fn_MODB_MasterAclLoad(ac_Path, &st_Acl,
                                     ac_Error, sizeof(ac_Error)) != GW_OK);

    TEST_CHECK(fn_WriteText(ac_Path, ""));
    TEST_CHECK(fn_MODB_MasterAclLoad(ac_Path, &st_Acl,
                                     ac_Error, sizeof(ac_Error)) == GW_OK);
    TEST_CHECK(st_Acl.ui8_Count == 0U);

    TEST_CHECK(unlink(ac_Path) == 0);
    (void)puts("Master ACL parser test passed");
    return 0;
}
