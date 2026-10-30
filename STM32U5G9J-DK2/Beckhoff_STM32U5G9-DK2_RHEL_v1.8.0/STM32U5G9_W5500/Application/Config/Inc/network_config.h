#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

/**
 * @file    network_config.h
 * @brief   W5500 and RHEL Modbus TCP network configuration.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#define NET_CFG_LOCAL_MAC_0              (0x02U)
#define NET_CFG_LOCAL_MAC_1              (0x55U)
#define NET_CFG_LOCAL_MAC_2              (0x00U)
#define NET_CFG_LOCAL_MAC_3              (0x00U)
#define NET_CFG_LOCAL_MAC_4              (0x01U)
#define NET_CFG_LOCAL_MAC_5              (0x60U)

#define NET_CFG_LOCAL_IP_0               (192U)
#define NET_CFG_LOCAL_IP_1               (168U)
#define NET_CFG_LOCAL_IP_2               (1U)
#define NET_CFG_LOCAL_IP_3               (60U)

#define NET_CFG_SUBNET_0                 (255U)
#define NET_CFG_SUBNET_1                 (255U)
#define NET_CFG_SUBNET_2                 (255U)
#define NET_CFG_SUBNET_3                 (0U)

#define NET_CFG_GATEWAY_0                (192U)
#define NET_CFG_GATEWAY_1                (168U)
#define NET_CFG_GATEWAY_2                (1U)
#define NET_CFG_GATEWAY_3                (254U)

/** @brief Matched RHEL v1.8.0 gateway endpoint used by this release pair. */
#define NET_CFG_RHEL_IP_0                (192U)
#define NET_CFG_RHEL_IP_1                (168U)
#define NET_CFG_RHEL_IP_2                (1U)
#define NET_CFG_RHEL_IP_3                (7U)
#define NET_CFG_RHEL_TCP_PORT            (502U)
#define NET_CFG_RHEL_EXPECTED_RATE_LIMIT (500U)

#define NET_CFG_POLL_PERIOD_MS           (20U)
#define NET_CFG_DIAGNOSTIC_DIVIDER       (10U)
#define NET_CFG_W5500_INIT_RETRY_MS      (1000U)
#define NET_CFG_RECONNECT_BACKOFF_MS     (1000U)
#define NET_CFG_CONNECT_TIMEOUT_MS       (1500U)
#define NET_CFG_TRANSACTION_TIMEOUT_MS   (250U)

#define NET_CFG_SOURCE_PORT_BASE         (49152U)
#define NET_CFG_SOURCE_PORT_COUNT        (1024U)

#endif /* NETWORK_CONFIG_H */
