/**
 * @file    test_modbus_tcp.c
 * @brief   Host regression tests for the Modbus TCP client.
 * @author  Prathamesh Chipkar
 * @date    09-Sep-2026
 * @version 2.0.0-alpha.9.0
 */
#include "modbus_tcp_client.h"
#include "w5500_driver.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t g_tx[260];
static uint16_t g_txLen;
static uint8_t g_rx[260];
static uint16_t g_rxLen;
static uint16_t g_rxPos;

static void fn_Put16(uint8_t *pui8_Data, uint16_t ui16_Value)
{
    pui8_Data[0] = (uint8_t)(ui16_Value >> 8);
    pui8_Data[1] = (uint8_t)ui16_Value;
}

static void fn_PrepareResponse(uint16_t ui16_Tid, uint8_t ui8_Unit,
                               const uint8_t *pui8_Pdu, uint16_t ui16_PduLength)
{
    assert((uint16_t)(7U + ui16_PduLength) <= sizeof(g_rx));
    fn_Put16(&g_rx[0], ui16_Tid);
    fn_Put16(&g_rx[2], 0U);
    fn_Put16(&g_rx[4], (uint16_t)(ui16_PduLength + 1U));
    g_rx[6] = ui8_Unit;
    memcpy(&g_rx[7], pui8_Pdu, ui16_PduLength);
    g_rxLen = (uint16_t)(7U + ui16_PduLength);
    g_rxPos = 0U;
    g_txLen = 0U;
}

static void fn_AppendResponse(uint16_t ui16_Tid, uint8_t ui8_Unit,
                              const uint8_t *pui8_Pdu, uint16_t ui16_PduLength)
{
    const uint16_t ui16_Start = g_rxLen;
    assert((uint32_t)ui16_Start + 7U + ui16_PduLength <= sizeof(g_rx));
    fn_Put16(&g_rx[ui16_Start], ui16_Tid);
    fn_Put16(&g_rx[ui16_Start + 2U], 0U);
    fn_Put16(&g_rx[ui16_Start + 4U], (uint16_t)(ui16_PduLength + 1U));
    g_rx[ui16_Start + 6U] = ui8_Unit;
    memcpy(&g_rx[ui16_Start + 7U], pui8_Pdu, ui16_PduLength);
    g_rxLen = (uint16_t)(ui16_Start + 7U + ui16_PduLength);
}

E_W5500_Result fn_W5500_Send(const uint8_t *pui8_Data, uint16_t ui16_Length, uint32_t ui32_TimeoutMs)
{
    (void)ui32_TimeoutMs;
    assert(pui8_Data != NULL);
    assert(ui16_Length <= sizeof(g_tx));
    memcpy(g_tx, pui8_Data, ui16_Length);
    g_txLen = ui16_Length;
    return E_W5500_OK;
}

E_W5500_Result fn_W5500_RecvExact(uint8_t *pui8_Data, uint16_t ui16_Length, uint32_t ui32_TimeoutMs)
{
    (void)ui32_TimeoutMs;
    assert(pui8_Data != NULL);
    if ((uint32_t)g_rxPos + ui16_Length > g_rxLen) return E_W5500_IO_ERROR;
    memcpy(pui8_Data, &g_rx[g_rxPos], ui16_Length);
    g_rxPos = (uint16_t)(g_rxPos + ui16_Length);
    return E_W5500_OK;
}

void fn_W5500_Close(void)
{
    /* Host mock: Modbus closes the persistent socket on protocol/transport faults. */
}

static void fn_TestReadBits(void)
{
    S_ModbusTcpClient st_Client;
    uint16_t ui16_Mask = 0U;
    const uint8_t aui8_Pdu[] = {0x02U, 0x02U, 0x05U, 0x80U};
    fn_ModbusTcp_Init(&st_Client);
    fn_PrepareResponse(1U, 1U, aui8_Pdu, sizeof(aui8_Pdu));
    assert(fn_ModbusTcp_ReadBits(&st_Client, 1U, 0x02U, 0U, 16U, &ui16_Mask) == E_MBTCP_OK);
    assert(ui16_Mask == 0x8005U);
    assert(g_txLen == 12U);
    assert(g_tx[6] == 1U && g_tx[7] == 0x02U);
}

static void fn_TestReadRegisters(void)
{
    S_ModbusTcpClient st_Client;
    uint16_t aui16_Registers[4] = {0U};
    const uint8_t aui8_Pdu[] = {0x04U,0x08U, 0x00U,0x7BU, 0xFFU,0x38U, 0x12U,0x34U, 0x80U,0x00U};
    fn_ModbusTcp_Init(&st_Client);
    fn_PrepareResponse(1U, 1U, aui8_Pdu, sizeof(aui8_Pdu));
    assert(fn_ModbusTcp_ReadRegisters(&st_Client, 1U, 0x04U, 1024U, 4U, aui16_Registers) == E_MBTCP_OK);
    assert(aui16_Registers[0] == 0x007BU && aui16_Registers[1] == 0xFF38U);
    assert(aui16_Registers[2] == 0x1234U && aui16_Registers[3] == 0x8000U);
}

static void fn_TestWrites(void)
{
    S_ModbusTcpClient st_Client;
    const uint8_t aui8_CoilPdu[] = {0x05U,0x00U,0x07U,0xFFU,0x00U};
    const uint8_t aui8_RegPdu[]  = {0x06U,0x00U,0x02U,0x00U,0x84U};
    fn_ModbusTcp_Init(&st_Client);
    fn_PrepareResponse(1U, 2U, aui8_CoilPdu, sizeof(aui8_CoilPdu));
    assert(fn_ModbusTcp_WriteSingleCoil(&st_Client, 2U, 7U, true) == E_MBTCP_OK);
    assert(g_tx[7] == 0x05U && g_tx[10] == 0xFFU);

    fn_ModbusTcp_Init(&st_Client);
    fn_PrepareResponse(1U, 2U, aui8_RegPdu, sizeof(aui8_RegPdu));
    assert(fn_ModbusTcp_WriteSingleRegister(&st_Client, 2U, 2U, 132U) == E_MBTCP_OK);
    assert(g_tx[7] == 0x06U && g_tx[10] == 0x00U && g_tx[11] == 0x84U);
}

static void fn_TestException(void)
{
    S_ModbusTcpClient st_Client;
    uint16_t aui16_Registers[4] = {0U};
    const uint8_t aui8_Exception[] = {0x84U,0x02U};
    fn_ModbusTcp_Init(&st_Client);
    fn_PrepareResponse(1U, 1U, aui8_Exception, sizeof(aui8_Exception));
    assert(fn_ModbusTcp_ReadRegisters(&st_Client, 1U, 0x04U, 0U, 4U, aui16_Registers) == E_MBTCP_EXCEPTION);
    assert(st_Client.ui8_LastException == 0x02U);
}

static void fn_TestMalformedAndOverflow(void)
{
    S_ModbusTcpClient client;
    uint16_t data[4] = {123U, 123U, 123U, 123U};
    const uint8_t malformed[] = {0x84U, 0x06U, 0x00U};
    const uint8_t busy[] = {0x84U, 0x06U};
    fn_ModbusTcp_Init(&client);
    fn_PrepareResponse(1U, 1U, malformed, sizeof(malformed));
    assert(fn_ModbusTcp_ReadRegisters(&client, 1U, 4U, 0U, 4U, data) == E_MBTCP_PROTOCOL_ERROR);
    assert(data[0] == 123U);
    fn_ModbusTcp_Init(&client);
    fn_PrepareResponse(1U, 1U, busy, sizeof(busy));
    assert(fn_ModbusTcp_ReadRegisters(&client, 1U, 4U, 0U, 4U, data) == E_MBTCP_EXCEPTION);
    assert(client.ui8_LastException == 6U);
    g_txLen = 0U;
    assert(fn_ModbusTcp_ReadRegisters(&client, 1U, 4U, 65535U, 2U, data) == E_MBTCP_INVALID_ARGUMENT);
    assert(g_txLen == 0U);
    assert(fn_ModbusTcp_ReadBits(&client, 1U, 2U, 65535U, 2U, data) == E_MBTCP_INVALID_ARGUMENT);
    assert(g_txLen == 0U);
}

static void fn_TestMismatchedFrameIsFullyConsumed(void)
{
    S_ModbusTcpClient st_Client;
    uint16_t ui16_Mask = 0U;
    const uint8_t aui8_StalePdu[] = {0x02U, 0x02U, 0xAAU, 0x55U};
    const uint8_t aui8_ValidPdu[] = {0x02U, 0x02U, 0x05U, 0x80U};

    fn_ModbusTcp_Init(&st_Client);
    fn_PrepareResponse(99U, 1U, aui8_StalePdu, sizeof(aui8_StalePdu));
    fn_AppendResponse(2U, 1U, aui8_ValidPdu, sizeof(aui8_ValidPdu));

    assert(fn_ModbusTcp_ReadBits(&st_Client, 1U, 0x02U, 0U, 16U, &ui16_Mask) ==
           E_MBTCP_PROTOCOL_ERROR);
    assert(g_rxPos == (uint16_t)(7U + sizeof(aui8_StalePdu)));
    assert(fn_ModbusTcp_ReadBits(&st_Client, 1U, 0x02U, 0U, 16U, &ui16_Mask) == E_MBTCP_OK);
    assert(ui16_Mask == 0x8005U);
    assert(g_rxPos == g_rxLen);
}

int main(void)
{
    fn_TestReadBits();
    fn_TestReadRegisters();
    fn_TestWrites();
    fn_TestException();
    fn_TestMalformedAndOverflow();
    fn_TestMismatchedFrameIsFullyConsumed();
    puts("modbus tcp tests: PASS");
    return 0;
}
