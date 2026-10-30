#ifndef MODBUS_TCP_CLIENT_H
#define MODBUS_TCP_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @file    modbus_tcp_client.h
 * @brief   Minimal synchronous Modbus TCP client over the W5500 transport.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    E_MBTCP_OK = 0,
    E_MBTCP_INVALID_ARGUMENT = -1,
    E_MBTCP_TRANSPORT_ERROR = -2,
    E_MBTCP_PROTOCOL_ERROR = -3,
    E_MBTCP_EXCEPTION = -4
} E_ModbusTcpResult;

typedef struct
{
    uint16_t ui16_TransactionId;
    uint8_t  ui8_LastException;

    /* Last transaction context: retained even after an error for CubeIDE diagnostics. */
    uint8_t  ui8_LastUnitId;
    uint8_t  ui8_LastFunctionCode;
    uint16_t ui16_LastAddress;
    uint16_t ui16_LastQuantity;
    int32_t  i32_LastW5500Result;
} S_ModbusTcpClient;

/** @brief Initialize/reset the Modbus TCP client transaction state. */
void fn_ModbusTcp_Init(S_ModbusTcpClient *pst_Client);
/** @brief Execute FC01 or FC02 and decode up to 16 bits into a mask. */
E_ModbusTcpResult fn_ModbusTcp_ReadBits(S_ModbusTcpClient *pst_Client, uint8_t ui8_UnitId,
                                        uint8_t ui8_Function, uint16_t ui16_Address,
                                        uint16_t ui16_Quantity, uint16_t *pui16_Mask);
/** @brief Execute FC03 or FC04 and decode a contiguous register block. */
E_ModbusTcpResult fn_ModbusTcp_ReadRegisters(S_ModbusTcpClient *pst_Client, uint8_t ui8_UnitId,
                                             uint8_t ui8_Function, uint16_t ui16_Address,
                                             uint16_t ui16_Quantity, uint16_t *pui16_Registers);
/** @brief Execute FC05 for one configured digital output. */
E_ModbusTcpResult fn_ModbusTcp_WriteSingleCoil(S_ModbusTcpClient *pst_Client, uint8_t ui8_UnitId,
                                               uint16_t ui16_Address, bool b_Value);
/** @brief Execute FC06 for one configured analog/output register. */
E_ModbusTcpResult fn_ModbusTcp_WriteSingleRegister(S_ModbusTcpClient *pst_Client, uint8_t ui8_UnitId,
                                                   uint16_t ui16_Address, uint16_t ui16_Value);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_TCP_CLIENT_H */
