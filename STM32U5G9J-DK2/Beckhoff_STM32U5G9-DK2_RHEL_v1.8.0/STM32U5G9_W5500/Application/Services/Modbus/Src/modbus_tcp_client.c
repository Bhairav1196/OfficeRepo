/**
 * @file    modbus_tcp_client.c
 * @brief   Modbus TCP client implementation over the W5500 transport.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 *
 * @details Socket ownership belongs to the RTOS transport layer. This module
 *          never closes or reconnects socket 0. It only reports protocol and
 *          transport results so the single transport owner can make a
 *          deliberate recovery decision.
 */

#include "modbus_tcp_client.h"
#include "network_config.h"
#include "w5500_driver.h"
#include <stddef.h>
#include <string.h>

#define MBTCP_MAX_ADU (260U)
#define MBTCP_FC_READ_COILS (0x01U)
#define MBTCP_FC_READ_DISCRETE_INPUTS (0x02U)
#define MBTCP_FC_READ_HOLDING_REGISTERS (0x03U)
#define MBTCP_FC_READ_INPUT_REGISTERS (0x04U)
#define MBTCP_FC_WRITE_SINGLE_COIL (0x05U)
#define MBTCP_FC_WRITE_SINGLE_REGISTER (0x06U)

static void fn_ModbusTcp_PutU16Be(uint8_t *p, uint16_t v) { p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static uint16_t fn_ModbusTcp_GetU16Be(const uint8_t *p) { return (uint16_t)(((uint16_t)p[0]<<8)|p[1]); }

static E_ModbusTcpResult fn_ModbusTcp_RecordTransportResult(S_ModbusTcpClient *pst_Client,
                                                            E_W5500_Result e_Result)
{
    pst_Client->i32_LastW5500Result = (int32_t)e_Result;
    return (e_Result == E_W5500_OK) ? E_MBTCP_OK : E_MBTCP_TRANSPORT_ERROR;
}

void fn_ModbusTcp_Init(S_ModbusTcpClient *pst_Client)
{
    if (pst_Client != NULL)
    {
        (void)memset(pst_Client, 0, sizeof(*pst_Client));
        pst_Client->i32_LastW5500Result = (int32_t)E_W5500_OK;
    }
}

static E_ModbusTcpResult fn_ModbusTcp_Exchange(S_ModbusTcpClient *pst_Client, uint8_t unit,
                                                const uint8_t *pdu, uint16_t pdu_len,
                                                uint8_t *response_pdu, uint16_t response_capacity,
                                                uint16_t *response_len)
{
    uint8_t tx[MBTCP_MAX_ADU];
    uint8_t header[7];
    uint8_t received_pdu[254];
    uint16_t tid, length, remaining;
    E_W5500_Result e_W5500;

    if ((pst_Client==NULL)||(pdu==NULL)||(response_pdu==NULL)||(response_len==NULL)||(pdu_len==0U)||(pdu_len>253U))
    {
        return E_MBTCP_INVALID_ARGUMENT;
    }

    pst_Client->ui8_LastUnitId = unit;
    pst_Client->ui8_LastFunctionCode = pdu[0];
    pst_Client->ui16_LastAddress = (pdu_len >= 3U) ? fn_ModbusTcp_GetU16Be(&pdu[1]) : 0U;
    pst_Client->ui16_LastQuantity = (pdu_len >= 5U) ? fn_ModbusTcp_GetU16Be(&pdu[3]) : 0U;
    pst_Client->ui8_LastException = 0U;
    pst_Client->i32_LastW5500Result = (int32_t)E_W5500_OK;

    tid=++pst_Client->ui16_TransactionId;
    fn_ModbusTcp_PutU16Be(&tx[0], tid);
    fn_ModbusTcp_PutU16Be(&tx[2], 0U);
    fn_ModbusTcp_PutU16Be(&tx[4], (uint16_t)(pdu_len+1U));
    tx[6]=unit;
    (void)memcpy(&tx[7], pdu, pdu_len);

    e_W5500 = fn_W5500_Send(tx, (uint16_t)(7U+pdu_len), NET_CFG_TRANSACTION_TIMEOUT_MS);
    if (e_W5500 != E_W5500_OK)
    {
        return fn_ModbusTcp_RecordTransportResult(pst_Client, e_W5500);
    }

    e_W5500 = fn_W5500_RecvExact(header, 7U, NET_CFG_TRANSACTION_TIMEOUT_MS);
    if (e_W5500 != E_W5500_OK)
    {
        return fn_ModbusTcp_RecordTransportResult(pst_Client, e_W5500);
    }

    length=fn_ModbusTcp_GetU16Be(&header[4]);
    if ((length<2U)||(length>254U))
    {
        return E_MBTCP_PROTOCOL_ERROR;
    }

    remaining=(uint16_t)(length-1U);
    /* TCP is a byte stream. Consume the complete MBAP frame before validating
     * its identity so a stale/mismatched response cannot leave its PDU queued
     * and permanently shift every subsequent 7-byte header read. */
    e_W5500 = fn_W5500_RecvExact(received_pdu, remaining, NET_CFG_TRANSACTION_TIMEOUT_MS);
    if (e_W5500 != E_W5500_OK)
    {
        return fn_ModbusTcp_RecordTransportResult(pst_Client, e_W5500);
    }

    if ((fn_ModbusTcp_GetU16Be(&header[0])!=tid)||
        (fn_ModbusTcp_GetU16Be(&header[2])!=0U)||
        (header[6]!=unit)||
        (remaining>response_capacity))
    {
        return E_MBTCP_PROTOCOL_ERROR;
    }

    (void)memcpy(response_pdu, received_pdu, remaining);

    *response_len=remaining;
    if ((response_pdu[0] & 0x80U)!=0U)
    {
        if ((remaining != 2U) || (response_pdu[0] != (uint8_t)(pdu[0] | 0x80U)))
        {
            return E_MBTCP_PROTOCOL_ERROR;
        }
        pst_Client->ui8_LastException=response_pdu[1];
        return E_MBTCP_EXCEPTION;
    }
    return E_MBTCP_OK;
}

E_ModbusTcpResult fn_ModbusTcp_ReadBits(S_ModbusTcpClient *pst_Client, uint8_t unit_id, uint8_t function,
                                         uint16_t address, uint16_t quantity, uint16_t *mask)
{
    uint8_t req[5], rsp[254]; uint16_t rsp_len=0U, i; E_ModbusTcpResult r; uint16_t m=0U;
    if ((mask==NULL)||(quantity==0U)||(quantity>16U)||((uint32_t)address+quantity>65536U)||
        ((function!=MBTCP_FC_READ_COILS)&&(function!=MBTCP_FC_READ_DISCRETE_INPUTS)))
    {
        return E_MBTCP_INVALID_ARGUMENT;
    }
    req[0]=function; fn_ModbusTcp_PutU16Be(&req[1],address); fn_ModbusTcp_PutU16Be(&req[3],quantity);
    r=fn_ModbusTcp_Exchange(pst_Client,unit_id,req,5U,rsp,sizeof(rsp),&rsp_len); if(r!=E_MBTCP_OK)return r;
    if ((rsp_len<2U)||(rsp[0]!=function)||(rsp[1]!=(uint8_t)((quantity+7U)/8U))||
        (rsp_len!=(uint16_t)(2U+rsp[1])))
    {
        return E_MBTCP_PROTOCOL_ERROR;
    }
    for(i=0U;i<quantity;++i) if ((rsp[2U+(i/8U)] & (uint8_t)(1U<<(i%8U)))!=0U) m|=(uint16_t)(1U<<i);
    *mask=m; return E_MBTCP_OK;
}

E_ModbusTcpResult fn_ModbusTcp_ReadRegisters(S_ModbusTcpClient *pst_Client, uint8_t unit_id, uint8_t function,
                                              uint16_t address, uint16_t quantity, uint16_t *registers)
{
    uint8_t req[5], rsp[254]; uint16_t rsp_len=0U,i; E_ModbusTcpResult r;
    if ((registers==NULL)||(quantity==0U)||(quantity>125U)||((uint32_t)address+quantity>65536U)||
        ((function!=MBTCP_FC_READ_HOLDING_REGISTERS)&&(function!=MBTCP_FC_READ_INPUT_REGISTERS)))
    {
        return E_MBTCP_INVALID_ARGUMENT;
    }
    req[0]=function; fn_ModbusTcp_PutU16Be(&req[1],address); fn_ModbusTcp_PutU16Be(&req[3],quantity);
    r=fn_ModbusTcp_Exchange(pst_Client,unit_id,req,5U,rsp,sizeof(rsp),&rsp_len); if(r!=E_MBTCP_OK)return r;
    if ((rsp_len!=(uint16_t)(2U+2U*quantity))||(rsp[0]!=function)||(rsp[1]!=(uint8_t)(2U*quantity)))
    {
        return E_MBTCP_PROTOCOL_ERROR;
    }
    for(i=0U;i<quantity;++i) registers[i]=fn_ModbusTcp_GetU16Be(&rsp[2U+2U*i]);
    return E_MBTCP_OK;
}

static E_ModbusTcpResult fn_ModbusTcp_WriteSingle(S_ModbusTcpClient *pst_Client,uint8_t unit,uint8_t function,
                                                   uint16_t address,uint16_t value)
{
    uint8_t req[5],rsp[16];uint16_t rsp_len=0U;E_ModbusTcpResult r;
    req[0]=function;fn_ModbusTcp_PutU16Be(&req[1],address);fn_ModbusTcp_PutU16Be(&req[3],value);
    r=fn_ModbusTcp_Exchange(pst_Client,unit,req,5U,rsp,sizeof(rsp),&rsp_len);if(r!=E_MBTCP_OK)return r;
    if ((rsp_len!=5U)||(memcmp(req,rsp,5U)!=0))
    {
        return E_MBTCP_PROTOCOL_ERROR;
    }
    return E_MBTCP_OK;
}

E_ModbusTcpResult fn_ModbusTcp_WriteSingleCoil(S_ModbusTcpClient *pst_Client,uint8_t unit_id,uint16_t address,bool value)
{ return fn_ModbusTcp_WriteSingle(pst_Client,unit_id,MBTCP_FC_WRITE_SINGLE_COIL,address,value?0xFF00U:0x0000U); }
E_ModbusTcpResult fn_ModbusTcp_WriteSingleRegister(S_ModbusTcpClient *pst_Client,uint8_t unit_id,uint16_t address,uint16_t value)
{ return fn_ModbusTcp_WriteSingle(pst_Client,unit_id,MBTCP_FC_WRITE_SINGLE_REGISTER,address,value); }
