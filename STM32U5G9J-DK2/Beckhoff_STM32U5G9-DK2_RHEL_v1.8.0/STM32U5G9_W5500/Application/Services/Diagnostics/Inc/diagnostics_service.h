#ifndef DIAGNOSTICS_SERVICE_H
#define DIAGNOSTICS_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "io_config_types.h"

/**
 * @file    diagnostics_service.h
 * @brief   Shared communication and runtime diagnostic data model.
 * @author  Prathamesh Chipkar
 * @date    11-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#ifdef __cplusplus
extern "C" {
#endif

#define DIAG_AI_CHANNELS  (4U)
#define DIAG_AO_CHANNELS  (4U)

typedef enum
{
    E_TCP_DISCONNECT_NONE = 0,
    E_TCP_DISCONNECT_REMOTE_SOCKET_CLOSE,
    E_TCP_DISCONNECT_PHY_LINK_DOWN,
    E_TCP_DISCONNECT_PHY_READ_ERROR,
    E_TCP_DISCONNECT_SOCKET_STATUS_IO_ERROR,
    E_TCP_DISCONNECT_CONNECT_FAILURE_LIMIT,
    E_TCP_DISCONNECT_TRANSPORT_FAILURE_LIMIT,
    E_TCP_DISCONNECT_PROTOCOL_FAILURE_LIMIT,
    E_TCP_DISCONNECT_W5500_REINIT,
    E_TCP_DISCONNECT_INTERNAL_ERROR
} E_TCP_DisconnectReason;

typedef struct
{
    uint16_t ui16_DiMask;
    uint16_t ui16_DoMask;
    int16_t  ai16_AiRaw[DIAG_AI_CHANNELS];
    int16_t  ai16_AiEngineeringRegister[DIAG_AI_CHANNELS];
    uint16_t aui16_AiStatus[DIAG_AI_CHANNELS];
    int16_t  ai16_AoRegister[DIAG_AO_CHANNELS];
    uint16_t aui16_AoStatus[DIAG_AO_CHANNELS];

    bool     b_W5500Ready;
    bool     b_EthernetLinkUp;
    bool     b_TcpConnected;
    uint8_t  ui8_LastModbusException;
    E_IO_ConfigStatus e_IoConfigStatus;

    uint32_t ui32_CommunicationUptimeSec;
    uint32_t ui32_SuccessfulTransactions;
    uint32_t ui32_ProtocolErrors;
    uint32_t ui32_TransportErrors;
    uint32_t ui32_W5500InitFailures;
    uint32_t ui32_ReconnectCount;
    uint32_t ui32_ConnectAttempts;
    uint32_t ui32_ConnectFailures;
    uint32_t ui32_ConsecutiveConnectFailures;
    uint32_t ui32_ModbusExceptions;
    uint32_t ui32_CommandQueueDrops;
    uint32_t ui32_PhyReadErrors;
    uint32_t ui32_PhysicalLinkDownEvents;
    uint32_t ui32_SocketDisconnectEvents;

    uint8_t  ui8_LastPhyCfgr;
    uint8_t  ui8_LastSocketStatus;
    uint8_t  ui8_LastSocketInterrupt;

    /* Alpha.6 persistent-session diagnostics. */
    E_TCP_DisconnectReason e_LastDisconnectReason;
    uint32_t ui32_LastDisconnectTimestampMs;
    uint32_t ui32_CurrentTcpSessionDurationMs;
    uint32_t ui32_LastTcpSessionDurationMs;
    uint32_t ui32_IntentionalSocketCloseCount;
    uint32_t ui32_ConsecutiveTransportFailures;
    uint32_t ui32_ConsecutiveProtocolFailures;

    uint8_t  ui8_LastFailedUnitId;
    uint8_t  ui8_LastFailedFunctionCode;
    uint16_t ui16_LastFailedAddress;
    uint16_t ui16_LastFailedQuantity;
    uint16_t ui16_LastTransactionId;
    int32_t  i32_LastW5500Result;

    uint32_t ui32_W5500StableReadRetries;
    uint32_t ui32_W5500StableReadFailures;

    /* Alpha.6.2 health-check disagreement diagnostics. */
    uint32_t ui32_PhySocketDisagreementEvents;
    uint32_t ui32_SocketStatusReadSuppressedCloses;
    uint32_t ui32_HealthySocketReconnectSuppressions;
    uint32_t ui32_SocketStateDisagreementEvents;

    uint32_t ui32_TransportCycleCounter;
    uint32_t ui32_DiagnosticsCycleCounter;
    uint32_t ui32_SupervisorCycleCounter;
    uint32_t ui32_LastCycleDurationMs;
    uint32_t ui32_SnapshotTimestampMs;
} S_AppDiagnosticSnapshot;

/** @brief Zero-initialize a diagnostic snapshot and mark configuration as not yet validated. */
void fn_Diagnostics_Init(S_AppDiagnosticSnapshot *pst_Snapshot);

#ifdef __cplusplus
}
#endif

#endif /* DIAGNOSTICS_SERVICE_H */
