/**
 * @file    io_service.c
 * @brief   Configuration-driven process I/O conversion and validation service.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#include "io_service.h"
#include "io_config.h"
#include "app_config.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static S_IO_SignalValue gast_IO_State[APP_CFG_MAX_SIGNAL_COUNT];
static size_t g_sz_IOStateCount;
static E_IO_ConfigStatus g_e_ConfigStatus = E_IO_CONFIG_NULL;

static float fn_IOService_ClampFloat(float f32_Value, float f32_Min, float f32_Max)
{
    if (f32_Value < f32_Min)
    {
        return f32_Min;
    }
    if (f32_Value > f32_Max)
    {
        return f32_Max;
    }
    return f32_Value;
}

static bool fn_IOService_InRange(float f32_Value, const S_IO_Range *pst_Range)
{
    return (pst_Range != NULL) && (f32_Value >= pst_Range->f32_Min) && (f32_Value <= pst_Range->f32_Max);
}

static size_t fn_IOService_FindStateIndex(uint16_t ui16_SignalId)
{
    size_t sz_Index;
    for (sz_Index = 0U; sz_Index < g_sz_IOStateCount; ++sz_Index)
    {
        if (gast_IO_State[sz_Index].ui16_SignalId == ui16_SignalId)
        {
            return sz_Index;
        }
    }
    return APP_CFG_MAX_SIGNAL_COUNT;
}

float fn_IOService_ElectricalToProcess(const S_IO_SignalConfig *pst_Config, float f32_Electrical)
{
    uint8_t ui8_Index;

    if (pst_Config == NULL)
    {
        return 0.0F;
    }

    if (pst_Config->e_MappingType == E_IO_MAPPING_LINEAR)
    {
        const float f32_X0 = pst_Config->st_ElectricalMapRange.f32_Min;
        const float f32_X1 = pst_Config->st_ElectricalMapRange.f32_Max;
        const float f32_Y0 = pst_Config->st_ProcessMapRange.f32_Min;
        const float f32_Y1 = pst_Config->st_ProcessMapRange.f32_Max;
        return f32_Y0 + ((f32_Electrical - f32_X0) * (f32_Y1 - f32_Y0) / (f32_X1 - f32_X0));
    }

    if ((pst_Config->e_MappingType == E_IO_MAPPING_PIECEWISE_LINEAR) &&
        (pst_Config->past_Calibration != NULL) && (pst_Config->ui8_CalibrationPointCount >= 2U))
    {
        const S_IO_CalibrationPoint *past_Points = pst_Config->past_Calibration;
        if (f32_Electrical <= past_Points[0].f32_Electrical)
        {
            return past_Points[0].f32_Process;
        }
        for (ui8_Index = 1U; ui8_Index < pst_Config->ui8_CalibrationPointCount; ++ui8_Index)
        {
            if (f32_Electrical <= past_Points[ui8_Index].f32_Electrical)
            {
                const float f32_X0 = past_Points[ui8_Index - 1U].f32_Electrical;
                const float f32_X1 = past_Points[ui8_Index].f32_Electrical;
                const float f32_Y0 = past_Points[ui8_Index - 1U].f32_Process;
                const float f32_Y1 = past_Points[ui8_Index].f32_Process;
                return f32_Y0 + ((f32_Electrical - f32_X0) * (f32_Y1 - f32_Y0) / (f32_X1 - f32_X0));
            }
        }
        return past_Points[pst_Config->ui8_CalibrationPointCount - 1U].f32_Process;
    }

    return f32_Electrical;
}

float fn_IOService_ProcessToElectrical(const S_IO_SignalConfig *pst_Config, float f32_Process)
{
    uint8_t ui8_Index;

    if (pst_Config == NULL)
    {
        return 0.0F;
    }

    if (pst_Config->e_MappingType == E_IO_MAPPING_LINEAR)
    {
        const float f32_X0 = pst_Config->st_ProcessMapRange.f32_Min;
        const float f32_X1 = pst_Config->st_ProcessMapRange.f32_Max;
        const float f32_Y0 = pst_Config->st_ElectricalMapRange.f32_Min;
        const float f32_Y1 = pst_Config->st_ElectricalMapRange.f32_Max;
        if (f32_X1 == f32_X0)
        {
            return f32_Y0;
        }
        return f32_Y0 + ((f32_Process - f32_X0) * (f32_Y1 - f32_Y0) / (f32_X1 - f32_X0));
    }

    if ((pst_Config->e_MappingType == E_IO_MAPPING_PIECEWISE_LINEAR) &&
        (pst_Config->past_Calibration != NULL) && (pst_Config->ui8_CalibrationPointCount >= 2U))
    {
        const S_IO_CalibrationPoint *past_Points = pst_Config->past_Calibration;
        for (ui8_Index = 1U; ui8_Index < pst_Config->ui8_CalibrationPointCount; ++ui8_Index)
        {
            const float f32_P0 = past_Points[ui8_Index - 1U].f32_Process;
            const float f32_P1 = past_Points[ui8_Index].f32_Process;
            const bool b_InSegment = ((f32_Process >= f32_P0) && (f32_Process <= f32_P1)) ||
                                     ((f32_Process <= f32_P0) && (f32_Process >= f32_P1));
            if (b_InSegment && (f32_P1 != f32_P0))
            {
                const float f32_E0 = past_Points[ui8_Index - 1U].f32_Electrical;
                const float f32_E1 = past_Points[ui8_Index].f32_Electrical;
                return f32_E0 + ((f32_Process - f32_P0) * (f32_E1 - f32_E0) / (f32_P1 - f32_P0));
            }
        }
    }

    return f32_Process;
}

E_IO_ConfigStatus fn_IOService_Init(void)
{
    size_t sz_Index;
    size_t sz_Count = 0U;
    const S_IO_SignalConfig *past_Config = fn_IOConfig_GetTable(&sz_Count);

    g_e_ConfigStatus = fn_IOConfig_ValidateAll();
    g_sz_IOStateCount = 0U;
    (void)memset(gast_IO_State, 0, sizeof(gast_IO_State));

    if ((g_e_ConfigStatus != E_IO_CONFIG_OK) || (past_Config == NULL) || (sz_Count > APP_CFG_MAX_SIGNAL_COUNT))
    {
        return g_e_ConfigStatus;
    }

    g_sz_IOStateCount = sz_Count;
    for (sz_Index = 0U; sz_Index < sz_Count; ++sz_Index)
    {
        gast_IO_State[sz_Index].ui16_SignalId = past_Config[sz_Index].ui16_SignalId;
        gast_IO_State[sz_Index].e_Quality = past_Config[sz_Index].b_Enabled
                                         ? E_IO_QUALITY_STALE
                                         : E_IO_QUALITY_DISABLED;
    }
    return E_IO_CONFIG_OK;
}

void fn_IOService_SetCommunicationQuality(E_IO_Quality e_Quality)
{
    size_t sz_Index;
    const S_IO_SignalConfig *past_Config;
    size_t sz_Count;

    past_Config = fn_IOConfig_GetTable(&sz_Count);
    if ((past_Config == NULL) || (sz_Count != g_sz_IOStateCount))
    {
        return;
    }

    for (sz_Index = 0U; sz_Index < sz_Count; ++sz_Index)
    {
        if (past_Config[sz_Index].b_Enabled)
        {
            gast_IO_State[sz_Index].e_Quality = e_Quality;
            if (e_Quality != E_IO_QUALITY_GOOD)
            {
                gast_IO_State[sz_Index].b_Valid = false;
            }
        }
    }
}

void fn_IOService_UpdateDigitalMask(uint8_t ui8_UnitId, E_IO_SignalType e_Type,
                                    uint16_t ui16_Mask, uint32_t ui32_TimestampMs)
{
    size_t sz_Index;
    size_t sz_Count;
    const S_IO_SignalConfig *past_Config = fn_IOConfig_GetTable(&sz_Count);

    if ((past_Config == NULL) || ((e_Type != E_IO_SIGNAL_DI) && (e_Type != E_IO_SIGNAL_DO)))
    {
        return;
    }

    for (sz_Index = 0U; sz_Index < sz_Count; ++sz_Index)
    {
        const S_IO_SignalConfig *pst_Config = &past_Config[sz_Index];
        if (pst_Config->b_Enabled && (pst_Config->ui8_UnitId == ui8_UnitId) &&
            (pst_Config->e_SignalType == e_Type) && (pst_Config->ui8_BitIndex < 16U))
        {
            gast_IO_State[sz_Index].b_DigitalState = ((ui16_Mask >> pst_Config->ui8_BitIndex) & 1U) != 0U;
            gast_IO_State[sz_Index].i32_RawValue = gast_IO_State[sz_Index].b_DigitalState ? 1 : 0;
            gast_IO_State[sz_Index].f32_ElectricalValue = (float)gast_IO_State[sz_Index].i32_RawValue;
            gast_IO_State[sz_Index].f32_ProcessValue = gast_IO_State[sz_Index].f32_ElectricalValue;
            gast_IO_State[sz_Index].e_Quality = E_IO_QUALITY_GOOD;
            gast_IO_State[sz_Index].b_Valid = true;
            gast_IO_State[sz_Index].ui32_TimestampMs = ui32_TimestampMs;
        }
    }
}

void fn_IOService_UpdateAnalogRegisters(uint8_t ui8_UnitId, E_IO_SignalType e_Type,
                                        uint16_t ui16_StartAddress, const int16_t *pi16_Values,
                                        uint16_t ui16_Quantity, uint32_t ui32_TimestampMs)
{
    size_t sz_Index;
    size_t sz_Count;
    const S_IO_SignalConfig *past_Config = fn_IOConfig_GetTable(&sz_Count);

    if ((past_Config == NULL) || (pi16_Values == NULL) ||
        ((e_Type != E_IO_SIGNAL_AI) && (e_Type != E_IO_SIGNAL_AO)))
    {
        return;
    }

    for (sz_Index = 0U; sz_Index < sz_Count; ++sz_Index)
    {
        const S_IO_SignalConfig *pst_Config = &past_Config[sz_Index];
        if (pst_Config->b_Enabled && (pst_Config->ui8_UnitId == ui8_UnitId) &&
            (pst_Config->e_SignalType == e_Type) &&
            (pst_Config->ui16_RegisterAddress >= ui16_StartAddress) &&
            (pst_Config->ui16_RegisterAddress < (uint16_t)(ui16_StartAddress + ui16_Quantity)))
        {
            const uint16_t ui16_Offset = (uint16_t)(pst_Config->ui16_RegisterAddress - ui16_StartAddress);
            const int16_t i16_Raw = pi16_Values[ui16_Offset];
            const float f32_Electrical = ((float)i16_Raw * pst_Config->f32_RegisterToElectricalScale) +
                                         pst_Config->f32_RegisterToElectricalOffset;
            const float f32_Process = fn_IOService_ElectricalToProcess(pst_Config, f32_Electrical);
            E_IO_Quality e_Quality = E_IO_QUALITY_GOOD;

            if (!fn_IOService_InRange(f32_Electrical, &pst_Config->st_ElectricalHardwareRange) ||
                !fn_IOService_InRange(f32_Electrical, &pst_Config->st_ElectricalOperatingRange) ||
                !fn_IOService_InRange(f32_Process, &pst_Config->st_ProcessOperatingRange))
            {
                e_Quality = E_IO_QUALITY_OUT_OF_RANGE;
            }

            gast_IO_State[sz_Index].i32_RawValue = (int32_t)i16_Raw;
            gast_IO_State[sz_Index].f32_ElectricalValue = f32_Electrical;
            gast_IO_State[sz_Index].f32_ProcessValue = f32_Process;
            gast_IO_State[sz_Index].e_Quality = e_Quality;
            gast_IO_State[sz_Index].b_Valid = true;
            gast_IO_State[sz_Index].ui32_TimestampMs = ui32_TimestampMs;
        }
    }
}

bool fn_IOService_GetValue(uint16_t ui16_SignalId, S_IO_SignalValue *pst_Value)
{
    const size_t sz_Index = fn_IOService_FindStateIndex(ui16_SignalId);
    if ((pst_Value == NULL) || (sz_Index >= g_sz_IOStateCount))
    {
        return false;
    }
    *pst_Value = gast_IO_State[sz_Index];
    return true;
}

E_IO_Status fn_IOService_DigitalWriteAllowed(uint16_t ui16_SignalId)
{
    const S_IO_SignalConfig *pst_Config = fn_IOConfig_FindById(ui16_SignalId);
    if (pst_Config == NULL) return E_IO_STATUS_NOT_FOUND;
    if (!pst_Config->b_Enabled) return E_IO_STATUS_DISABLED;
    if ((pst_Config->e_SignalType != E_IO_SIGNAL_DO) || !pst_Config->b_Writable)
        return E_IO_STATUS_NOT_WRITABLE;
    return E_IO_STATUS_OK;
}

E_IO_Status fn_IOService_ProcessToRegister(uint16_t ui16_SignalId, float f32_ProcessValue,
                                           int16_t *pi16_RegisterValue,
                                           float *pf32_AppliedProcessValue)
{
    const S_IO_SignalConfig *pst_Config = fn_IOConfig_FindById(ui16_SignalId);
    float f32_Applied;
    float f32_Electrical;
    float f32_Raw;
    int32_t i32_Raw;

    if ((pi16_RegisterValue == NULL) || (f32_ProcessValue != f32_ProcessValue))
        return E_IO_STATUS_INVALID_ARGUMENT;
    if (pst_Config == NULL) return E_IO_STATUS_NOT_FOUND;
    if (!pst_Config->b_Enabled) return E_IO_STATUS_DISABLED;
    if ((pst_Config->e_SignalType != E_IO_SIGNAL_AO) || !pst_Config->b_Writable)
        return E_IO_STATUS_NOT_WRITABLE;

    f32_Applied = f32_ProcessValue;
    if (!fn_IOService_InRange(f32_Applied, &pst_Config->st_ProcessOperatingRange))
    {
        if (pst_Config->e_WriteRangePolicy == E_IO_WRITE_REJECT)
        {
            return E_IO_STATUS_OUT_OF_RANGE;
        }
        f32_Applied = fn_IOService_ClampFloat(f32_Applied,
                                              pst_Config->st_ProcessOperatingRange.f32_Min,
                                              pst_Config->st_ProcessOperatingRange.f32_Max);
    }

    f32_Electrical = fn_IOService_ProcessToElectrical(pst_Config, f32_Applied);
    if (!fn_IOService_InRange(f32_Electrical, &pst_Config->st_ElectricalOperatingRange) ||
        !fn_IOService_InRange(f32_Electrical, &pst_Config->st_ElectricalHardwareRange))
    {
        return E_IO_STATUS_OUT_OF_RANGE;
    }

    f32_Raw = (f32_Electrical - pst_Config->f32_RegisterToElectricalOffset) /
              pst_Config->f32_RegisterToElectricalScale;
    if ((f32_Raw > (float)INT16_MAX) || (f32_Raw < (float)INT16_MIN))
    {
        return E_IO_STATUS_OUT_OF_RANGE;
    }

    i32_Raw = (int32_t)(f32_Raw + ((f32_Raw >= 0.0F) ? 0.5F : -0.5F));
    *pi16_RegisterValue = (int16_t)i32_Raw;
    if (pf32_AppliedProcessValue != NULL)
    {
        *pf32_AppliedProcessValue = f32_Applied;
    }
    return E_IO_STATUS_OK;
}
