#ifndef W5500_DRIVER_H
#define W5500_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @file    w5500_driver.h
 * @brief   Public W5500 transport driver interface.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    E_W5500_OK = 0,
    E_W5500_INVALID_ARGUMENT = -1,
    E_W5500_IO_ERROR = -2,
    E_W5500_TIMEOUT = -3,
    E_W5500_LINK_DOWN = -4,
    E_W5500_SOCKET_ERROR = -5,
    E_W5500_VERSION_ERROR = -6
} E_W5500_Result;

typedef enum
{
    E_W5500_LINK_STATUS_UP = 0,
    E_W5500_LINK_STATUS_DOWN,
    E_W5500_LINK_STATUS_IO_ERROR
} E_W5500_LinkStatus;

typedef struct
{
    uint8_t aui8_Mac[6];
    uint8_t aui8_Ip[4];
    uint8_t aui8_Subnet[4];
    uint8_t aui8_Gateway[4];
} S_W5500_NetInfo;

typedef struct
{
    uint8_t  ui8_PhyCfgr;
    uint8_t  ui8_SocketStatus;
    uint8_t  ui8_SocketInterrupt;
    uint32_t ui32_StableReadRetries;
    uint32_t ui32_StableReadFailures;
} S_W5500_Diagnostics;

/** @brief Initialize W5500 network registers and socket memory. */
E_W5500_Result fn_W5500_Init(const S_W5500_NetInfo *pst_NetInfo);
/** @brief Read the PHY link state while distinguishing SPI read failure. */
E_W5500_LinkStatus fn_W5500_GetLinkStatus(uint8_t *pui8_PhyCfgr);
/** @brief Read socket-0 connection state without collapsing SPI failure into disconnected. */
E_W5500_Result fn_W5500_GetConnectionStatus(bool *pb_Connected, uint8_t *pui8_SocketStatus,
                                             uint8_t *pui8_SocketInterrupt);
/** @brief Convenience connection query; prefer fn_W5500_GetConnectionStatus() in recovery logic. */
bool fn_W5500_IsConnected(void);
/** @brief Connect socket 0 to the configured Modbus TCP server. */
E_W5500_Result fn_W5500_Connect(const uint8_t aui8_ServerIp[4], uint16_t ui16_ServerPort,
                                uint16_t ui16_SourcePort, uint32_t ui32_TimeoutMs);
/** @brief Close socket 0 safely. */
void fn_W5500_Close(void);
/** @brief Send exactly one TCP payload. */
E_W5500_Result fn_W5500_Send(const uint8_t *pui8_Data, uint16_t ui16_Length, uint32_t ui32_TimeoutMs);
/** @brief Receive exactly the requested number of TCP payload bytes. */
E_W5500_Result fn_W5500_RecvExact(uint8_t *pui8_Data, uint16_t ui16_Length, uint32_t ui32_TimeoutMs);
/** @brief Read W5500 VERSIONR; expected value is 0x04. */
uint8_t fn_W5500_GetVersion(void);
/** @brief Read diagnostic PHY/socket registers without changing socket state. */
E_W5500_Result fn_W5500_GetDiagnostics(S_W5500_Diagnostics *pst_Diagnostics);

#ifdef __cplusplus
}
#endif

#endif /* W5500_DRIVER_H */
