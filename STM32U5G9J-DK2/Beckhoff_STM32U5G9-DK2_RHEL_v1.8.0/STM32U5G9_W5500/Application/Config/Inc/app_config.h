#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/**
 * @file    app_config.h
 * @brief   Compile-time capabilities and RTOS sizing policy.
 * @author  Prathamesh Chipkar
 * @date    11-Sep-2026
 * @version 2.0.0-alpha.9.0
 *
 * @details Per-signal names, modes, mappings, limits and enable/disable state
 *          belong in io_config.c. This file is reserved for build capabilities
 *          and fixed resource ceilings.
 */

#define APP_CFG_ENABLE_MODBUS_TCP           (1U)
#define APP_CFG_ENABLE_TOUCHGFX             (1U)
#define APP_CFG_ENABLE_DIAGNOSTICS          (1U)
#define APP_CFG_ENABLE_SUPERVISOR           (1U)
#define APP_CFG_ENABLE_IWDG                 (1U)

#define APP_CFG_BECKHOFF_UNIT_COUNT         (1U)
#define APP_CFG_MAX_SIGNAL_COUNT            (40U)
#define APP_CFG_MAX_DI_COUNT                (16U)
#define APP_CFG_MAX_DO_COUNT                (16U)
#define APP_CFG_MAX_AI_COUNT                (4U)
#define APP_CFG_MAX_AO_COUNT                (4U)
#define APP_CFG_MAX_CALIBRATION_POINTS      (12U)

#define APP_CFG_ANALOG_DECIMALS_DEFAULT     (3U)
#define APP_CFG_ANALOG_HMI_SLOTS            (4U)
#define APP_CFG_DIGITAL_HMI_SLOTS           (16U)

#define APP_CFG_COMMAND_QUEUE_DEPTH         (16U)
#define APP_CFG_TRANSPORT_THREAD_PRIORITY   (6U)
#define APP_CFG_DIAG_THREAD_PRIORITY        (14U)
#define APP_CFG_SUPERVISOR_THREAD_PRIORITY  (15U)
#define APP_CFG_TRANSPORT_STACK_BYTES       (4096U)
#define APP_CFG_DIAG_STACK_BYTES            (2048U)
#define APP_CFG_SUPERVISOR_STACK_BYTES      (2048U)

#define APP_CFG_DIAG_PERIOD_MS              (100U)
#define APP_CFG_SUPERVISOR_PERIOD_MS        (250U)
#define APP_CFG_LINK_DEBOUNCE_SAMPLES       (3U)
#define APP_CFG_TRANSPORT_FAILURE_RECONNECT (3U)
#define APP_CFG_PROTOCOL_FAILURE_RECONNECT  (3U)
#define APP_CFG_CONNECT_FAILURE_REINIT      (0U) /* Disabled: server refusal must never reset a healthy W5500. */

#endif /* APP_CONFIG_H */
