#ifndef HMI_SERVICE_H
#define HMI_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "io_config_types.h"
#include "diagnostics_service.h"

/**
 * @file    hmi_service.h
 * @brief   Unit-aware TouchGFX API independent of Modbus, W5500 and STM32 HAL.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief HMI metadata for one Beckhoff logical unit. */
typedef struct
{
    uint8_t     ui8_UnitId;
    const char *pc_Name;
    const char *pc_Description;
    uint8_t     ui8_DiCount;
    uint8_t     ui8_DoCount;
    uint8_t     ui8_AiCount;
    uint8_t     ui8_AoCount;
    bool        b_Enabled;
} S_HMI_UnitInfo;

/** @brief HMI-ready representation of one named process signal. */
typedef struct
{
    uint16_t       ui16_SignalId;
    uint8_t        ui8_UnitId;
    const char    *pc_Name;
    const char    *pc_Unit;
    E_IO_SignalType e_Type;
    E_IO_Quality   e_Quality;
    float          f32_ProcessValue;
    float          f32_ElectricalValue;
    int32_t        i32_RawValue;
    bool           b_Valid;
    bool           b_DigitalState;
    bool           b_Writable;
    bool           b_Enabled;
    bool           b_ShowPlusSign;
    uint8_t        ui8_DecimalPlaces;
    float          f32_HmiStep;
} S_HMI_SignalView;

/** @brief Minimal communication status intentionally retained on the operator screen. */
typedef struct
{
    bool              b_TcpConnected;
    bool              b_EthernetLinkUp;
    uint32_t          ui32_CommunicationUptimeSec;
    E_IO_ConfigStatus e_ConfigStatus;
} S_HMI_SystemStatus;

/** @brief Read high-level TCP/configuration state for the operator header. */
bool fn_HMI_GetSystemStatus(S_HMI_SystemStatus *pst_Status);
/** @brief Read one configured Beckhoff-unit descriptor. */
bool fn_HMI_GetUnitInfo(uint8_t ui8_UnitId, S_HMI_UnitInfo *pst_Info);
/** @brief Read one unit/type-specific configured signal slot. */
bool fn_HMI_GetUnitSlot(uint8_t ui8_UnitId,
                        E_IO_SignalType e_Type,
                        uint8_t ui8_Slot,
                        S_HMI_SignalView *pst_View);
/** @brief Queue one named digital-output request. */
E_IO_Status fn_HMI_RequestDigital(uint16_t ui16_SignalId, bool b_State);
/** @brief Validate and queue one process-domain analog-output request. */
E_IO_Status fn_HMI_RequestAnalog(uint16_t ui16_SignalId, float f32_ProcessValue);
/** @brief Read the complete engineering diagnostic snapshot used in CubeIDE. */
bool fn_HMI_GetDiagnosticSnapshot(S_AppDiagnosticSnapshot *pst_Snapshot);

#ifdef __cplusplus
}
#endif

#endif /* HMI_SERVICE_H */
