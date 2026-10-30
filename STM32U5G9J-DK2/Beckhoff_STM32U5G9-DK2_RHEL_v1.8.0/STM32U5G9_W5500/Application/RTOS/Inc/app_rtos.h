#ifndef APP_RTOS_H
#define APP_RTOS_H

#include "tx_api.h"

/**
 * @file    app_rtos.h
 * @brief   Application-owned ThreadX task creation entry point.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Create application mutex/queue and the transport, diagnostics and supervisor threads. */
UINT fn_AppRtos_Create(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_RTOS_H */
