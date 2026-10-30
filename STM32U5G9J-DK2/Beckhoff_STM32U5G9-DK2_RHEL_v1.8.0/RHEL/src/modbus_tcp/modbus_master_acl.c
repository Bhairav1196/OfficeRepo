/**
 * @file modbus_master_acl.c
 * @brief Bounded parser for the runtime Modbus master IPv4 allowlist.
 * @author Prathamesh Chipkar
 */
#define _POSIX_C_SOURCE 200809L
#include "modbus_tcp/modbus_master_acl.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODB_MASTER_ACL_LINE_LENGTH (256U)

/** @brief Store a bounded error string. */
static void fn_MODB_AclSetError(char *pc_Error,
                                const size_t sz_ErrorSize,
                                const char *pc_Text)
{
    if ((pc_Error != NULL) && (sz_ErrorSize > 0U))
    {
        (void)snprintf(pc_Error, sz_ErrorSize, "%s",
                       (pc_Text != NULL) ? pc_Text : "master ACL error");
    }
}

/** @brief Trim leading and trailing ASCII whitespace in place. */
static char *fn_MODB_AclTrim(char *pc_Text)
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

/** @brief Copy one bounded field. */
static bool fn_MODB_AclCopy(char *pc_Destination,
                            const size_t sz_Capacity,
                            const char *pc_Source)
{
    const int i32_Written = snprintf(pc_Destination, sz_Capacity, "%s", pc_Source);
    return (i32_Written >= 0) && ((size_t)i32_Written < sz_Capacity);
}

/** @brief Accept ordinary unicast IPv4 addresses, including loopback for tests. */
static bool fn_MODB_AclIsUnicast(const uint32_t ui32_NetworkOrder)
{
    const uint32_t ui32_HostOrder = ntohl(ui32_NetworkOrder);
    const uint8_t ui8_FirstOctet = (uint8_t)(ui32_HostOrder >> 24U);

    return (ui32_HostOrder != UINT32_C(0)) &&
           (ui32_HostOrder != UINT32_C(0xFFFFFFFF)) &&
           (ui8_FirstOctet < 224U);
}

/** @brief Validate a printable master label without separators. */
static bool fn_MODB_AclValidName(const char *pc_Name)
{
    size_t sz_Index;
    const size_t sz_Length = (pc_Name != NULL) ? strlen(pc_Name) : 0U;

    if ((sz_Length == 0U) || (sz_Length >= GW_NAME_LENGTH))
    {
        return false;
    }
    for (sz_Index = 0U; sz_Index < sz_Length; ++sz_Index)
    {
        const unsigned char ui8_Character = (unsigned char)pc_Name[sz_Index];
        if ((isalnum(ui8_Character) == 0) && (ui8_Character != '_') &&
            (ui8_Character != '-') && (ui8_Character != '.'))
        {
            return false;
        }
    }
    return true;
}

/** @brief Parse a bounded unsigned connection count. */
static bool fn_MODB_AclParseConnections(const char *pc_Text,
                                        uint8_t *pui8_Value)
{
    char *pc_End = NULL;
    unsigned long ul_Value;

    if ((pc_Text == NULL) || (pui8_Value == NULL))
    {
        return false;
    }
    errno = 0;
    ul_Value = strtoul(pc_Text, &pc_End, 10);
    if ((errno != 0) || (pc_End == pc_Text) || (*pc_End != '\0') ||
        (ul_Value == 0UL) || (ul_Value > GW_MAX_MODBUS_CLIENTS))
    {
        return false;
    }
    *pui8_Value = (uint8_t)ul_Value;
    return true;
}

/** @brief Parse an optional per-master request-rate ceiling. Zero means default. */
static bool fn_MODB_AclParseRate(const char *pc_Text, uint32_t *pui32_Value)
{
    char *pc_End = NULL;
    unsigned long ul_Value;

    if ((pc_Text == NULL) || (pui32_Value == NULL))
    {
        return false;
    }
    errno = 0;
    ul_Value = strtoul(pc_Text, &pc_End, 10);
    if ((errno != 0) || (pc_End == pc_Text) || (*pc_End != '\0') ||
        (ul_Value > UINT32_MAX))
    {
        return false;
    }
    *pui32_Value = (uint32_t)ul_Value;
    return true;
}

bool fn_MODB_MasterAclFindIndex(const S_MODB_MASTER_ACL *pst_Acl,
                                const uint32_t ui32_Ipv4NetworkOrder,
                                uint8_t *pui8_Index)
{
    uint8_t ui8_Index;

    if ((pst_Acl == NULL) || (pui8_Index == NULL))
    {
        return false;
    }
    for (ui8_Index = 0U; ui8_Index < pst_Acl->ui8_Count; ++ui8_Index)
    {
        if (pst_Acl->ast_Entries[ui8_Index].ui32_Ipv4NetworkOrder ==
            ui32_Ipv4NetworkOrder)
        {
            *pui8_Index = ui8_Index;
            return true;
        }
    }
    return false;
}

const S_MODB_MASTER_ACL_ENTRY *fn_MODB_MasterAclFind(
    const S_MODB_MASTER_ACL *pst_Acl,
    const uint32_t ui32_Ipv4NetworkOrder)
{
    uint8_t ui8_Index = 0U;

    if (!fn_MODB_MasterAclFindIndex(pst_Acl, ui32_Ipv4NetworkOrder,
                                    &ui8_Index))
    {
        return NULL;
    }
    return &pst_Acl->ast_Entries[ui8_Index];
}

uint32_t fn_MODB_MasterAclEffectiveRate(
    const S_MODB_MASTER_ACL_ENTRY *pst_Entry,
    const uint32_t ui32_DefaultRate)
{
    if ((pst_Entry != NULL) && (pst_Entry->ui32_MaxRequestsPerSecond != 0U))
    {
        return pst_Entry->ui32_MaxRequestsPerSecond;
    }
    return ui32_DefaultRate;
}

E_GW_RESULT fn_MODB_MasterAclLoad(const char *pc_Path,
                                  S_MODB_MASTER_ACL *pst_Acl,
                                  char *pc_Error,
                                  const size_t sz_ErrorSize)
{
    FILE *pst_File;
    S_MODB_MASTER_ACL st_Candidate;
    char ac_Line[MODB_MASTER_ACL_LINE_LENGTH];
    unsigned long ul_LineNumber = 0UL;

    if ((pc_Path == NULL) || (pst_Acl == NULL))
    {
        return GW_ERR_ARGUMENT;
    }
    (void)memset(&st_Candidate, 0, sizeof(st_Candidate));
    pst_File = fopen(pc_Path, "r");
    if (pst_File == NULL)
    {
        fn_MODB_AclSetError(pc_Error, sz_ErrorSize, strerror(errno));
        return GW_ERR_IO;
    }

    while (fgets(ac_Line, (int)sizeof(ac_Line), pst_File) != NULL)
    {
        char *pc_Text;
        char *pc_Name;
        char *pc_Ip;
        char *pc_Max;
        char *pc_Rate = NULL;
        char *pc_FirstSeparator;
        char *pc_SecondSeparator;
        char *pc_ThirdSeparator;
        struct in_addr st_Address;
        uint8_t ui8_MaxConnections = 0U;
        uint32_t ui32_MaxRequestsPerSecond = 0U;
        uint8_t ui8_Index;

        ++ul_LineNumber;
        if ((strchr(ac_Line, '\n') == NULL) && (feof(pst_File) == 0))
        {
            (void)snprintf(pc_Error, sz_ErrorSize,
                           "line %lu exceeds %u bytes", ul_LineNumber,
                           (unsigned int)(MODB_MASTER_ACL_LINE_LENGTH - 1U));
            (void)fclose(pst_File);
            return GW_ERR_RANGE;
        }
        pc_Text = fn_MODB_AclTrim(ac_Line);
        if ((*pc_Text == '\0') || (*pc_Text == '#') || (*pc_Text == ';'))
        {
            continue;
        }
        if (st_Candidate.ui8_Count >= MODB_MASTER_ACL_MAX_ENTRIES)
        {
            fn_MODB_AclSetError(pc_Error, sz_ErrorSize,
                               "too many master ACL entries");
            (void)fclose(pst_File);
            return GW_ERR_RANGE;
        }

        pc_FirstSeparator = strchr(pc_Text, '|');
        pc_SecondSeparator = (pc_FirstSeparator != NULL) ?
                             strchr(pc_FirstSeparator + 1, '|') : NULL;
        pc_ThirdSeparator = (pc_SecondSeparator != NULL) ?
                            strchr(pc_SecondSeparator + 1, '|') : NULL;
        if ((pc_FirstSeparator == NULL) || (pc_SecondSeparator == NULL) ||
            ((pc_ThirdSeparator != NULL) &&
             (strchr(pc_ThirdSeparator + 1, '|') != NULL)))
        {
            (void)snprintf(pc_Error, sz_ErrorSize,
                           "line %lu: expected name|IPv4|max_connections[|max_requests_per_second]",
                           ul_LineNumber);
            (void)fclose(pst_File);
            return GW_ERR_CONFIG;
        }
        *pc_FirstSeparator = '\0';
        *pc_SecondSeparator = '\0';
        if (pc_ThirdSeparator != NULL)
        {
            *pc_ThirdSeparator = '\0';
            pc_Rate = fn_MODB_AclTrim(pc_ThirdSeparator + 1);
        }
        pc_Name = fn_MODB_AclTrim(pc_Text);
        pc_Ip = fn_MODB_AclTrim(pc_FirstSeparator + 1);
        pc_Max = fn_MODB_AclTrim(pc_SecondSeparator + 1);

        if (!fn_MODB_AclValidName(pc_Name) ||
            (inet_pton(AF_INET, pc_Ip, &st_Address) != 1) ||
            (!fn_MODB_AclIsUnicast(st_Address.s_addr)) ||
            (!fn_MODB_AclParseConnections(pc_Max, &ui8_MaxConnections)) ||
            ((pc_Rate != NULL) &&
             (!fn_MODB_AclParseRate(pc_Rate, &ui32_MaxRequestsPerSecond))))
        {
            (void)snprintf(pc_Error, sz_ErrorSize,
                           "line %lu: invalid master entry", ul_LineNumber);
            (void)fclose(pst_File);
            return GW_ERR_CONFIG;
        }
        for (ui8_Index = 0U; ui8_Index < st_Candidate.ui8_Count; ++ui8_Index)
        {
            if ((strcmp(st_Candidate.ast_Entries[ui8_Index].ac_Name,
                        pc_Name) == 0) ||
                (st_Candidate.ast_Entries[ui8_Index].ui32_Ipv4NetworkOrder ==
                 st_Address.s_addr))
            {
                (void)snprintf(pc_Error, sz_ErrorSize,
                               "line %lu: duplicate master name or IPv4",
                               ul_LineNumber);
                (void)fclose(pst_File);
                return GW_ERR_CONFIG;
            }
        }

        {
            S_MODB_MASTER_ACL_ENTRY *pst_Entry =
                &st_Candidate.ast_Entries[st_Candidate.ui8_Count];
            if ((!fn_MODB_AclCopy(pst_Entry->ac_Name,
                                  sizeof(pst_Entry->ac_Name), pc_Name)) ||
                (!fn_MODB_AclCopy(pst_Entry->ac_IpAddress,
                                  sizeof(pst_Entry->ac_IpAddress), pc_Ip)))
            {
                fn_MODB_AclSetError(pc_Error, sz_ErrorSize,
                                   "master ACL field is too long");
                (void)fclose(pst_File);
                return GW_ERR_RANGE;
            }
            pst_Entry->ui32_Ipv4NetworkOrder = st_Address.s_addr;
            pst_Entry->ui8_MaxConnections = ui8_MaxConnections;
            pst_Entry->ui32_MaxRequestsPerSecond = ui32_MaxRequestsPerSecond;
            ++st_Candidate.ui8_Count;
        }
    }
    if (ferror(pst_File) != 0)
    {
        fn_MODB_AclSetError(pc_Error, sz_ErrorSize, "master ACL read failed");
        (void)fclose(pst_File);
        return GW_ERR_IO;
    }
    (void)fclose(pst_File);
    *pst_Acl = st_Candidate;
    fn_MODB_AclSetError(pc_Error, sz_ErrorSize, "");
    return GW_OK;
}
