/**
 * @file route_manager.h
 * @brief Owns two native-C EtherNet/IP Class 1 connections and process images.
 * @author Prathamesh Chipkar
 */
#ifndef GW_ROUTE_MANAGER_H
#define GW_ROUTE_MANAGER_H

#include "gateway/gateway_config.h"

#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque route manager. */
typedef struct S_GW_ROUTE_MANAGER S_GW_ROUTE_MANAGER;

/** @brief Public route health and channel counts. */
typedef struct
{
    char ac_Name[GW_NAME_LENGTH];
    uint8_t ui8_UnitId;
    size_t sz_DigitalInputCount;
    size_t sz_DigitalOutputCount;
    size_t sz_AnalogInputCount;
    size_t sz_AnalogOutputCount;
    E_GW_ROUTE_STATE e_State;
    bool b_IoOpen;
    bool b_InputValid;
    uint64_t ui64_InputSequence;
    uint64_t ui64_InputAgeMs;
    uint64_t ui64_OutputRevision;
    uint32_t ui32_CommunicationErrors;
    uint32_t ui32_ReconnectCount;
    uint32_t ui32_WriteErrors;
    uint32_t ui32_LastWriterIpv4;
    uint64_t ui64_LastWriteMs;
} S_GW_ROUTE_INFO;

/** @brief Create and validate the coupler route without starting its network thread. */
E_GW_RESULT fn_GW_RouteManagerCreate(const S_GW_CONFIG *pst_Config,
                                      S_GW_ROUTE_MANAGER **ppst_Manager,
                                      char *pc_Error,
                                      size_t sz_ErrorSize);

/** @brief Start one EtherNet/IP worker thread per enabled coupler. */
E_GW_RESULT fn_GW_RouteManagerStart(S_GW_ROUTE_MANAGER *pst_Manager);

/** @brief Stop and join all EtherNet/IP workers. */
void fn_GW_RouteManagerStop(S_GW_ROUTE_MANAGER *pst_Manager);

/** @brief Destroy a stopped manager. */
void fn_GW_RouteManagerDestroy(S_GW_ROUTE_MANAGER **ppst_Manager);

/** @brief Begin an image transaction; pair success with TransactionEnd on the same thread.
 * Do not perform blocking network operations while holding this lock. */
E_GW_RESULT fn_GW_RouteTransactionBegin(S_GW_ROUTE_MANAGER *manager, uint8_t unit);
/** @brief Release the image transaction lock. */
void fn_GW_RouteTransactionEnd(S_GW_ROUTE_MANAGER *manager, uint8_t unit);

/** @brief Read route information selected by Modbus Unit ID. */
E_GW_RESULT fn_GW_RouteGetInfo(S_GW_ROUTE_MANAGER *pst_Manager,
                               uint8_t ui8_UnitId,
                               S_GW_ROUTE_INFO *pst_Info);

/** @brief Read one logical EL1409 input channel, zero-based. */
E_GW_RESULT fn_GW_RouteReadDigitalInput(S_GW_ROUTE_MANAGER *pst_Manager,
                                        uint8_t ui8_UnitId,
                                        uint16_t ui16_Channel,
                                        bool *pb_Value);

/** @brief Read one logical EL2409 output channel, zero-based. */
E_GW_RESULT fn_GW_RouteReadDigitalOutput(S_GW_ROUTE_MANAGER *pst_Manager,
                                         uint8_t ui8_UnitId,
                                         uint16_t ui16_Channel,
                                         bool *pb_Value);

/** @brief Write one logical EL2409 output channel, zero-based. */
E_GW_RESULT fn_GW_RouteWriteDigitalOutput(S_GW_ROUTE_MANAGER *pst_Manager,
                                          uint8_t ui8_UnitId,
                                          uint16_t ui16_Channel,
                                          bool b_Value,
                                          uint32_t ui32_WriterIpv4);

/** @brief Atomically write multiple EL2409 outputs from packed Modbus bits. */
E_GW_RESULT fn_GW_RouteWriteDigitalOutputs(S_GW_ROUTE_MANAGER *pst_Manager,
                                           uint8_t ui8_UnitId,
                                           uint16_t ui16_Start,
                                           uint16_t ui16_Quantity,
                                           const uint8_t *pui8_Packed,
                                           uint32_t ui32_WriterIpv4);

/** @brief Read one logical EL3074 analog input channel, zero-based. */
E_GW_RESULT fn_GW_RouteReadAnalogInput(S_GW_ROUTE_MANAGER *pst_Manager,
                                       uint8_t ui8_UnitId,
                                       uint16_t ui16_Channel,
                                       ead_analog_sample_t *pst_Sample);

/** @brief Read one logical EL4074 output command and status, zero-based. */
E_GW_RESULT fn_GW_RouteReadAnalogOutput(S_GW_ROUTE_MANAGER *pst_Manager,
                                        uint8_t ui8_UnitId,
                                        uint16_t ui16_Channel,
                                        ead_analog_sample_t *pst_Sample);


/** @brief Read the configured analog-input profile for one logical EL3074 channel, zero-based. */
E_GW_RESULT fn_GW_RouteGetAnalogInputMode(S_GW_ROUTE_MANAGER *pst_Manager,
                                          uint8_t ui8_UnitId,
                                          uint16_t ui16_Channel,
                                          ead_analog_mode_t *pe_Mode);

/** @brief Read the configured analog-output profile for one logical EL4074 channel, zero-based. */
E_GW_RESULT fn_GW_RouteGetAnalogOutputMode(S_GW_ROUTE_MANAGER *pst_Manager,
                                           uint8_t ui8_UnitId,
                                           uint16_t ui16_Channel,
                                           ead_analog_mode_t *pe_Mode);

/** @brief Write one logical EL4074 output in configured SI engineering units, zero-based. */
E_GW_RESULT fn_GW_RouteWriteAnalogOutput(S_GW_ROUTE_MANAGER *pst_Manager,
                                         uint8_t ui8_UnitId,
                                         uint16_t ui16_Channel,
                                         double d_Engineering,
                                         uint32_t ui32_WriterIpv4);

/** @brief Atomically write multiple EL4074 outputs in configured SI engineering units. */
E_GW_RESULT fn_GW_RouteWriteAnalogOutputs(S_GW_ROUTE_MANAGER *pst_Manager,
                                          uint8_t ui8_UnitId,
                                          uint16_t ui16_Start,
                                          uint16_t ui16_Quantity,
                                          const double *pd_Engineering,
                                          uint32_t ui32_WriterIpv4);

/** @brief Record an accepted or rejected Modbus TCP connection. */
void fn_GW_RecordConnection(S_GW_ROUTE_MANAGER *pst_Manager, bool b_Accepted);
/** @brief Set the current Modbus TCP client count. */
void fn_GW_SetActiveClients(S_GW_ROUTE_MANAGER *pst_Manager, uint32_t ui32_Count);
/** @brief Record a processed Modbus request. */
void fn_GW_RecordRequest(S_GW_ROUTE_MANAGER *pst_Manager, bool b_Exception);
/** @brief Read server-wide diagnostics. */
void fn_GW_GetServerDiagnostics(S_GW_ROUTE_MANAGER *pst_Manager,
                                uint32_t *pui32_ActiveClients,
                                uint64_t *pui64_Connections,
                                uint64_t *pui64_RejectedConnections,
                                uint64_t *pui64_Requests,
                                uint64_t *pui64_Exceptions);

#ifdef __cplusplus
}
#endif

#endif
