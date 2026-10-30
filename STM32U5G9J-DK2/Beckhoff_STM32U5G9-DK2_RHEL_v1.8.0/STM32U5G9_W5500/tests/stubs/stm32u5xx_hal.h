#ifndef STM32U5XX_HAL_H
#define STM32U5XX_HAL_H
#include <stdint.h>
typedef struct { uint32_t dummy; } SPI_HandleTypeDef;
typedef struct { uint32_t dummy; } GPIO_TypeDef;
typedef enum { HAL_OK=0, HAL_ERROR=1 } HAL_StatusTypeDef;
typedef enum { GPIO_PIN_RESET=0, GPIO_PIN_SET=1 } GPIO_PinState;
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t);
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef*, uint8_t*, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef*, uint8_t*, uint16_t, uint32_t);
void HAL_GPIO_WritePin(GPIO_TypeDef*, uint16_t, GPIO_PinState);
#endif
