#ifndef STM32U5XX_H
#define STM32U5XX_H
#include <stdint.h>
typedef struct { volatile uint32_t KR, PR, RLR, SR, WINR, EWCR; } IWDG_TypeDef;
extern IWDG_TypeDef g_test_iwdg;
#define IWDG (&g_test_iwdg)
#define IWDG_PR_PR_2 (4UL)
#define IWDG_WINR_WIN (0xFFFUL)
#define IWDG_SR_PVU (1UL)
#define IWDG_SR_RVU (2UL)
#define IWDG_SR_WVU (4UL)
#endif
