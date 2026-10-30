#ifndef MODBUS_CONFIG_H
#define MODBUS_CONFIG_H

/**
 * @file    modbus_config.h
 * @brief   Fixed RHEL gateway compatibility contract and cyclic block policy.
 * @author  Prathamesh Chipkar
 * @date    11-Sep-2026
 * @version 2.0.0-alpha.9.0
 *
 * @details
 * These are zero-based Modbus PDU addresses for the proven RHEL gateway
 * contract. Per-signal names/modes/scaling are configured separately in
 * io_config.c and must remain consistent with these block locations.
 */

#define MODBUS_CFG_UNIT                    (1U)

#define MODBUS_CFG_ADDR_DO                 (0U)
#define MODBUS_CFG_QTY_DO                  (16U)
#define MODBUS_CFG_ADDR_DI                 (0U)
#define MODBUS_CFG_QTY_DI                  (16U)

#define MODBUS_CFG_ADDR_AI_RAW             (0U)
#define MODBUS_CFG_ADDR_AI_ENG             (1024U)
#define MODBUS_CFG_ADDR_AI_STATUS          (2048U)
#define MODBUS_CFG_QTY_AI                  (4U)

#define MODBUS_CFG_ADDR_AO                 (0U)
#define MODBUS_CFG_ADDR_AO_STATUS          (3072U)
#define MODBUS_CFG_QTY_AO                  (4U)

#endif /* MODBUS_CONFIG_H */
