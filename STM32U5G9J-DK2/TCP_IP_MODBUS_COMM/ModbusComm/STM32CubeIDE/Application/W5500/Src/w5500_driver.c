/**
 * @file    w5500_driver.c
 * @brief   W5500 hardwired TCP/IP driver using socket 0 for persistent Modbus TCP.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#include "w5500_driver.h"
#include "w5500_port_stm32.h"
#include <string.h>

#define W5500_CTRL_READ(bsb)      ((uint8_t)((bsb) << 3))
#define W5500_CTRL_WRITE(bsb)     ((uint8_t)(((bsb) << 3) | 0x04U))
#define W5500_BSB_COMMON          (0x00U)
#define W5500_BSB_S0_REG          (0x01U)
#define W5500_BSB_S0_TX           (0x02U)
#define W5500_BSB_S0_RX           (0x03U)

#define W5500_MR                   (0x0000U)
#define W5500_GAR                  (0x0001U)
#define W5500_SUBR                 (0x0005U)
#define W5500_SHAR                 (0x0009U)
#define W5500_SIPR                 (0x000FU)
#define W5500_RTR                  (0x0019U)
#define W5500_RCR                  (0x001BU)
#define W5500_PHYCFGR              (0x002EU)
#define W5500_VERSIONR             (0x0039U)

#define W5500_SN_MR                (0x0000U)
#define W5500_SN_CR                (0x0001U)
#define W5500_SN_IR                (0x0002U)
#define W5500_SN_SR                (0x0003U)
#define W5500_SN_PORT              (0x0004U)
#define W5500_SN_DIPR              (0x000CU)
#define W5500_SN_DPORT             (0x0010U)
#define W5500_SN_RXBUF_SIZE        (0x001EU)
#define W5500_SN_TXBUF_SIZE        (0x001FU)
#define W5500_SN_TX_FSR            (0x0020U)
#define W5500_SN_TX_WR             (0x0024U)
#define W5500_SN_RX_RSR            (0x0026U)
#define W5500_SN_RX_RD             (0x0028U)
#define W5500_SN_KPALVTR           (0x002FU)

#define W5500_SN_MR_TCP            (0x01U)
#define W5500_CR_OPEN              (0x01U)
#define W5500_CR_CONNECT           (0x04U)
#define W5500_CR_DISCON            (0x08U)
#define W5500_CR_CLOSE             (0x10U)
#define W5500_CR_SEND              (0x20U)
#define W5500_CR_RECV              (0x40U)
#define W5500_IR_CON               (0x01U)
#define W5500_IR_DISCON            (0x02U)
#define W5500_IR_RECV              (0x04U)
#define W5500_IR_TIMEOUT           (0x08U)
#define W5500_IR_SENDOK            (0x10U)
#define W5500_SOCK_CLOSED          (0x00U)
#define W5500_SOCK_INIT            (0x13U)
#define W5500_SOCK_ESTABLISHED     (0x17U)
#define W5500_SOCK_CLOSE_WAIT      (0x1CU)
#define W5500_VERSION_EXPECTED     (0x04U)
#define W5500_SOCKET_BUFFER_SIZE   (16384U)
#define W5500_SOCKET_BUFFER_MASK   (W5500_SOCKET_BUFFER_SIZE - 1U)
#define W5500_STABLE_READ_ATTEMPTS  (32U)

static uint32_t s_ui32_StableReadRetries;
static uint32_t s_ui32_StableReadFailures;

static int fn_W5500_RegWrite(uint8_t bsb, uint16_t address, const uint8_t *data, uint16_t len)
{
    return fn_W5500Port_WriteFrame(address, W5500_CTRL_WRITE(bsb), data, len);
}
static int fn_W5500_RegRead(uint8_t bsb, uint16_t address, uint8_t *data, uint16_t len)
{
    return fn_W5500Port_ReadFrame(address, W5500_CTRL_READ(bsb), data, len);
}
static int fn_W5500_Write8(uint8_t bsb, uint16_t address, uint8_t value)
{
    return fn_W5500_RegWrite(bsb, address, &value, 1U);
}
static int fn_W5500_Read8(uint8_t bsb, uint16_t address, uint8_t *value)
{
    return fn_W5500_RegRead(bsb, address, value, 1U);
}
static int fn_W5500_Write16(uint8_t bsb, uint16_t address, uint16_t value)
{
    uint8_t v[2] = {(uint8_t)(value >> 8), (uint8_t)value};
    return fn_W5500_RegWrite(bsb, address, v, 2U);
}
static int fn_W5500_Read16Once(uint8_t bsb, uint16_t address, uint16_t *value)
{
    uint8_t v[2];
    if (fn_W5500_RegRead(bsb, address, v, 2U) != 0) return -1;
    *value = (uint16_t)(((uint16_t)v[0] << 8) | v[1]);
    return 0;
}
static int fn_W5500_Read16Stable(uint8_t bsb, uint16_t address, uint16_t *value)
{
    uint16_t a = 0U;
    uint16_t b = 0U;
    unsigned int tries;

    if (value == NULL)
    {
        return -1;
    }

    /*
     * Sn_TX_FSR and Sn_RX_RSR are volatile counters updated asynchronously by
     * the W5500. The datasheet-recommended approach is to read until two
     * consecutive samples agree. Four fixed attempts were too aggressive for
     * a continuously active Modbus session and could falsely report an SPI/I/O
     * fault while the counter was simply changing. Keep retrying for a bounded
     * 32-sample window and yield between mismatches.
     */
    for (tries = 0U; tries < W5500_STABLE_READ_ATTEMPTS; ++tries)
    {
        if ((fn_W5500_Read16Once(bsb, address, &a) != 0) ||
            (fn_W5500_Read16Once(bsb, address, &b) != 0))
        {
            ++s_ui32_StableReadFailures;
            return -1;
        }
        if (a == b)
        {
            *value = a;
            return 0;
        }
        ++s_ui32_StableReadRetries;
        fn_W5500Port_Yield();
    }

    ++s_ui32_StableReadFailures;
    return -1;
}
static bool fn_W5500_Elapsed(uint32_t start, uint32_t timeout)
{
    return ((uint32_t)(fn_W5500Port_Millis() - start) >= timeout);
}
static E_W5500_Result fn_W5500_Command(uint8_t cmd, uint32_t timeout_ms)
{
    uint32_t start = fn_W5500Port_Millis();
    uint8_t cr = cmd;
    if (fn_W5500_Write8(W5500_BSB_S0_REG, W5500_SN_CR, cr) != 0) return E_W5500_IO_ERROR;
    do
    {
        if (fn_W5500_Read8(W5500_BSB_S0_REG, W5500_SN_CR, &cr) != 0) return E_W5500_IO_ERROR;
        if (cr == 0U) return E_W5500_OK;
        fn_W5500Port_Yield();
    } while (!fn_W5500_Elapsed(start, timeout_ms));
    return E_W5500_TIMEOUT;
}
static E_W5500_Result fn_W5500_BufferWrite(uint16_t pointer, const uint8_t *data, uint16_t len)
{
    uint16_t off = (uint16_t)(pointer & W5500_SOCKET_BUFFER_MASK);
    uint16_t first = len;
    if ((uint32_t)off + len > W5500_SOCKET_BUFFER_SIZE)
    {
        first = (uint16_t)(W5500_SOCKET_BUFFER_SIZE - off);
    }
    if (fn_W5500_RegWrite(W5500_BSB_S0_TX, off, data, first) != 0) return E_W5500_IO_ERROR;
    if (first < len)
    {
        if (fn_W5500_RegWrite(W5500_BSB_S0_TX, 0U, &data[first], (uint16_t)(len - first)) != 0) return E_W5500_IO_ERROR;
    }
    return E_W5500_OK;
}
static E_W5500_Result fn_W5500_BufferRead(uint16_t pointer, uint8_t *data, uint16_t len)
{
    uint16_t off = (uint16_t)(pointer & W5500_SOCKET_BUFFER_MASK);
    uint16_t first = len;
    if ((uint32_t)off + len > W5500_SOCKET_BUFFER_SIZE)
    {
        first = (uint16_t)(W5500_SOCKET_BUFFER_SIZE - off);
    }
    if (fn_W5500_RegRead(W5500_BSB_S0_RX, off, data, first) != 0) return E_W5500_IO_ERROR;
    if (first < len)
    {
        if (fn_W5500_RegRead(W5500_BSB_S0_RX, 0U, &data[first], (uint16_t)(len - first)) != 0) return E_W5500_IO_ERROR;
    }
    return E_W5500_OK;
}

uint8_t fn_W5500_GetVersion(void)
{
    uint8_t v = 0U;
    (void)fn_W5500_Read8(W5500_BSB_COMMON, W5500_VERSIONR, &v);
    return v;
}

E_W5500_Result fn_W5500_Init(const S_W5500_NetInfo *pst_NetInfo)
{
    uint8_t reset = 0x80U;
    uint8_t zero = 0U;
    uint8_t sixteen = 16U;
    uint8_t version;
    uint16_t rtr = 2000U; /* 200 ms in 100 us units */
    uint8_t rcr = 3U;
    uint32_t start;
    unsigned int s;
    if (pst_NetInfo == NULL) return E_W5500_INVALID_ARGUMENT;
    s_ui32_StableReadRetries = 0U;
    s_ui32_StableReadFailures = 0U;
    fn_W5500Port_HardwareReset();
    if (fn_W5500_Write8(W5500_BSB_COMMON, W5500_MR, reset) != 0) return E_W5500_IO_ERROR;
    start = fn_W5500Port_Millis();
    do
    {
        if (fn_W5500_Read8(W5500_BSB_COMMON, W5500_MR, &reset) != 0) return E_W5500_IO_ERROR;
        if ((reset & 0x80U) == 0U) break;
        fn_W5500Port_Yield();
    } while (!fn_W5500_Elapsed(start, 100U));
    if ((reset & 0x80U) != 0U) return E_W5500_TIMEOUT;
    version = fn_W5500_GetVersion();
    if (version != W5500_VERSION_EXPECTED) return E_W5500_VERSION_ERROR;
    if ((fn_W5500_RegWrite(W5500_BSB_COMMON, W5500_GAR, pst_NetInfo->aui8_Gateway, 4U) != 0) ||
        (fn_W5500_RegWrite(W5500_BSB_COMMON, W5500_SUBR, pst_NetInfo->aui8_Subnet, 4U) != 0) ||
        (fn_W5500_RegWrite(W5500_BSB_COMMON, W5500_SHAR, pst_NetInfo->aui8_Mac, 6U) != 0) ||
        (fn_W5500_RegWrite(W5500_BSB_COMMON, W5500_SIPR, pst_NetInfo->aui8_Ip, 4U) != 0) ||
        (fn_W5500_Write16(W5500_BSB_COMMON, W5500_RTR, rtr) != 0) ||
        (fn_W5500_Write8(W5500_BSB_COMMON, W5500_RCR, rcr) != 0))
    {
        return E_W5500_IO_ERROR;
    }
    /* One persistent socket receives all memory: 16 KiB TX + 16 KiB RX. */
    if ((fn_W5500_Write8(W5500_BSB_S0_REG, W5500_SN_RXBUF_SIZE, sixteen) != 0) ||
        (fn_W5500_Write8(W5500_BSB_S0_REG, W5500_SN_TXBUF_SIZE, sixteen) != 0)) return E_W5500_IO_ERROR;
    for (s = 1U; s < 8U; ++s)
    {
        const uint8_t bsb = (uint8_t)(1U + (4U * s));
        if ((fn_W5500_Write8(bsb, W5500_SN_RXBUF_SIZE, zero) != 0) || (fn_W5500_Write8(bsb, W5500_SN_TXBUF_SIZE, zero) != 0))
            return E_W5500_IO_ERROR;
    }
    (void)fn_W5500_Write8(W5500_BSB_S0_REG, W5500_SN_KPALVTR, 2U); /* ~10 s keepalive after auto-keepalive starts. */
    return E_W5500_OK;
}

E_W5500_LinkStatus fn_W5500_GetLinkStatus(uint8_t *pui8_PhyCfgr)
{
    uint8_t ui8_Phy = 0U;
    if (fn_W5500_Read8(W5500_BSB_COMMON, W5500_PHYCFGR, &ui8_Phy) != 0)
    {
        return E_W5500_LINK_STATUS_IO_ERROR;
    }
    if (pui8_PhyCfgr != NULL)
    {
        *pui8_PhyCfgr = ui8_Phy;
    }
    return ((ui8_Phy & 0x01U) != 0U) ? E_W5500_LINK_STATUS_UP : E_W5500_LINK_STATUS_DOWN;
}

E_W5500_Result fn_W5500_GetConnectionStatus(bool *pb_Connected, uint8_t *pui8_SocketStatus,
                                             uint8_t *pui8_SocketInterrupt)
{
    uint8_t ui8_Status = W5500_SOCK_CLOSED;
    uint8_t ui8_Interrupt = 0U;

    if (pb_Connected == NULL)
    {
        return E_W5500_INVALID_ARGUMENT;
    }
    if ((fn_W5500_Read8(W5500_BSB_S0_REG, W5500_SN_SR, &ui8_Status) != 0) ||
        (fn_W5500_Read8(W5500_BSB_S0_REG, W5500_SN_IR, &ui8_Interrupt) != 0))
    {
        return E_W5500_IO_ERROR;
    }
    *pb_Connected = (ui8_Status == W5500_SOCK_ESTABLISHED);
    if (pui8_SocketStatus != NULL)
    {
        *pui8_SocketStatus = ui8_Status;
    }
    if (pui8_SocketInterrupt != NULL)
    {
        *pui8_SocketInterrupt = ui8_Interrupt;
    }
    return E_W5500_OK;
}

bool fn_W5500_IsConnected(void)
{
    bool b_Connected = false;
    return (fn_W5500_GetConnectionStatus(&b_Connected, NULL, NULL) == E_W5500_OK) && b_Connected;
}

void fn_W5500_Close(void)
{
    uint8_t ir = 0xFFU;
    if (fn_W5500_IsConnected()) (void)fn_W5500_Command(W5500_CR_DISCON, 50U);
    (void)fn_W5500_Command(W5500_CR_CLOSE, 50U);
    (void)fn_W5500_Write8(W5500_BSB_S0_REG, W5500_SN_IR, ir);
}

static void fn_W5500_AbortSocket(void)
{
    /* Immediate local normalization without DISCON. Used only when Socket 0
     * has already been confirmed non-ESTABLISHED or a CONNECT attempt failed.
     * This cannot generate the graceful FIN that confused earlier captures. */
    (void)fn_W5500_Command(W5500_CR_CLOSE, 50U);
    (void)fn_W5500_Write8(W5500_BSB_S0_REG, W5500_SN_IR, 0xFFU);
}

E_W5500_Result fn_W5500_Connect(const uint8_t server_ip[4], uint16_t server_port,
                                            uint16_t source_port, uint32_t timeout_ms)
{
    uint32_t start;
    uint8_t sr = 0U;
    uint8_t ir = 0U;
    unsigned int sample;
    bool established_seen = false;

    if ((server_ip == NULL) || (server_port == 0U) || (source_port == 0U))
    {
        return E_W5500_INVALID_ARGUMENT;
    }

    {
        uint8_t ui8_Phy = 0U;
        const E_W5500_LinkStatus e_Link = fn_W5500_GetLinkStatus(&ui8_Phy);
        (void)ui8_Phy;
        if (e_Link == E_W5500_LINK_STATUS_IO_ERROR) return E_W5500_IO_ERROR;
        if (e_Link != E_W5500_LINK_STATUS_UP) return E_W5500_LINK_DOWN;
    }

    /* Alpha.7 reconnect guard. A caller may reach this function after a
     * transient Sn_SR disagreement. Never DISCON/CLOSE a live session. Check
     * several samples and return success if ESTABLISHED appears in any one. */
    for (sample = 0U; sample < 3U; ++sample)
    {
        if (fn_W5500_Read8(W5500_BSB_S0_REG, W5500_SN_SR, &sr) != 0)
        {
            return E_W5500_IO_ERROR;
        }
        if (sr == W5500_SOCK_ESTABLISHED)
        {
            established_seen = true;
            break;
        }
        fn_W5500Port_Yield();
    }
    if (established_seen)
    {
        return E_W5500_OK;
    }

    /* Socket has been confirmed non-established. Normalize locally without a
     * DISCON command, then start a fresh TCP active open. */
    fn_W5500_AbortSocket();
    if ((fn_W5500_Write8(W5500_BSB_S0_REG, W5500_SN_MR, W5500_SN_MR_TCP) != 0) ||
        (fn_W5500_Write16(W5500_BSB_S0_REG, W5500_SN_PORT, source_port) != 0) ||
        (fn_W5500_RegWrite(W5500_BSB_S0_REG, W5500_SN_DIPR, server_ip, 4U) != 0) ||
        (fn_W5500_Write16(W5500_BSB_S0_REG, W5500_SN_DPORT, server_port) != 0))
    {
        return E_W5500_IO_ERROR;
    }
    if (fn_W5500_Command(W5500_CR_OPEN, 100U) != E_W5500_OK) return E_W5500_SOCKET_ERROR;
    if ((fn_W5500_Read8(W5500_BSB_S0_REG, W5500_SN_SR, &sr) != 0) ||
        (sr != W5500_SOCK_INIT))
    {
        fn_W5500_AbortSocket();
        return E_W5500_SOCKET_ERROR;
    }
    if (fn_W5500_Command(W5500_CR_CONNECT, 100U) != E_W5500_OK)
    {
        fn_W5500_AbortSocket();
        return E_W5500_SOCKET_ERROR;
    }

    start = fn_W5500Port_Millis();
    do
    {
        if ((fn_W5500_Read8(W5500_BSB_S0_REG, W5500_SN_SR, &sr) != 0) ||
            (fn_W5500_Read8(W5500_BSB_S0_REG, W5500_SN_IR, &ir) != 0))
        {
            return E_W5500_IO_ERROR;
        }
        if (sr == W5500_SOCK_ESTABLISHED)
        {
            if ((ir & W5500_IR_CON) != 0U)
            {
                (void)fn_W5500_Write8(W5500_BSB_S0_REG, W5500_SN_IR, W5500_IR_CON);
            }
            return E_W5500_OK;
        }
        if (((ir & W5500_IR_TIMEOUT) != 0U) ||
            ((ir & W5500_IR_DISCON) != 0U) ||
            (sr == W5500_SOCK_CLOSED))
        {
            if ((ir & (W5500_IR_TIMEOUT | W5500_IR_DISCON)) != 0U)
            {
                (void)fn_W5500_Write8(W5500_BSB_S0_REG, W5500_SN_IR,
                                      (uint8_t)(ir & (W5500_IR_TIMEOUT | W5500_IR_DISCON)));
            }
            fn_W5500_AbortSocket();
            return E_W5500_TIMEOUT;
        }
        fn_W5500Port_Yield();
    } while (!fn_W5500_Elapsed(start, timeout_ms));

    fn_W5500_AbortSocket();
    return E_W5500_TIMEOUT;
}

E_W5500_Result fn_W5500_Send(const uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
    uint32_t start;
    uint16_t free_size = 0U;
    uint16_t wr = 0U;
    uint8_t ir = 0U;
    if ((data == NULL) || (length == 0U) || (length > W5500_SOCKET_BUFFER_SIZE)) return E_W5500_INVALID_ARGUMENT;
    if (!fn_W5500_IsConnected()) return E_W5500_SOCKET_ERROR;
    start = fn_W5500Port_Millis();
    while (!fn_W5500_Elapsed(start, timeout_ms))
    {
        if (fn_W5500_Read16Stable(W5500_BSB_S0_REG, W5500_SN_TX_FSR, &free_size) != 0) return E_W5500_IO_ERROR;
        if (free_size >= length) break;
        if (!fn_W5500_IsConnected()) return E_W5500_SOCKET_ERROR;
        fn_W5500Port_Yield();
    }
    if (free_size < length) return E_W5500_TIMEOUT;
    if (fn_W5500_Read16Once(W5500_BSB_S0_REG, W5500_SN_TX_WR, &wr) != 0) return E_W5500_IO_ERROR;
    if (fn_W5500_BufferWrite(wr, data, length) != E_W5500_OK) return E_W5500_IO_ERROR;
    wr = (uint16_t)(wr + length);
    if (fn_W5500_Write16(W5500_BSB_S0_REG, W5500_SN_TX_WR, wr) != 0) return E_W5500_IO_ERROR;
    if (fn_W5500_Command(W5500_CR_SEND, 100U) != E_W5500_OK) return E_W5500_SOCKET_ERROR;
    start = fn_W5500Port_Millis();
    do
    {
        if (fn_W5500_Read8(W5500_BSB_S0_REG, W5500_SN_IR, &ir) != 0) return E_W5500_IO_ERROR;
        if ((ir & W5500_IR_SENDOK) != 0U)
        {
            (void)fn_W5500_Write8(W5500_BSB_S0_REG, W5500_SN_IR, W5500_IR_SENDOK);
            return E_W5500_OK;
        }
        if ((ir & (W5500_IR_TIMEOUT | W5500_IR_DISCON)) != 0U) return E_W5500_TIMEOUT;
        fn_W5500Port_Yield();
    } while (!fn_W5500_Elapsed(start, timeout_ms));
    return E_W5500_TIMEOUT;
}

E_W5500_Result fn_W5500_RecvExact(uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
    uint32_t start;
    uint16_t done = 0U;
    if ((data == NULL) || (length == 0U)) return E_W5500_INVALID_ARGUMENT;
    start = fn_W5500Port_Millis();
    while (done < length)
    {
        uint16_t available = 0U;
        uint16_t rd = 0U;
        uint16_t take;
        uint8_t sr = 0U;
        if (fn_W5500_Read16Stable(W5500_BSB_S0_REG, W5500_SN_RX_RSR, &available) != 0) return E_W5500_IO_ERROR;
        if (available == 0U)
        {
            if (fn_W5500_Elapsed(start, timeout_ms)) return E_W5500_TIMEOUT;
            if (fn_W5500_Read8(W5500_BSB_S0_REG, W5500_SN_SR, &sr) != 0) return E_W5500_IO_ERROR;
            if ((sr != W5500_SOCK_ESTABLISHED) && (sr != W5500_SOCK_CLOSE_WAIT)) return E_W5500_SOCKET_ERROR;
            fn_W5500Port_Yield();
            continue;
        }
        take = (uint16_t)(length - done);
        if (take > available) take = available;
        if (fn_W5500_Read16Once(W5500_BSB_S0_REG, W5500_SN_RX_RD, &rd) != 0) return E_W5500_IO_ERROR;
        if (fn_W5500_BufferRead(rd, &data[done], take) != E_W5500_OK) return E_W5500_IO_ERROR;
        rd = (uint16_t)(rd + take);
        if (fn_W5500_Write16(W5500_BSB_S0_REG, W5500_SN_RX_RD, rd) != 0) return E_W5500_IO_ERROR;
        if (fn_W5500_Command(W5500_CR_RECV, 100U) != E_W5500_OK) return E_W5500_SOCKET_ERROR;
        done = (uint16_t)(done + take);
    }
    return E_W5500_OK;
}


E_W5500_Result fn_W5500_GetDiagnostics(S_W5500_Diagnostics *pst_Diagnostics)
{
    if (pst_Diagnostics == NULL)
    {
        return E_W5500_INVALID_ARGUMENT;
    }
    if ((fn_W5500_Read8(W5500_BSB_COMMON, W5500_PHYCFGR, &pst_Diagnostics->ui8_PhyCfgr) != 0) ||
        (fn_W5500_Read8(W5500_BSB_S0_REG, W5500_SN_SR, &pst_Diagnostics->ui8_SocketStatus) != 0) ||
        (fn_W5500_Read8(W5500_BSB_S0_REG, W5500_SN_IR, &pst_Diagnostics->ui8_SocketInterrupt) != 0))
    {
        return E_W5500_IO_ERROR;
    }
    pst_Diagnostics->ui32_StableReadRetries = s_ui32_StableReadRetries;
    pst_Diagnostics->ui32_StableReadFailures = s_ui32_StableReadFailures;
    return E_W5500_OK;
}
