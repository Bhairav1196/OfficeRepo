#ifndef W5500_PORT_STM32_H
#define W5500_PORT_STM32_H

#include <stddef.h>
#include <stdint.h>

/**
 * @file    w5500_port_stm32.h
 * @brief   STM32 HAL adaptation layer for the W5500 SPI driver.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Write one W5500 VDM frame over SPI1. @return 0 on success, negative on HAL failure. */
int32_t fn_W5500Port_WriteFrame(uint16_t ui16_Address, uint8_t ui8_Control,
                                const uint8_t *pui8_Data, uint16_t ui16_Length);
/** @brief Read one W5500 VDM frame over SPI1. @return 0 on success, negative on HAL failure. */
int32_t fn_W5500Port_ReadFrame(uint16_t ui16_Address, uint8_t ui8_Control,
                               uint8_t *pui8_Data, uint16_t ui16_Length);
/** @brief Perform the configured W5500 hardware reset sequence. */
void fn_W5500Port_HardwareReset(void);
/** @brief Return the STM32 HAL millisecond time base. */
uint32_t fn_W5500Port_Millis(void);
/** @brief Yield execution while W5500 polling loops wait for hardware state changes. */
void fn_W5500Port_Yield(void);

#ifdef __cplusplus
}
#endif

#endif /* W5500_PORT_STM32_H */
