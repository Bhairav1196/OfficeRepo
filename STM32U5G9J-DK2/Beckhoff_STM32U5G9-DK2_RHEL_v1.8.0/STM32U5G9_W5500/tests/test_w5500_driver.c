/**
 * @file    test_w5500_driver.c
 * @brief   Host regression tests for the W5500 transport driver.
 * @author  Prathamesh Chipkar
 * @date    09-Sep-2026
 * @version 2.0.0-alpha.9.0
 */
#include "w5500_driver.h"
#include "w5500_port_stm32.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define COMMON_BSB 0U
#define S0_REG_BSB 1U
#define S0_TX_BSB  2U
#define S0_RX_BSB  3U
#define MR         0x0000U
#define PHYCFGR    0x002EU
#define VERSIONR   0x0039U
#define SN_CR      0x0001U
#define SN_IR      0x0002U
#define SN_SR      0x0003U
#define SN_DIPR    0x000CU
#define SN_DPORT   0x0010U
#define SN_RXBUF   0x001EU
#define SN_TXBUF   0x001FU
#define SN_TX_FSR  0x0020U
#define SN_TX_WR   0x0024U
#define SN_RX_RSR  0x0026U
#define SN_RX_RD   0x0028U

static uint8_t g_common[0x10000];
static uint8_t g_s0Reg[0x10000];
static uint8_t g_txMem[16384];
static uint8_t g_rxMem[16384];
static uint8_t g_otherSocketReg[0x10000];
static uint32_t g_nowMs;
static bool g_failReads;
static uint32_t g_disconCommands;
static uint32_t g_closeCommands;

static void fn_Put16(uint8_t *pui8_Memory, uint16_t ui16_Address, uint16_t ui16_Value)
{
    pui8_Memory[ui16_Address] = (uint8_t)(ui16_Value >> 8);
    pui8_Memory[(uint16_t)(ui16_Address + 1U)] = (uint8_t)ui16_Value;
}

static uint16_t fn_Get16(const uint8_t *pui8_Memory, uint16_t ui16_Address)
{
    return (uint16_t)(((uint16_t)pui8_Memory[ui16_Address] << 8) |
                      pui8_Memory[(uint16_t)(ui16_Address + 1U)]);
}

static void fn_ResetModel(void)
{
    memset(g_common, 0, sizeof(g_common));
    memset(g_s0Reg, 0, sizeof(g_s0Reg));
    memset(g_txMem, 0, sizeof(g_txMem));
    memset(g_rxMem, 0, sizeof(g_rxMem));
    memset(g_otherSocketReg, 0, sizeof(g_otherSocketReg));
    g_common[VERSIONR] = 0x04U;
    g_common[PHYCFGR] = 0x01U;
    fn_Put16(g_s0Reg, SN_TX_FSR, 16384U);
    g_nowMs = 0U;
    g_failReads = false;
    g_disconCommands = 0U;
    g_closeCommands = 0U;
}

static uint8_t fn_BsbFromControl(uint8_t ui8_Control)
{
    return (uint8_t)((ui8_Control >> 3) & 0x1FU);
}

static uint8_t *fn_SpaceFor(uint8_t ui8_Bsb)
{
    switch (ui8_Bsb)
    {
        case COMMON_BSB: return g_common;
        case S0_REG_BSB: return g_s0Reg;
        case S0_TX_BSB: return g_txMem;
        case S0_RX_BSB: return g_rxMem;
        default: return g_otherSocketReg;
    }
}

int32_t fn_W5500Port_WriteFrame(uint16_t ui16_Address, uint8_t ui8_Control,
                                const uint8_t *pui8_Data, uint16_t ui16_Length)
{
    const uint8_t ui8_Bsb = fn_BsbFromControl(ui8_Control);
    uint8_t *pui8_Space = fn_SpaceFor(ui8_Bsb);
    uint16_t ui16_Index;
    if ((ui8_Control & 0x04U) == 0U) return -1;
    for (ui16_Index = 0U; ui16_Index < ui16_Length; ++ui16_Index)
    {
        const uint16_t ui16_A = ((ui8_Bsb == S0_TX_BSB) || (ui8_Bsb == S0_RX_BSB))
                              ? (uint16_t)((ui16_Address + ui16_Index) & 0x3FFFU)
                              : (uint16_t)(ui16_Address + ui16_Index);
        pui8_Space[ui16_A] = pui8_Data[ui16_Index];
    }
    if ((ui8_Bsb == COMMON_BSB) && (ui16_Address == MR) && (ui16_Length == 1U) &&
        ((pui8_Data[0] & 0x80U) != 0U))
    {
        memset(g_common, 0, sizeof(g_common));
        g_common[VERSIONR] = 0x04U;
        g_common[PHYCFGR] = 0x01U;
        memset(g_s0Reg, 0, sizeof(g_s0Reg));
        fn_Put16(g_s0Reg, SN_TX_FSR, 16384U);
    }
    if ((ui8_Bsb == S0_REG_BSB) && (ui16_Address == SN_CR) && (ui16_Length == 1U))
    {
        switch (pui8_Data[0])
        {
            case 0x01U: g_s0Reg[SN_SR] = 0x13U; break;
            case 0x04U: g_s0Reg[SN_SR] = 0x17U; g_s0Reg[SN_IR] |= 0x01U; break;
            case 0x08U: ++g_disconCommands; g_s0Reg[SN_SR] = 0x00U; g_s0Reg[SN_IR] |= 0x02U; break;
            case 0x10U: ++g_closeCommands; g_s0Reg[SN_SR] = 0x00U; break;
            case 0x20U: g_s0Reg[SN_IR] |= 0x10U; break;
            default: break;
        }
        g_s0Reg[SN_CR] = 0U;
    }
    if ((ui8_Bsb == S0_REG_BSB) && (ui16_Address == SN_IR) && (ui16_Length == 1U))
    {
        g_s0Reg[SN_IR] = (uint8_t)(g_s0Reg[SN_IR] & (uint8_t)~pui8_Data[0]);
    }
    return 0;
}

int32_t fn_W5500Port_ReadFrame(uint16_t ui16_Address, uint8_t ui8_Control,
                               uint8_t *pui8_Data, uint16_t ui16_Length)
{
    const uint8_t ui8_Bsb = fn_BsbFromControl(ui8_Control);
    uint8_t *pui8_Space = fn_SpaceFor(ui8_Bsb);
    uint16_t ui16_Index;
    if (g_failReads || ((ui8_Control & 0x04U) != 0U)) return -1;
    for (ui16_Index = 0U; ui16_Index < ui16_Length; ++ui16_Index)
    {
        const uint16_t ui16_A = ((ui8_Bsb == S0_TX_BSB) || (ui8_Bsb == S0_RX_BSB))
                              ? (uint16_t)((ui16_Address + ui16_Index) & 0x3FFFU)
                              : (uint16_t)(ui16_Address + ui16_Index);
        pui8_Data[ui16_Index] = pui8_Space[ui16_A];
    }
    return 0;
}

void fn_W5500Port_HardwareReset(void) {}
uint32_t fn_W5500Port_Millis(void) { return g_nowMs; }
void fn_W5500Port_Yield(void) { ++g_nowMs; }

static void fn_TestInitConnectAndBuffers(void)
{
    const S_W5500_NetInfo st_Net = {
        {0x02U,0x55U,0U,0U,1U,0x60U},
        {192U,168U,1U,60U},
        {255U,255U,255U,0U},
        {192U,168U,1U,254U}
    };
    const uint8_t aui8_Server[4] = {192U,168U,1U,7U};
    uint8_t ui8_Phy = 0U;
    bool b_Connected = false;
    fn_ResetModel();
    assert(fn_W5500_Init(&st_Net) == E_W5500_OK);
    assert(fn_W5500_GetVersion() == 0x04U);
    assert(fn_W5500_GetLinkStatus(&ui8_Phy) == E_W5500_LINK_STATUS_UP);
    assert(g_s0Reg[SN_RXBUF] == 16U);
    assert(g_s0Reg[SN_TXBUF] == 16U);
    assert(fn_W5500_Connect(aui8_Server, 502U, 49152U, 100U) == E_W5500_OK);
    assert(fn_W5500_GetConnectionStatus(&b_Connected, NULL, NULL) == E_W5500_OK);
    assert(b_Connected);
    assert(memcmp(&g_s0Reg[SN_DIPR], aui8_Server, 4U) == 0);
    assert(fn_Get16(g_s0Reg, SN_DPORT) == 502U);
}


static void fn_TestConnectDoesNotTearDownEstablishedSession(void)
{
    const uint8_t aui8_Server[4] = {192U,168U,1U,7U};
    fn_ResetModel();
    g_s0Reg[SN_SR] = 0x17U;
    assert(fn_W5500_Connect(aui8_Server, 502U, 50000U, 100U) == E_W5500_OK);
    assert(g_s0Reg[SN_SR] == 0x17U);
    assert(g_disconCommands == 0U);
    assert(g_closeCommands == 0U);
}

static void fn_TestLinkReadFailureIsNotLinkDown(void)
{
    uint8_t ui8_Phy = 0U;
    fn_ResetModel();
    g_failReads = true;
    assert(fn_W5500_GetLinkStatus(&ui8_Phy) == E_W5500_LINK_STATUS_IO_ERROR);
    g_failReads = false;
    g_common[PHYCFGR] = 0U;
    assert(fn_W5500_GetLinkStatus(&ui8_Phy) == E_W5500_LINK_STATUS_DOWN);
}

static void fn_TestTxWrap(void)
{
    const uint8_t aui8_Payload[8] = {1U,2U,3U,4U,5U,6U,7U,8U};
    fn_Put16(g_s0Reg, SN_TX_FSR, 16384U);
    fn_Put16(g_s0Reg, SN_TX_WR, 16380U);
    g_s0Reg[SN_SR] = 0x17U;
    assert(fn_W5500_Send(aui8_Payload, sizeof(aui8_Payload), 100U) == E_W5500_OK);
    assert(memcmp(&g_txMem[16380], aui8_Payload, 4U) == 0);
    assert(memcmp(&g_txMem[0], &aui8_Payload[4], 4U) == 0);
    assert(fn_Get16(g_s0Reg, SN_TX_WR) == 16388U);
}

static void fn_TestRxWrap(void)
{
    uint8_t aui8_Out[6] = {0U};
    const uint8_t aui8_Expected[6] = {10U,11U,12U,13U,14U,15U};
    g_s0Reg[SN_SR] = 0x17U;
    g_rxMem[16382] = 10U; g_rxMem[16383] = 11U;
    g_rxMem[0] = 12U; g_rxMem[1] = 13U; g_rxMem[2] = 14U; g_rxMem[3] = 15U;
    fn_Put16(g_s0Reg, SN_RX_RD, 16382U);
    fn_Put16(g_s0Reg, SN_RX_RSR, 6U);
    assert(fn_W5500_RecvExact(aui8_Out, sizeof(aui8_Out), 100U) == E_W5500_OK);
    assert(memcmp(aui8_Out, aui8_Expected, sizeof(aui8_Expected)) == 0);
    assert(fn_Get16(g_s0Reg, SN_RX_RD) == 16388U);
}

int main(void)
{
    fn_TestInitConnectAndBuffers();
    fn_TestConnectDoesNotTearDownEstablishedSession();
    fn_TestLinkReadFailureIsNotLinkDown();
    fn_TestTxWrap();
    fn_TestRxWrap();
    puts("w5500 driver tests: PASS");
    return 0;
}
