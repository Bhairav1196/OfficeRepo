/**
 * @file    w5500_port_stm32.c
 * @brief   STM32U5 HAL adaptation for W5500 variable-length SPI mode.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#include "w5500_port_stm32.h"
#include "main.h"
#include "tx_api.h"

#include <stdint.h>

extern SPI_HandleTypeDef hspi1;

#if !defined(W5500_CS_Pin) && defined(WS5500_CS_Pin)
#define W5500_CS_Pin          WS5500_CS_Pin
#define W5500_CS_GPIO_Port    WS5500_CS_GPIO_Port
#endif

#if !defined(W5500_RST_Pin) && defined(W5500_RESETn_Pin)
#define W5500_RST_Pin         W5500_RESETn_Pin
#define W5500_RST_GPIO_Port   W5500_RESETn_GPIO_Port
#endif

#define W5500_HAL_TIMEOUT_MS       (100U)
#define W5500_RESET_ASSERT_MS      (2U)
#define W5500_RESET_RELEASE_MS     (50U)

static void fn_W5500Port_Select(void)
{
    HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_RESET);
}

static void fn_W5500Port_Deselect(void)
{
    HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_SET);
}

void fn_W5500Port_HardwareReset(void)
{
#if defined(W5500_RST_Pin) && defined(W5500_RST_GPIO_Port)
    fn_W5500Port_Deselect();
    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(W5500_RESET_ASSERT_MS);
    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(W5500_RESET_RELEASE_MS);
#else
    /* Software reset remains authoritative when hardware reset is unwired. */
#endif
}

int32_t fn_W5500Port_WriteFrame(uint16_t ui16_Address, uint8_t ui8_Control,
                                const uint8_t *pui8_Data, uint16_t ui16_Length)
{
    uint8_t aui8_Header[3];
    HAL_StatusTypeDef e_Status;

    if ((pui8_Data == NULL) && (ui16_Length != 0U))
    {
        return -1;
    }

    aui8_Header[0] = (uint8_t)(ui16_Address >> 8);
    aui8_Header[1] = (uint8_t)(ui16_Address & 0xFFU);
    aui8_Header[2] = ui8_Control;

    fn_W5500Port_Select();
    e_Status = HAL_SPI_Transmit(&hspi1, aui8_Header, 3U, W5500_HAL_TIMEOUT_MS);
    if ((e_Status == HAL_OK) && (ui16_Length != 0U))
    {
        e_Status = HAL_SPI_Transmit(&hspi1, (uint8_t *)(uintptr_t)pui8_Data,
                                   ui16_Length, W5500_HAL_TIMEOUT_MS);
    }
    fn_W5500Port_Deselect();
    return (e_Status == HAL_OK) ? 0 : -1;
}

int32_t fn_W5500Port_ReadFrame(uint16_t ui16_Address, uint8_t ui8_Control,
                               uint8_t *pui8_Data, uint16_t ui16_Length)
{
    uint8_t aui8_Header[3];
    HAL_StatusTypeDef e_Status;

    if ((pui8_Data == NULL) && (ui16_Length != 0U))
    {
        return -1;
    }

    aui8_Header[0] = (uint8_t)(ui16_Address >> 8);
    aui8_Header[1] = (uint8_t)(ui16_Address & 0xFFU);
    aui8_Header[2] = ui8_Control;

    fn_W5500Port_Select();
    e_Status = HAL_SPI_Transmit(&hspi1, aui8_Header, 3U, W5500_HAL_TIMEOUT_MS);
    if ((e_Status == HAL_OK) && (ui16_Length != 0U))
    {
        e_Status = HAL_SPI_Receive(&hspi1, pui8_Data, ui16_Length, W5500_HAL_TIMEOUT_MS);
    }
    fn_W5500Port_Deselect();
    return (e_Status == HAL_OK) ? 0 : -1;
}

uint32_t fn_W5500Port_Millis(void)
{
    return HAL_GetTick();
}

void fn_W5500Port_Yield(void)
{
    (void)tx_thread_relinquish();
}
