/**
 * @file modbus_master_acl.h
 * @brief Runtime-reloadable IPv4 allowlist for Modbus TCP masters.
 * @author Prathamesh Chipkar
 */
#ifndef MODBUS_MASTER_ACL_H
#define MODBUS_MASTER_ACL_H

#include "gateway/gateway_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Maximum number of independently controlled Modbus master hosts. */
#define MODB_MASTER_ACL_MAX_ENTRIES (32U)

/** @brief One permitted Modbus TCP master host. */
typedef struct
{
    char ac_Name[GW_NAME_LENGTH];
    char ac_IpAddress[GW_IPV4_LENGTH];
    uint32_t ui32_Ipv4NetworkOrder;
    uint8_t ui8_MaxConnections;
    uint32_t ui32_MaxRequestsPerSecond; /**< 0 uses the gateway-wide default. */
} S_MODB_MASTER_ACL_ENTRY;

/** @brief Complete in-memory master access-control table. */
typedef struct
{
    S_MODB_MASTER_ACL_ENTRY ast_Entries[MODB_MASTER_ACL_MAX_ENTRIES];
    uint8_t ui8_Count;
} S_MODB_MASTER_ACL;

/**
 * @brief Load and validate a master allowlist file.
 *
 * @param[in] pc_Path Path to the allowlist file.
 * @param[out] pst_Acl Destination table. It is replaced only on success.
 * @param[out] pc_Error Human-readable error destination.
 * @param[in] sz_ErrorSize Capacity of pc_Error in bytes.
 * @return GW_OK on success; otherwise a deterministic gateway error.
 * @note Blank files are valid and deny every remote Modbus master.
 * @note File format is: name|IPv4|max_connections[|max_requests_per_second].
 * @note A missing or zero request-rate field uses the gateway-wide default.
 */
E_GW_RESULT fn_MODB_MasterAclLoad(const char *pc_Path,
                                  S_MODB_MASTER_ACL *pst_Acl,
                                  char *pc_Error,
                                  size_t sz_ErrorSize);

/**
 * @brief Locate a permitted master by its source IPv4 address.
 *
 * @param[in] pst_Acl Access-control table.
 * @param[in] ui32_Ipv4NetworkOrder Source IPv4 address in network byte order.
 * @return Pointer to the matching immutable entry, or NULL when denied.
 */
const S_MODB_MASTER_ACL_ENTRY *fn_MODB_MasterAclFind(
    const S_MODB_MASTER_ACL *pst_Acl,
    uint32_t ui32_Ipv4NetworkOrder);

/**
 * @brief Resolve the effective request-rate ceiling for one permitted master.
 * @param[in] pst_Entry ACL entry; may be NULL.
 * @param[in] ui32_DefaultRate Gateway-wide fallback rate.
 * @return Custom ACL rate when non-zero, otherwise ui32_DefaultRate.
 */
uint32_t fn_MODB_MasterAclEffectiveRate(
    const S_MODB_MASTER_ACL_ENTRY *pst_Entry,
    uint32_t ui32_DefaultRate);

/**
 * @brief Return the zero-based index for an IPv4 entry.
 *
 * @param[in] pst_Acl Access-control table.
 * @param[in] ui32_Ipv4NetworkOrder Source IPv4 address in network byte order.
 * @param[out] pui8_Index Matching index when found.
 * @return true when found; otherwise false.
 */
bool fn_MODB_MasterAclFindIndex(const S_MODB_MASTER_ACL *pst_Acl,
                                uint32_t ui32_Ipv4NetworkOrder,
                                uint8_t *pui8_Index);

#ifdef __cplusplus
}
#endif

#endif
