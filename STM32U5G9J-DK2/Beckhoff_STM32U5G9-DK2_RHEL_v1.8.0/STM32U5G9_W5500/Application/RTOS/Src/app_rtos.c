/**
 * @file    app_rtos.c
 * @brief   Application ThreadX orchestration and single-owner Modbus transport.
 * @author  Prathamesh Chipkar
 * @date    11-Sep-2026
 * @version 2.0.0-alpha.9.0
 *
 * @details
 * The transport task is the only task allowed to touch W5500/TCP/Modbus.
 * Output requests are serialized through a ThreadX queue. The diagnostics and
 * supervisor tasks never access the W5500 directly. TouchGFX remains in the
 * ST-generated GUI task and consumes only hmi_service.h.
 */

#include "app_rtos.h"
#include "app_config.h"
#include "network_config.h"
#include "modbus_config.h"
#include "io_config.h"
#include "io_service.h"
#include "diagnostics_service.h"
#include "hmi_service.h"
#include "modbus_tcp_client.h"
#include "w5500_driver.h"
#include "app_watchdog.h"
#include "stm32u5xx_hal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef enum
{
    E_APP_CMD_DIGITAL = 1,
    E_APP_CMD_ANALOG = 2
} E_AppCommandType;

typedef struct
{
    uint32_t ui32_Type;
    uint32_t ui32_SignalId;
    uint32_t ui32_ValueBits;
    uint32_t ui32_Reserved;
} S_AppCommand;

#define APP_COMMAND_WORDS          ((UINT)(sizeof(S_AppCommand) / sizeof(ULONG)))
#define APP_STALE_DATA_TIMEOUT_MS  (1000U)

_Static_assert((sizeof(S_AppCommand) % sizeof(ULONG)) == 0U,
               "ThreadX queue message must be an exact number of ULONG words");
_Static_assert(APP_COMMAND_WORDS <= 16U,
               "ThreadX queues support at most 16 ULONG words per message");
_Static_assert((APP_CFG_TRANSPORT_STACK_BYTES % sizeof(ULONG)) == 0U,
               "Transport stack must align to ULONG");
_Static_assert((APP_CFG_DIAG_STACK_BYTES % sizeof(ULONG)) == 0U,
               "Diagnostics stack must align to ULONG");
_Static_assert((APP_CFG_SUPERVISOR_STACK_BYTES % sizeof(ULONG)) == 0U,
               "Supervisor stack must align to ULONG");
_Static_assert(NET_CFG_SOURCE_PORT_COUNT > 0U,
               "Source-port rotation range must not be empty");

static TX_THREAD s_transport_thread;
static TX_THREAD s_diagnostics_thread;
static TX_THREAD s_supervisor_thread;
static ULONG s_transport_stack[APP_CFG_TRANSPORT_STACK_BYTES / sizeof(ULONG)];
static ULONG s_diagnostics_stack[APP_CFG_DIAG_STACK_BYTES / sizeof(ULONG)];
static ULONG s_supervisor_stack[APP_CFG_SUPERVISOR_STACK_BYTES / sizeof(ULONG)];
static TX_MUTEX s_data_mutex;
static TX_QUEUE s_command_queue;
static ULONG s_queue_storage[APP_CFG_COMMAND_QUEUE_DEPTH * APP_COMMAND_WORDS];

/**
 * @brief Published debug snapshot. Add `s_snapshot` to CubeIDE Expressions.
 * @note  Writes occur only through fn_AppRtos_PublishSnapshot().
 */
static S_AppDiagnosticSnapshot s_snapshot;
static S_ModbusTcpClient s_modbus_client;
static bool s_created;
static volatile uint32_t s_ui32_CommunicationUptimeSec;
static volatile uint32_t s_ui32_TransportHeartbeat;
static volatile uint32_t s_ui32_DiagnosticsHeartbeat;
static volatile uint32_t s_ui32_SupervisorHeartbeat;
static volatile uint32_t s_ui32_CommandQueueDrops;

static uint32_t fn_AppRtos_TicksFromMs(uint32_t ui32_Ms)
{
    const uint64_t ui64_Ticks = ((uint64_t)ui32_Ms * (uint64_t)TX_TIMER_TICKS_PER_SECOND + 999ULL) / 1000ULL;
    return (uint32_t)((ui64_Ticks == 0ULL) ? 1ULL : ui64_Ticks);
}

static uint16_t fn_AppRtos_NextSourcePort(uint16_t ui16_Current)
{
    const uint32_t ui32_Base = NET_CFG_SOURCE_PORT_BASE;
    const uint32_t ui32_Count = NET_CFG_SOURCE_PORT_COUNT;
    uint32_t ui32_Offset;

    if (((uint32_t)ui16_Current < ui32_Base) ||
        ((uint32_t)ui16_Current >= (ui32_Base + ui32_Count)))
    {
        return (uint16_t)ui32_Base;
    }
    ui32_Offset = ((uint32_t)ui16_Current - ui32_Base + 1U) % ui32_Count;
    return (uint16_t)(ui32_Base + ui32_Offset);
}

static void fn_AppRtos_PublishSnapshot(S_AppDiagnosticSnapshot *pst_Current)
{
    if (pst_Current == NULL)
    {
        return;
    }

    pst_Current->ui32_CommunicationUptimeSec = s_ui32_CommunicationUptimeSec;
    pst_Current->ui32_DiagnosticsCycleCounter = s_ui32_DiagnosticsHeartbeat;
    pst_Current->ui32_SupervisorCycleCounter = s_ui32_SupervisorHeartbeat;
    pst_Current->ui32_CommandQueueDrops = s_ui32_CommandQueueDrops;
    pst_Current->ui32_SnapshotTimestampMs = HAL_GetTick();

    if (tx_mutex_get(&s_data_mutex, TX_WAIT_FOREVER) == TX_SUCCESS)
    {
        s_snapshot = *pst_Current;
        (void)tx_mutex_put(&s_data_mutex);
    }
}

static void fn_AppRtos_SetAllIoQuality(E_IO_Quality e_Quality)
{
    if (tx_mutex_get(&s_data_mutex, TX_WAIT_FOREVER) == TX_SUCCESS)
    {
        fn_IOService_SetCommunicationQuality(e_Quality);
        (void)tx_mutex_put(&s_data_mutex);
    }
}

static void fn_AppRtos_RecordModbusResult(S_AppDiagnosticSnapshot *pst_Current,
                                          E_ModbusTcpResult e_Result)
{
    if (pst_Current == NULL)
    {
        return;
    }

    pst_Current->ui16_LastTransactionId = s_modbus_client.ui16_TransactionId;
    pst_Current->i32_LastW5500Result = s_modbus_client.i32_LastW5500Result;

    if (e_Result == E_MBTCP_OK)
    {
        ++pst_Current->ui32_SuccessfulTransactions;
        pst_Current->ui8_LastModbusException = 0U;
        return;
    }

    pst_Current->ui8_LastFailedUnitId = s_modbus_client.ui8_LastUnitId;
    pst_Current->ui8_LastFailedFunctionCode = s_modbus_client.ui8_LastFunctionCode;
    pst_Current->ui16_LastFailedAddress = s_modbus_client.ui16_LastAddress;
    pst_Current->ui16_LastFailedQuantity = s_modbus_client.ui16_LastQuantity;

    if (e_Result == E_MBTCP_EXCEPTION)
    {
        /* A valid Modbus exception is an application-level response, not a
         * malformed TCP/Modbus frame and therefore must not influence the
         * TCP reconnect policy. */
        pst_Current->ui8_LastModbusException = s_modbus_client.ui8_LastException;
        ++pst_Current->ui32_ModbusExceptions;
    }
    else if (e_Result == E_MBTCP_PROTOCOL_ERROR)
    {
        ++pst_Current->ui32_ProtocolErrors;
    }
    else
    {
        ++pst_Current->ui32_TransportErrors;
    }
}

static bool fn_AppRtos_ReadBits(S_AppDiagnosticSnapshot *pst_Current,
                                uint8_t ui8_UnitId, uint8_t ui8_Function,
                                E_IO_SignalType e_Type, uint16_t *pui16_Destination)
{
    E_ModbusTcpResult e_Result;

    e_Result = fn_ModbusTcp_ReadBits(&s_modbus_client, ui8_UnitId, ui8_Function,
                                     MODBUS_CFG_ADDR_DI, MODBUS_CFG_QTY_DI,
                                     pui16_Destination);
    fn_AppRtos_RecordModbusResult(pst_Current, e_Result);
    if (e_Result == E_MBTCP_OK)
    {
        if (tx_mutex_get(&s_data_mutex, TX_WAIT_FOREVER) == TX_SUCCESS)
        {
            fn_IOService_UpdateDigitalMask(ui8_UnitId, e_Type, *pui16_Destination, HAL_GetTick());
            (void)tx_mutex_put(&s_data_mutex);
        }
        return true;
    }
    return false;
}

static bool fn_AppRtos_ReadSignedRegisters(S_AppDiagnosticSnapshot *pst_Current,
                                           uint8_t ui8_UnitId, uint8_t ui8_Function,
                                           E_IO_SignalType e_Type, uint16_t ui16_Address,
                                           uint16_t ui16_Quantity, int16_t *pi16_Destination,
                                           bool b_UpdateIoService)
{
    uint16_t aui16_Words[DIAG_AI_CHANNELS];
    uint16_t ui16_Index;
    E_ModbusTcpResult e_Result;

    _Static_assert(DIAG_AO_CHANNELS <= DIAG_AI_CHANNELS,
                   "Signed-register scratch buffer must cover AO channels");

    if ((pi16_Destination == NULL) || (ui16_Quantity > DIAG_AI_CHANNELS))
    {
        return false;
    }

    e_Result = fn_ModbusTcp_ReadRegisters(&s_modbus_client, ui8_UnitId, ui8_Function,
                                          ui16_Address, ui16_Quantity, aui16_Words);
    fn_AppRtos_RecordModbusResult(pst_Current, e_Result);
    if (e_Result != E_MBTCP_OK)
    {
        return false;
    }

    for (ui16_Index = 0U; ui16_Index < ui16_Quantity; ++ui16_Index)
    {
        pi16_Destination[ui16_Index] = (int16_t)aui16_Words[ui16_Index];
    }

    if (b_UpdateIoService && (tx_mutex_get(&s_data_mutex, TX_WAIT_FOREVER) == TX_SUCCESS))
    {
        fn_IOService_UpdateAnalogRegisters(ui8_UnitId, e_Type, ui16_Address,
                                           pi16_Destination, ui16_Quantity, HAL_GetTick());
        (void)tx_mutex_put(&s_data_mutex);
    }
    return true;
}

static bool fn_AppRtos_ReadRegisters(S_AppDiagnosticSnapshot *pst_Current,
                                     uint8_t ui8_UnitId, uint8_t ui8_Function,
                                     uint16_t ui16_Address, uint16_t ui16_Quantity,
                                     uint16_t *pui16_Destination)
{
    const E_ModbusTcpResult e_Result = fn_ModbusTcp_ReadRegisters(&s_modbus_client,
                                                                  ui8_UnitId,
                                                                  ui8_Function,
                                                                  ui16_Address,
                                                                  ui16_Quantity,
                                                                  pui16_Destination);
    fn_AppRtos_RecordModbusResult(pst_Current, e_Result);
    return e_Result == E_MBTCP_OK;
}

static bool fn_AppRtos_ProcessCommand(S_AppDiagnosticSnapshot *pst_Current,
                                      const S_AppCommand *pst_Command)
{
    const S_IO_SignalConfig *pst_Config;
    E_ModbusTcpResult e_Result;

    if ((pst_Current == NULL) || (pst_Command == NULL))
    {
        return true;
    }

    pst_Config = fn_IOConfig_FindById((uint16_t)pst_Command->ui32_SignalId);
    if ((pst_Config == NULL) || !pst_Config->b_Enabled || !pst_Config->b_Writable)
    {
        return true;
    }

    if (pst_Command->ui32_Type == (uint32_t)E_APP_CMD_DIGITAL)
    {
        const uint16_t ui16_Address = (uint16_t)(pst_Config->ui16_RegisterAddress + pst_Config->ui8_BitIndex);
        e_Result = fn_ModbusTcp_WriteSingleCoil(&s_modbus_client,
                                                pst_Config->ui8_UnitId,
                                                ui16_Address,
                                                pst_Command->ui32_ValueBits != 0U);
    }
    else if (pst_Command->ui32_Type == (uint32_t)E_APP_CMD_ANALOG)
    {
        float f32_Requested = 0.0F;
        int16_t i16_Register = 0;
        E_IO_Status e_IoStatus;

        (void)memcpy(&f32_Requested, &pst_Command->ui32_ValueBits, sizeof(f32_Requested));
        e_IoStatus = fn_IOService_ProcessToRegister(pst_Config->ui16_SignalId,
                                                    f32_Requested,
                                                    &i16_Register,
                                                    NULL);
        if (e_IoStatus != E_IO_STATUS_OK)
        {
            return true;
        }
        e_Result = fn_ModbusTcp_WriteSingleRegister(&s_modbus_client,
                                                    pst_Config->ui8_UnitId,
                                                    pst_Config->ui16_RegisterAddress,
                                                    (uint16_t)i16_Register);
    }
    else
    {
        return true;
    }

    fn_AppRtos_RecordModbusResult(pst_Current, e_Result);
    return e_Result == E_MBTCP_OK;
}

static bool fn_AppRtos_CheckPhysicalLink(S_AppDiagnosticSnapshot *pst_Current,
                                         uint32_t *pui32_LinkDownSamples,
                                         uint32_t *pui32_PhyIoErrorSamples)
{
    uint8_t ui8_Phy;
    E_W5500_LinkStatus e_Link;

    if ((pst_Current == NULL) || (pui32_LinkDownSamples == NULL) ||
        (pui32_PhyIoErrorSamples == NULL))
    {
        return false;
    }

    ui8_Phy = pst_Current->ui8_LastPhyCfgr;
    e_Link = fn_W5500_GetLinkStatus(&ui8_Phy);

    if (e_Link == E_W5500_LINK_STATUS_UP)
    {
        pst_Current->ui8_LastPhyCfgr = ui8_Phy;
        pst_Current->b_EthernetLinkUp = true;
        *pui32_LinkDownSamples = 0U;
        *pui32_PhyIoErrorSamples = 0U;
        return true;
    }

    if (e_Link == E_W5500_LINK_STATUS_IO_ERROR)
    {
        ++pst_Current->ui32_PhyReadErrors;
        ++(*pui32_PhyIoErrorSamples);

        /*
         * Alpha.6.2: a failed PHY register read is diagnostic evidence only.
         * It must never issue DISCON while Socket 0 is otherwise alive. A
         * transient SPI/PHYCFGR read error was capable of creating a clean FIN
         * from the STM32 even though Modbus traffic was still valid.
         */
        if (*pui32_PhyIoErrorSamples >= APP_CFG_LINK_DEBOUNCE_SAMPLES)
        {
            *pui32_PhyIoErrorSamples = APP_CFG_LINK_DEBOUNCE_SAMPLES;
            ++pst_Current->ui32_PhySocketDisagreementEvents;
        }
        return pst_Current->b_EthernetLinkUp;
    }

    *pui32_PhyIoErrorSamples = 0U;
    ++(*pui32_LinkDownSamples);
    if (*pui32_LinkDownSamples >= APP_CFG_LINK_DEBOUNCE_SAMPLES)
    {
        bool b_SocketConnected = false;
        uint8_t ui8_SocketStatus = pst_Current->ui8_LastSocketStatus;
        uint8_t ui8_SocketInterrupt = pst_Current->ui8_LastSocketInterrupt;
        const E_W5500_Result e_Socket = fn_W5500_GetConnectionStatus(
            &b_SocketConnected, &ui8_SocketStatus, &ui8_SocketInterrupt);

        pst_Current->ui8_LastSocketStatus = ui8_SocketStatus;
        pst_Current->ui8_LastSocketInterrupt = ui8_SocketInterrupt;

        if ((e_Socket == E_W5500_OK) && b_SocketConnected)
        {
            /* PHYCFGR and Sn_SR disagree: trust the established TCP socket and
             * suppress an application-generated FIN. */
            ++pst_Current->ui32_PhySocketDisagreementEvents;
            *pui32_LinkDownSamples = 0U;
            pst_Current->b_EthernetLinkUp = true;
            return true;
        }

        if (pst_Current->b_EthernetLinkUp)
        {
            ++pst_Current->ui32_PhysicalLinkDownEvents;
        }
        pst_Current->b_EthernetLinkUp = false;
        return false;
    }

    /* One or two transient PHY-down samples do not destroy a healthy TCP session. */
    return pst_Current->b_EthernetLinkUp;
}

static bool fn_AppRtos_CheckSocket(S_AppDiagnosticSnapshot *pst_Current,
                                   uint32_t *pui32_SocketReadErrors,
                                   uint32_t *pui32_SocketDownSamples)
{
    bool b_Connected = false;
    uint8_t ui8_Status;
    uint8_t ui8_Interrupt;
    E_W5500_Result e_Result;

    if ((pst_Current == NULL) || (pui32_SocketReadErrors == NULL) ||
        (pui32_SocketDownSamples == NULL))
    {
        return false;
    }

    ui8_Status = pst_Current->ui8_LastSocketStatus;
    ui8_Interrupt = pst_Current->ui8_LastSocketInterrupt;
    e_Result = fn_W5500_GetConnectionStatus(&b_Connected, &ui8_Status, &ui8_Interrupt);

    if (e_Result != E_W5500_OK)
    {
        ++pst_Current->ui32_TransportErrors;
        ++(*pui32_SocketReadErrors);

        /*
         * Alpha.6.2: never turn an inability to sample Sn_SR/Sn_IR into an
         * active DISCON command. Keep the last known state and let a later
         * positive socket-state observation decide whether reconnect is needed.
         */
        if (*pui32_SocketReadErrors >= APP_CFG_LINK_DEBOUNCE_SAMPLES)
        {
            *pui32_SocketReadErrors = APP_CFG_LINK_DEBOUNCE_SAMPLES;
            ++pst_Current->ui32_SocketStatusReadSuppressedCloses;
        }
        return pst_Current->b_TcpConnected;
    }

    *pui32_SocketReadErrors = 0U;
    pst_Current->ui8_LastSocketStatus = ui8_Status;
    pst_Current->ui8_LastSocketInterrupt = ui8_Interrupt;

    if (b_Connected)
    {
        *pui32_SocketDownSamples = 0U;
        pst_Current->b_TcpConnected = true;
        return true;
    }

    if (!pst_Current->b_TcpConnected)
    {
        *pui32_SocketDownSamples = 0U;
        return false;
    }

    /* CLOSE_WAIT is positive evidence that the remote endpoint sent FIN. */
    if (ui8_Status == 0x1CU)
    {
        *pui32_SocketDownSamples = APP_CFG_LINK_DEBOUNCE_SAMPLES;
    }
    else
    {
        ++(*pui32_SocketDownSamples);
    }

    if (*pui32_SocketDownSamples < APP_CFG_LINK_DEBOUNCE_SAMPLES)
    {
        /*
         * One isolated Sn_SR sample must not trigger reconnect. The previous
         * code could observe one transient non-ESTABLISHED value, mark the
         * session down, then fn_W5500_Connect() would see ESTABLISHED again
         * and actively DISCON the valid socket.
         */
        ++pst_Current->ui32_SocketStateDisagreementEvents;
        return true;
    }

    ++pst_Current->ui32_SocketDisconnectEvents;
    pst_Current->e_LastDisconnectReason = E_TCP_DISCONNECT_REMOTE_SOCKET_CLOSE;
    pst_Current->ui32_LastDisconnectTimestampMs = HAL_GetTick();
    pst_Current->ui32_LastTcpSessionDurationMs = pst_Current->ui32_CurrentTcpSessionDurationMs;
    pst_Current->b_TcpConnected = false;
    *pui32_SocketDownSamples = 0U;
    return false;
}

static void fn_AppRtos_TransportThread(ULONG ul_Argument)
{
    S_AppDiagnosticSnapshot st_Current;
    uint32_t ui32_DiagnosticCycle = 0U;
    uint32_t ui32_ConsecutiveTransportFailures = 0U;
    uint32_t ui32_ConsecutiveProtocolFailures = 0U;
    uint32_t ui32_ConnectFailures = 0U;
    uint32_t ui32_SessionStartMs = 0U;
    uint32_t ui32_LinkDownSamples = 0U;
    uint32_t ui32_PhyIoErrorSamples = 0U;
    uint32_t ui32_SocketReadErrors = 0U;
    uint32_t ui32_SocketDownSamples = 0U;
    uint16_t ui16_SourcePort = NET_CFG_SOURCE_PORT_BASE;
    const uint8_t aui8_Server[4] = {NET_CFG_RHEL_IP_0, NET_CFG_RHEL_IP_1,
                                    NET_CFG_RHEL_IP_2, NET_CFG_RHEL_IP_3};
    const S_W5500_NetInfo st_Network =
    {
        {NET_CFG_LOCAL_MAC_0, NET_CFG_LOCAL_MAC_1, NET_CFG_LOCAL_MAC_2,
         NET_CFG_LOCAL_MAC_3, NET_CFG_LOCAL_MAC_4, NET_CFG_LOCAL_MAC_5},
        {NET_CFG_LOCAL_IP_0, NET_CFG_LOCAL_IP_1, NET_CFG_LOCAL_IP_2, NET_CFG_LOCAL_IP_3},
        {NET_CFG_SUBNET_0, NET_CFG_SUBNET_1, NET_CFG_SUBNET_2, NET_CFG_SUBNET_3},
        {NET_CFG_GATEWAY_0, NET_CFG_GATEWAY_1, NET_CFG_GATEWAY_2, NET_CFG_GATEWAY_3}
    };

    (void)ul_Argument;
    fn_Diagnostics_Init(&st_Current);
    st_Current.e_IoConfigStatus = fn_IOConfig_ValidateAll();
    fn_ModbusTcp_Init(&s_modbus_client);
    fn_AppRtos_PublishSnapshot(&st_Current);

    for (;;)
    {
        uint32_t ui32_StartMs;
        uint32_t ui32_TransportErrorsBefore;
        uint32_t ui32_ProtocolErrorsBefore;
        bool b_CycleOk = true;
        S_AppCommand st_Command;

        ++s_ui32_TransportHeartbeat;

        if (st_Current.e_IoConfigStatus != E_IO_CONFIG_OK)
        {
            fn_AppRtos_SetAllIoQuality(E_IO_QUALITY_CONFIG_ERROR);
            fn_AppRtos_PublishSnapshot(&st_Current);
            tx_thread_sleep(fn_AppRtos_TicksFromMs(1000U));
            continue;
        }

        if (!st_Current.b_W5500Ready)
        {
            const E_W5500_Result e_Init = fn_W5500_Init(&st_Network);
            st_Current.b_W5500Ready = (e_Init == E_W5500_OK);
            st_Current.b_TcpConnected = false;
            st_Current.b_EthernetLinkUp = false;

            if (!st_Current.b_W5500Ready)
            {
                ++st_Current.ui32_W5500InitFailures;
                ++st_Current.ui32_TransportErrors;
                fn_AppRtos_SetAllIoQuality(E_IO_QUALITY_COMM_ERROR);
                fn_AppRtos_PublishSnapshot(&st_Current);
                tx_thread_sleep(fn_AppRtos_TicksFromMs(NET_CFG_W5500_INIT_RETRY_MS));
                continue;
            }
            ui32_ConsecutiveTransportFailures = 0U;
            ui32_ConsecutiveProtocolFailures = 0U;
            ui32_ConnectFailures = 0U;
            ui32_LinkDownSamples = 0U;
            ui32_PhyIoErrorSamples = 0U;
            ui32_SocketReadErrors = 0U;
            ui32_SocketDownSamples = 0U;
        }

        if (!fn_AppRtos_CheckPhysicalLink(&st_Current,
                                          &ui32_LinkDownSamples,
                                          &ui32_PhyIoErrorSamples))
        {
            fn_AppRtos_SetAllIoQuality(E_IO_QUALITY_COMM_ERROR);
            fn_AppRtos_PublishSnapshot(&st_Current);
            tx_thread_sleep(fn_AppRtos_TicksFromMs(NET_CFG_RECONNECT_BACKOFF_MS));
            continue;
        }

        if (!fn_AppRtos_CheckSocket(&st_Current, &ui32_SocketReadErrors,
                                    &ui32_SocketDownSamples))
        {
            E_W5500_Result e_Connect;

            if (!st_Current.b_W5500Ready)
            {
                fn_AppRtos_PublishSnapshot(&st_Current);
                continue;
            }

            ui16_SourcePort = fn_AppRtos_NextSourcePort(ui16_SourcePort);
            ++st_Current.ui32_ReconnectCount;
            ++st_Current.ui32_ConnectAttempts;
            e_Connect = fn_W5500_Connect(aui8_Server,
                                        NET_CFG_RHEL_TCP_PORT,
                                        ui16_SourcePort,
                                        NET_CFG_CONNECT_TIMEOUT_MS);
            if (e_Connect != E_W5500_OK)
            {
                /* Alpha.7: SYN rejection / server absence is an endpoint
                 * availability condition. Never hardware-reset a W5500 that
                 * still passes SPI/version/link checks merely because RHEL
                 * refused TCP port 502. Keep static IP state and retry with a
                 * fresh source port after the bounded backoff. */
                ++st_Current.ui32_TransportErrors;
                ++st_Current.ui32_ConnectFailures;
                ++ui32_ConnectFailures;
                st_Current.ui32_ConsecutiveConnectFailures = ui32_ConnectFailures;
                st_Current.i32_LastW5500Result = (int32_t)e_Connect;
                fn_AppRtos_SetAllIoQuality(E_IO_QUALITY_COMM_ERROR);
                fn_AppRtos_PublishSnapshot(&st_Current);
                tx_thread_sleep(fn_AppRtos_TicksFromMs(NET_CFG_RECONNECT_BACKOFF_MS));
                continue;
            }

            ui32_ConsecutiveTransportFailures = 0U;
            ui32_ConsecutiveProtocolFailures = 0U;
            ui32_ConnectFailures = 0U;
            st_Current.ui32_ConsecutiveConnectFailures = 0U;
            st_Current.b_TcpConnected = true;
            st_Current.ui32_CurrentTcpSessionDurationMs = 0U;
            ui32_SessionStartMs = HAL_GetTick();
            fn_ModbusTcp_Init(&s_modbus_client);
        }

        ui32_StartMs = HAL_GetTick();
        ui32_TransportErrorsBefore = st_Current.ui32_TransportErrors;
        ui32_ProtocolErrorsBefore = st_Current.ui32_ProtocolErrors;
        st_Current.ui32_CurrentTcpSessionDurationMs =
            st_Current.b_TcpConnected ? (uint32_t)(ui32_StartMs - ui32_SessionStartMs) : 0U;

        /* Stop the cycle immediately after the first failed transaction. */
        b_CycleOk = fn_AppRtos_ReadBits(&st_Current, MODBUS_CFG_UNIT, 0x02U,
                                        E_IO_SIGNAL_DI, &st_Current.ui16_DiMask);
        if (b_CycleOk) b_CycleOk = fn_AppRtos_ReadBits(&st_Current, MODBUS_CFG_UNIT, 0x01U,
                                                       E_IO_SIGNAL_DO, &st_Current.ui16_DoMask);
        if (b_CycleOk) b_CycleOk = fn_AppRtos_ReadSignedRegisters(&st_Current, MODBUS_CFG_UNIT, 0x04U,
                                                                  E_IO_SIGNAL_AI, MODBUS_CFG_ADDR_AI_ENG,
                                                                  MODBUS_CFG_QTY_AI,
                                                                  st_Current.ai16_AiEngineeringRegister, true);
        if (b_CycleOk) b_CycleOk = fn_AppRtos_ReadSignedRegisters(&st_Current, MODBUS_CFG_UNIT, 0x03U,
                                                                  E_IO_SIGNAL_AO, MODBUS_CFG_ADDR_AO,
                                                                  MODBUS_CFG_QTY_AO,
                                                                  st_Current.ai16_AoRegister, true);

        ++ui32_DiagnosticCycle;
        if ((ui32_DiagnosticCycle % NET_CFG_DIAGNOSTIC_DIVIDER) == 0U)
        {
            if (b_CycleOk) b_CycleOk = fn_AppRtos_ReadSignedRegisters(&st_Current, MODBUS_CFG_UNIT, 0x04U,
                                                                      E_IO_SIGNAL_AI, MODBUS_CFG_ADDR_AI_RAW,
                                                                      MODBUS_CFG_QTY_AI,
                                                                      st_Current.ai16_AiRaw, false);
            if (b_CycleOk) b_CycleOk = fn_AppRtos_ReadRegisters(&st_Current, MODBUS_CFG_UNIT, 0x04U,
                                                                MODBUS_CFG_ADDR_AI_STATUS,
                                                                MODBUS_CFG_QTY_AI,
                                                                st_Current.aui16_AiStatus);
            if (b_CycleOk) b_CycleOk = fn_AppRtos_ReadRegisters(&st_Current, MODBUS_CFG_UNIT, 0x04U,
                                                                MODBUS_CFG_ADDR_AO_STATUS,
                                                                MODBUS_CFG_QTY_AO,
                                                                st_Current.aui16_AoStatus);
        }

        while (b_CycleOk &&
               (tx_queue_receive(&s_command_queue, &st_Command, TX_NO_WAIT) == TX_SUCCESS))
        {
            if (!fn_AppRtos_ProcessCommand(&st_Current, &st_Command))
            {
                b_CycleOk = false;
                break;
            }
        }

        if (st_Current.ui32_TransportErrors != ui32_TransportErrorsBefore)
        {
            ++ui32_ConsecutiveTransportFailures;
        }
        else
        {
            ui32_ConsecutiveTransportFailures = 0U;
        }

        if (st_Current.ui32_ProtocolErrors != ui32_ProtocolErrorsBefore)
        {
            ++ui32_ConsecutiveProtocolFailures;
        }
        else
        {
            ui32_ConsecutiveProtocolFailures = 0U;
        }

        st_Current.ui32_ConsecutiveTransportFailures = ui32_ConsecutiveTransportFailures;
        st_Current.ui32_ConsecutiveProtocolFailures = ui32_ConsecutiveProtocolFailures;

        if (!b_CycleOk)
        {
            const bool b_StillConnected = fn_AppRtos_CheckSocket(&st_Current,
                                                                  &ui32_SocketReadErrors,
                                                                  &ui32_SocketDownSamples);

            /* ESTABLISHED proves TCP transport state, not Modbus byte-stream
             * alignment. Reopen after a bounded run of protocol failures so
             * malformed MBAP lengths or an unrecoverable stale stream cannot
             * trap the client forever. Transport-only failures retain the
             * alpha.6.2 suppression rule while the socket is confirmed live. */
            if (b_StillConnected &&
                (ui32_ConsecutiveProtocolFailures >= APP_CFG_PROTOCOL_FAILURE_RECONNECT))
            {
                fn_W5500_Close();
                ++st_Current.ui32_IntentionalSocketCloseCount;
                ++st_Current.ui32_SocketDisconnectEvents;
                st_Current.e_LastDisconnectReason = E_TCP_DISCONNECT_PROTOCOL_FAILURE_LIMIT;
                st_Current.ui32_LastDisconnectTimestampMs = HAL_GetTick();
                st_Current.ui32_LastTcpSessionDurationMs = st_Current.ui32_CurrentTcpSessionDurationMs;
                st_Current.b_TcpConnected = false;
                ui32_ConsecutiveTransportFailures = 0U;
                ui32_ConsecutiveProtocolFailures = 0U;
            }
            else if (b_StillConnected &&
                     (ui32_ConsecutiveTransportFailures >= APP_CFG_TRANSPORT_FAILURE_RECONNECT))
            {
                ++st_Current.ui32_HealthySocketReconnectSuppressions;
                ui32_ConsecutiveTransportFailures = 0U;
            }
        }

        {
            S_W5500_Diagnostics st_Diagnostics;
            if (fn_W5500_GetDiagnostics(&st_Diagnostics) == E_W5500_OK)
            {
                st_Current.ui8_LastPhyCfgr = st_Diagnostics.ui8_PhyCfgr;
                st_Current.ui8_LastSocketStatus = st_Diagnostics.ui8_SocketStatus;
                st_Current.ui8_LastSocketInterrupt = st_Diagnostics.ui8_SocketInterrupt;
                st_Current.ui32_W5500StableReadRetries = st_Diagnostics.ui32_StableReadRetries;
                st_Current.ui32_W5500StableReadFailures = st_Diagnostics.ui32_StableReadFailures;
            }
        }

        ++st_Current.ui32_TransportCycleCounter;
        st_Current.ui32_LastCycleDurationMs = (uint32_t)(HAL_GetTick() - ui32_StartMs);
        fn_AppRtos_PublishSnapshot(&st_Current);

        if (st_Current.ui32_LastCycleDurationMs < NET_CFG_POLL_PERIOD_MS)
        {
            tx_thread_sleep(fn_AppRtos_TicksFromMs(NET_CFG_POLL_PERIOD_MS -
                                                   st_Current.ui32_LastCycleDurationMs));
        }
        else
        {
            tx_thread_relinquish();
        }
    }
}

static void fn_AppRtos_DiagnosticsThread(ULONG ul_Argument)
{
    bool b_WasConnected = false;
    uint32_t ui32_ConnectedSinceMs = 0U;

    (void)ul_Argument;
    for (;;)
    {
        S_AppDiagnosticSnapshot st_Local;
        const uint32_t ui32_Now = HAL_GetTick();

        if (fn_HMI_GetDiagnosticSnapshot(&st_Local))
        {
            if (st_Local.b_TcpConnected)
            {
                if (!b_WasConnected)
                {
                    ui32_ConnectedSinceMs = ui32_Now;
                }
                s_ui32_CommunicationUptimeSec = (uint32_t)(ui32_Now - ui32_ConnectedSinceMs) / 1000U;
            }
            else
            {
                s_ui32_CommunicationUptimeSec = 0U;
            }
            b_WasConnected = st_Local.b_TcpConnected;
        }
        ++s_ui32_DiagnosticsHeartbeat;
        tx_thread_sleep(fn_AppRtos_TicksFromMs(APP_CFG_DIAG_PERIOD_MS));
    }
}

static void fn_AppRtos_SupervisorThread(ULONG ul_Argument)
{
    uint32_t ui32_LastTransportCycle = 0U;
    uint32_t ui32_LastDiagnosticsCycle = 0U;
    bool b_HaveBaseline = false;
    (void)ul_Argument;
    for (;;)
    {
        S_AppDiagnosticSnapshot st_Local;
        const uint32_t ui32_Now = HAL_GetTick();

        if (fn_HMI_GetDiagnosticSnapshot(&st_Local))
        {
            if (st_Local.e_IoConfigStatus != E_IO_CONFIG_OK)
            {
                fn_AppRtos_SetAllIoQuality(E_IO_QUALITY_CONFIG_ERROR);
            }
            else if (!st_Local.b_TcpConnected)
            {
                fn_AppRtos_SetAllIoQuality(E_IO_QUALITY_COMM_ERROR);
            }
            else if ((uint32_t)(ui32_Now - st_Local.ui32_SnapshotTimestampMs) > APP_STALE_DATA_TIMEOUT_MS)
            {
                fn_AppRtos_SetAllIoQuality(E_IO_QUALITY_STALE);
            }

#if APP_CFG_ENABLE_IWDG
            /* A network outage is not a task failure: the transport cycle still
             * advances. Starvation, deadlock or termination of either required
             * worker prevents refresh and lets the hardware reset the MCU. */
            if (b_HaveBaseline &&
                (s_ui32_TransportHeartbeat != ui32_LastTransportCycle) &&
                (s_ui32_DiagnosticsHeartbeat != ui32_LastDiagnosticsCycle))
            {
                fn_AppWatchdog_Refresh();
            }
            ui32_LastTransportCycle = s_ui32_TransportHeartbeat;
            ui32_LastDiagnosticsCycle = s_ui32_DiagnosticsHeartbeat;
            b_HaveBaseline = true;
#endif
        }
        ++s_ui32_SupervisorHeartbeat;
        tx_thread_sleep(fn_AppRtos_TicksFromMs(APP_CFG_SUPERVISOR_PERIOD_MS));
    }
}

bool fn_HMI_GetSystemStatus(S_HMI_SystemStatus *pst_Status)
{
    if ((pst_Status == NULL) || !s_created)
    {
        return false;
    }
    if (tx_mutex_get(&s_data_mutex, TX_NO_WAIT) != TX_SUCCESS)
    {
        return false;
    }
    pst_Status->b_TcpConnected = s_snapshot.b_TcpConnected;
    pst_Status->b_EthernetLinkUp = s_snapshot.b_EthernetLinkUp;
    pst_Status->ui32_CommunicationUptimeSec = s_snapshot.ui32_CommunicationUptimeSec;
    pst_Status->e_ConfigStatus = s_snapshot.e_IoConfigStatus;
    (void)tx_mutex_put(&s_data_mutex);
    return true;
}

bool fn_HMI_GetUnitInfo(uint8_t ui8_UnitId, S_HMI_UnitInfo *pst_Info)
{
    const S_IO_UnitConfig *pst_Config;

    if ((pst_Info == NULL) || !s_created)
    {
        return false;
    }
    pst_Config = fn_IOConfig_FindUnit(ui8_UnitId);
    if (pst_Config == NULL)
    {
        return false;
    }

    pst_Info->ui8_UnitId = pst_Config->ui8_UnitId;
    pst_Info->pc_Name = pst_Config->pc_Name;
    pst_Info->pc_Description = pst_Config->pc_Description;
    pst_Info->ui8_DiCount = pst_Config->ui8_DiCount;
    pst_Info->ui8_DoCount = pst_Config->ui8_DoCount;
    pst_Info->ui8_AiCount = pst_Config->ui8_AiCount;
    pst_Info->ui8_AoCount = pst_Config->ui8_AoCount;
    pst_Info->b_Enabled = pst_Config->b_Enabled;
    return true;
}

bool fn_HMI_GetUnitSlot(uint8_t ui8_UnitId,
                        E_IO_SignalType e_Type,
                        uint8_t ui8_Slot,
                        S_HMI_SignalView *pst_View)
{
    const S_IO_SignalConfig *pst_Config;
    S_IO_SignalValue st_Value;
    const char *pc_ProcessUnit;
    const char *pc_ElectricalUnit;
    bool b_ProcessMapped;

    if ((pst_View == NULL) || !s_created)
    {
        return false;
    }

    pst_Config = fn_IOConfig_FindUnitHmiSlot(ui8_UnitId, e_Type, ui8_Slot);
    if (pst_Config == NULL)
    {
        return false;
    }

    if (tx_mutex_get(&s_data_mutex, TX_NO_WAIT) != TX_SUCCESS)
    {
        return false;
    }
    if (!fn_IOService_GetValue(pst_Config->ui16_SignalId, &st_Value))
    {
        (void)tx_mutex_put(&s_data_mutex);
        return false;
    }

    pc_ProcessUnit = (pst_Config->pc_ProcessUnit != NULL) ? pst_Config->pc_ProcessUnit : "";
    pc_ElectricalUnit = fn_IOConfig_GetElectricalUnit(pst_Config->e_AnalogMode);
    b_ProcessMapped = (pst_Config->e_MappingType != E_IO_MAPPING_NONE);

    pst_View->ui16_SignalId = pst_Config->ui16_SignalId;
    pst_View->ui8_UnitId = pst_Config->ui8_UnitId;
    pst_View->pc_Name = pst_Config->pc_Name;
    pst_View->pc_Unit = (b_ProcessMapped && (pc_ProcessUnit[0] != '\0')) ? pc_ProcessUnit : pc_ElectricalUnit;
    pst_View->e_Type = pst_Config->e_SignalType;
    pst_View->e_Quality = st_Value.e_Quality;
    pst_View->f32_ProcessValue = st_Value.f32_ProcessValue;
    pst_View->f32_ElectricalValue = st_Value.f32_ElectricalValue;
    pst_View->i32_RawValue = st_Value.i32_RawValue;
    pst_View->b_Valid = st_Value.b_Valid;
    pst_View->b_DigitalState = st_Value.b_DigitalState;
    pst_View->b_Writable = pst_Config->b_Writable;
    pst_View->b_Enabled = pst_Config->b_Enabled;
    pst_View->b_ShowPlusSign = b_ProcessMapped
        ? (pst_Config->st_ProcessOperatingRange.f32_Min < 0.0F)
        : (pst_Config->st_ElectricalOperatingRange.f32_Min < 0.0F);
    pst_View->ui8_DecimalPlaces = pst_Config->ui8_DecimalPlaces;
    pst_View->f32_HmiStep = pst_Config->f32_HmiStep;
    (void)tx_mutex_put(&s_data_mutex);
    return true;
}

E_IO_Status fn_HMI_RequestDigital(uint16_t ui16_SignalId, bool b_State)
{
    S_AppCommand st_Command;
    const E_IO_Status e_Status = fn_IOService_DigitalWriteAllowed(ui16_SignalId);

    if (!s_created)
    {
        return E_IO_STATUS_CONFIG_ERROR;
    }
    if (e_Status != E_IO_STATUS_OK)
    {
        return e_Status;
    }

    st_Command.ui32_Type = (uint32_t)E_APP_CMD_DIGITAL;
    st_Command.ui32_SignalId = (uint32_t)ui16_SignalId;
    st_Command.ui32_ValueBits = b_State ? 1U : 0U;
    st_Command.ui32_Reserved = 0U;
    if (tx_queue_send(&s_command_queue, &st_Command, TX_NO_WAIT) != TX_SUCCESS)
    {
        ++s_ui32_CommandQueueDrops;
        return E_IO_STATUS_CONFIG_ERROR;
    }
    return E_IO_STATUS_OK;
}

E_IO_Status fn_HMI_RequestAnalog(uint16_t ui16_SignalId, float f32_ProcessValue)
{
    S_AppCommand st_Command;
    int16_t i16_DummyRegister;
    E_IO_Status e_Status;

    if (!s_created)
    {
        return E_IO_STATUS_CONFIG_ERROR;
    }

    e_Status = fn_IOService_ProcessToRegister(ui16_SignalId,
                                              f32_ProcessValue,
                                              &i16_DummyRegister,
                                              NULL);
    if (e_Status != E_IO_STATUS_OK)
    {
        return e_Status;
    }

    st_Command.ui32_Type = (uint32_t)E_APP_CMD_ANALOG;
    st_Command.ui32_SignalId = (uint32_t)ui16_SignalId;
    (void)memcpy(&st_Command.ui32_ValueBits, &f32_ProcessValue, sizeof(f32_ProcessValue));
    st_Command.ui32_Reserved = 0U;
    if (tx_queue_send(&s_command_queue, &st_Command, TX_NO_WAIT) != TX_SUCCESS)
    {
        ++s_ui32_CommandQueueDrops;
        return E_IO_STATUS_CONFIG_ERROR;
    }
    return E_IO_STATUS_OK;
}

bool fn_HMI_GetDiagnosticSnapshot(S_AppDiagnosticSnapshot *pst_Output)
{
    if ((pst_Output == NULL) || !s_created)
    {
        return false;
    }
    if (tx_mutex_get(&s_data_mutex, TX_NO_WAIT) != TX_SUCCESS)
    {
        return false;
    }
    *pst_Output = s_snapshot;
    (void)tx_mutex_put(&s_data_mutex);
    return true;
}

UINT fn_AppRtos_Create(void)
{
    UINT ui_Result;

    if (s_created)
    {
        return TX_SUCCESS;
    }

    fn_Diagnostics_Init(&s_snapshot);
    s_snapshot.e_IoConfigStatus = fn_IOService_Init();

    ui_Result = tx_mutex_create(&s_data_mutex, "application data", TX_INHERIT);
    if (ui_Result != TX_SUCCESS)
    {
        return ui_Result;
    }

    ui_Result = tx_queue_create(&s_command_queue, "output commands", APP_COMMAND_WORDS,
                                s_queue_storage, sizeof(s_queue_storage));
    if (ui_Result != TX_SUCCESS)
    {
        (void)tx_mutex_delete(&s_data_mutex);
        return ui_Result;
    }

    s_created = true;

    ui_Result = tx_thread_create(&s_transport_thread, "W5500 Modbus transport",
                                 fn_AppRtos_TransportThread, 0U,
                                 s_transport_stack, sizeof(s_transport_stack),
                                 APP_CFG_TRANSPORT_THREAD_PRIORITY,
                                 APP_CFG_TRANSPORT_THREAD_PRIORITY,
                                 TX_NO_TIME_SLICE, TX_AUTO_START);
    if (ui_Result != TX_SUCCESS)
    {
        s_created = false;
        (void)tx_queue_delete(&s_command_queue);
        (void)tx_mutex_delete(&s_data_mutex);
        return ui_Result;
    }

#if APP_CFG_ENABLE_DIAGNOSTICS
    ui_Result = tx_thread_create(&s_diagnostics_thread, "application diagnostics",
                                 fn_AppRtos_DiagnosticsThread, 0U,
                                 s_diagnostics_stack, sizeof(s_diagnostics_stack),
                                 APP_CFG_DIAG_THREAD_PRIORITY,
                                 APP_CFG_DIAG_THREAD_PRIORITY,
                                 TX_NO_TIME_SLICE, TX_AUTO_START);
    if (ui_Result != TX_SUCCESS)
    {
        return ui_Result;
    }
#endif

#if APP_CFG_ENABLE_SUPERVISOR
    ui_Result = tx_thread_create(&s_supervisor_thread, "application supervisor",
                                 fn_AppRtos_SupervisorThread, 0U,
                                 s_supervisor_stack, sizeof(s_supervisor_stack),
                                 APP_CFG_SUPERVISOR_THREAD_PRIORITY,
                                 APP_CFG_SUPERVISOR_THREAD_PRIORITY,
                                 TX_NO_TIME_SLICE, TX_AUTO_START);
#endif

    return ui_Result;
}
