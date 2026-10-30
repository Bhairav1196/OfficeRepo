#ifndef MAIN_H
#define MAIN_H
#include "stm32u5xx_hal.h"
extern GPIO_TypeDef g_gpio_b;
#define W5500_CS_GPIO_Port (&g_gpio_b)
#define W5500_CS_Pin ((uint16_t)0x0001U)
#define W5500_RST_GPIO_Port (&g_gpio_b)
#define W5500_RST_Pin ((uint16_t)0x0002U)
#endif
