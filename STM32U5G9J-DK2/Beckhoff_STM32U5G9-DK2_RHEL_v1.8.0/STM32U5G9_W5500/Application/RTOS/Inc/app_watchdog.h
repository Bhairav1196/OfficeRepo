#ifndef APP_WATCHDOG_H
#define APP_WATCHDOG_H

/**
 * @file    app_watchdog.h
 * @brief   Independent watchdog interface for task-health supervision.
 * @author  Prathamesh Chipkar
 * @date    11-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Starts the hardware IWDG. It cannot be stopped without a reset. */
void fn_AppWatchdog_Init(void);

/** @brief Reloads the IWDG only when the supervisor proves all required tasks progressed. */
void fn_AppWatchdog_Refresh(void);

/** @brief Returns true after the IWDG has been started. */
bool fn_AppWatchdog_IsStarted(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_WATCHDOG_H */
