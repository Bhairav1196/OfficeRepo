/**
 * @file    app_watchdog.c
 * @brief   STM32U5 independent watchdog implementation without a HAL runtime dependency.
 * @author  Prathamesh Chipkar
 * @date    11-Sep-2026
 * @version 2.0.0-alpha.9.0
 *
 * @details The LSI-driven IWDG uses prescaler /64 and reload 1000, providing
 *          approximately two seconds at a nominal 32 kHz LSI. The watchdog is
 *          deliberately not refreshed from interrupts or individual workers.
 */

#include "app_watchdog.h"
#include "stm32u5xx.h"

#include <stdint.h>

#define APP_IWDG_KEY_ENABLE_ACCESS  (0x5555U)
#define APP_IWDG_KEY_START          (0xCCCCU)
#define APP_IWDG_KEY_REFRESH        (0xAAAAU)
#define APP_IWDG_RELOAD             (1000U)
#define APP_IWDG_UPDATE_TIMEOUT     (1000000UL)

#if defined(APP_WATCHDOG_HOST_TEST)
IWDG_TypeDef g_test_iwdg;
#endif
static bool s_started;

void fn_AppWatchdog_Init(void)
{
    uint32_t timeout = APP_IWDG_UPDATE_TIMEOUT;

    IWDG->KR = APP_IWDG_KEY_ENABLE_ACCESS;
    IWDG->PR = IWDG_PR_PR_2; /* Encoding 4: divide nominal 32 kHz LSI by 64. */
    IWDG->RLR = APP_IWDG_RELOAD;
    IWDG->WINR = IWDG_WINR_WIN;
    while (((IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU | IWDG_SR_WVU)) != 0U) && (timeout > 0U))
    {
        --timeout;
    }
    IWDG->KR = APP_IWDG_KEY_START;
    IWDG->KR = APP_IWDG_KEY_REFRESH;
    s_started = true;
}

void fn_AppWatchdog_Refresh(void)
{
    if (s_started)
    {
        IWDG->KR = APP_IWDG_KEY_REFRESH;
    }
}

bool fn_AppWatchdog_IsStarted(void)
{
    return s_started;
}
