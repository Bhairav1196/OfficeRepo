/**
 * @file modbus_tcp_server.h
 * @brief Bounded epoll-based Modbus TCP server for simultaneous masters.
 * @author Prathamesh Chipkar
 */
#ifndef MODB_TCP_SERVER_H
#define MODB_TCP_SERVER_H

#include "modbus_tcp/modbus_tcp_protocol.h"

#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run the Modbus TCP listener until stop is requested.
 * @param[in] pst_Config Validated startup configuration.
 * @param[in,out] pst_RouteManager Owner of both EK9500 routes.
 * @param[in] pb_StopRequested Atomic shutdown request flag.
 * @param[in] pb_ReloadMastersRequested Atomic master-ACL reload request flag.
 * @return GW_OK for controlled shutdown; otherwise a deterministic error.
 */
E_GW_RESULT fn_MODB_ServerRun(const S_GW_CONFIG *pst_Config,
                              S_GW_ROUTE_MANAGER *pst_RouteManager,
                              atomic_bool *pb_StopRequested,
                              atomic_bool *pb_ReloadMastersRequested);

#ifdef __cplusplus
}
#endif

#endif
