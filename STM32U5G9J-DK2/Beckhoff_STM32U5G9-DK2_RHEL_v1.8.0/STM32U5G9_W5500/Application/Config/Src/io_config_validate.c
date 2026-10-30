/**
 * @file    io_config_validate.c
 * @brief   Startup validation for Beckhoff topology and I/O configuration.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#include "io_config.h"
#include "app_config.h"

#include <stddef.h>

static bool fn_IOConfig_IsAnalog(E_IO_SignalType e_Type)
{
    return (e_Type == E_IO_SIGNAL_AI) || (e_Type == E_IO_SIGNAL_AO);
}

static bool fn_IOConfig_IsPredefinedAnalogMode(E_IO_AnalogMode e_Mode)
{
    return (e_Mode >= E_IO_ANALOG_CURRENT_BIPOLAR_20MA) &&
           (e_Mode <= E_IO_ANALOG_VOLTAGE_0_10V);
}

static bool fn_IOConfig_RangeValid(const S_IO_Range *pst_Range)
{
    return (pst_Range != NULL) &&
           (pst_Range->f32_Min == pst_Range->f32_Min) &&
           (pst_Range->f32_Max == pst_Range->f32_Max) &&
           (pst_Range->f32_Min < pst_Range->f32_Max);
}

static bool fn_IOConfig_RangeInside(const S_IO_Range *pst_Inner, const S_IO_Range *pst_Outer)
{
    return fn_IOConfig_RangeValid(pst_Inner) && fn_IOConfig_RangeValid(pst_Outer) &&
           (pst_Inner->f32_Min >= pst_Outer->f32_Min) &&
           (pst_Inner->f32_Max <= pst_Outer->f32_Max);
}

static bool fn_IOConfig_FloatClose(float f32_A, float f32_B)
{
    const float f32_Diff = (f32_A >= f32_B) ? (f32_A - f32_B) : (f32_B - f32_A);
    return f32_Diff < 0.0005F;
}

static E_IO_ConfigStatus fn_IOConfig_ValidateOne(const S_IO_SignalConfig *pst_Config)
{
    S_IO_Range st_ModeRange;
    uint8_t ui8_Index;

    if (pst_Config == NULL)
    {
        return E_IO_CONFIG_NULL;
    }
    if ((pst_Config->ui8_UnitId == 0U) || (pst_Config->ui8_UnitId > 247U) ||
        (fn_IOConfig_FindUnit(pst_Config->ui8_UnitId) == NULL))
    {
        return E_IO_CONFIG_INVALID_UNIT;
    }
    if ((pst_Config->pc_Name == NULL) || (pst_Config->pc_Name[0] == '\0'))
    {
        return E_IO_CONFIG_NULL;
    }
    if ((pst_Config->e_SignalType < E_IO_SIGNAL_DI) || (pst_Config->e_SignalType > E_IO_SIGNAL_AO))
    {
        return E_IO_CONFIG_INVALID_TYPE;
    }
    if (((pst_Config->e_SignalType == E_IO_SIGNAL_AI) ||
         (pst_Config->e_SignalType == E_IO_SIGNAL_DI)) && pst_Config->b_Writable)
    {
        return E_IO_CONFIG_INVALID_ACCESS;
    }

    if (((pst_Config->e_SignalType == E_IO_SIGNAL_DI) && (pst_Config->ui8_ReadFunctionCode != 0x02U)) ||
        ((pst_Config->e_SignalType == E_IO_SIGNAL_DO) && (pst_Config->ui8_ReadFunctionCode != 0x01U)) ||
        ((pst_Config->e_SignalType == E_IO_SIGNAL_AI) && (pst_Config->ui8_ReadFunctionCode != 0x04U)) ||
        ((pst_Config->e_SignalType == E_IO_SIGNAL_AO) && (pst_Config->ui8_ReadFunctionCode != 0x03U)))
    {
        return E_IO_CONFIG_INVALID_ADDRESS;
    }

    if (pst_Config->b_HmiVisible)
    {
        const uint8_t ui8_MaxSlots = fn_IOConfig_IsAnalog(pst_Config->e_SignalType)
                                  ? APP_CFG_ANALOG_HMI_SLOTS
                                  : APP_CFG_DIGITAL_HMI_SLOTS;
        if (pst_Config->ui8_HmiSlot >= ui8_MaxSlots)
        {
            return E_IO_CONFIG_INVALID_HMI_SLOT;
        }
    }

    if (!fn_IOConfig_IsAnalog(pst_Config->e_SignalType))
    {
        if ((pst_Config->e_DataType != E_IO_DATATYPE_BOOL) ||
            (pst_Config->ui8_BitIndex >= 16U))
        {
            return E_IO_CONFIG_INVALID_TYPE;
        }
        return E_IO_CONFIG_OK;
    }

    if ((pst_Config->e_AnalogMode <= E_IO_ANALOG_NOT_APPLICABLE) ||
        (pst_Config->e_AnalogMode > E_IO_ANALOG_VOLTAGE_CUSTOM))
    {
        return E_IO_CONFIG_INVALID_MODE;
    }
    /* Current RHEL data model uses one signed 16-bit register per AI/AO channel. */
    if (pst_Config->e_DataType != E_IO_DATATYPE_INT16)
    {
        return E_IO_CONFIG_INVALID_TYPE;
    }
    if ((pst_Config->f32_RegisterToElectricalScale != pst_Config->f32_RegisterToElectricalScale) ||
        (pst_Config->f32_RegisterToElectricalScale == 0.0F))
    {
        return E_IO_CONFIG_INVALID_SCALE;
    }
    if (!fn_IOConfig_RangeValid(&pst_Config->st_ElectricalHardwareRange) ||
        !fn_IOConfig_RangeInside(&pst_Config->st_ElectricalOperatingRange,
                                &pst_Config->st_ElectricalHardwareRange) ||
        !fn_IOConfig_RangeInside(&pst_Config->st_ElectricalMapRange,
                                &pst_Config->st_ElectricalHardwareRange))
    {
        return E_IO_CONFIG_INVALID_RANGE;
    }

    if (fn_IOConfig_IsPredefinedAnalogMode(pst_Config->e_AnalogMode))
    {
        st_ModeRange = fn_IOConfig_GetModeHardwareRange(pst_Config->e_AnalogMode);
        if (!fn_IOConfig_FloatClose(st_ModeRange.f32_Min, pst_Config->st_ElectricalHardwareRange.f32_Min) ||
            !fn_IOConfig_FloatClose(st_ModeRange.f32_Max, pst_Config->st_ElectricalHardwareRange.f32_Max))
        {
            return E_IO_CONFIG_INVALID_RANGE;
        }
    }

    if (pst_Config->e_MappingType == E_IO_MAPPING_LINEAR)
    {
        const float f32_MapLow = (pst_Config->st_ProcessMapRange.f32_Min <= pst_Config->st_ProcessMapRange.f32_Max)
                               ? pst_Config->st_ProcessMapRange.f32_Min
                               : pst_Config->st_ProcessMapRange.f32_Max;
        const float f32_MapHigh = (pst_Config->st_ProcessMapRange.f32_Min <= pst_Config->st_ProcessMapRange.f32_Max)
                                ? pst_Config->st_ProcessMapRange.f32_Max
                                : pst_Config->st_ProcessMapRange.f32_Min;
        if (!fn_IOConfig_RangeValid(&pst_Config->st_ProcessOperatingRange) ||
            (pst_Config->st_ProcessOperatingRange.f32_Min < f32_MapLow) ||
            (pst_Config->st_ProcessOperatingRange.f32_Max > f32_MapHigh) ||
            (pst_Config->pc_ProcessUnit == NULL) || (pst_Config->pc_ProcessUnit[0] == '\0'))
        {
            return E_IO_CONFIG_INVALID_MAPPING;
        }
    }
    else if (pst_Config->e_MappingType == E_IO_MAPPING_PIECEWISE_LINEAR)
    {
        if ((pst_Config->past_Calibration == NULL) ||
            (pst_Config->ui8_CalibrationPointCount < 2U) ||
            (pst_Config->ui8_CalibrationPointCount > APP_CFG_MAX_CALIBRATION_POINTS) ||
            (pst_Config->pc_ProcessUnit == NULL) || (pst_Config->pc_ProcessUnit[0] == '\0') ||
            !fn_IOConfig_RangeValid(&pst_Config->st_ProcessOperatingRange))
        {
            return E_IO_CONFIG_INVALID_MAPPING;
        }
        for (ui8_Index = 1U; ui8_Index < pst_Config->ui8_CalibrationPointCount; ++ui8_Index)
        {
            if (!(pst_Config->past_Calibration[ui8_Index - 1U].f32_Electrical <
                  pst_Config->past_Calibration[ui8_Index].f32_Electrical))
            {
                return E_IO_CONFIG_INVALID_MAPPING;
            }
        }
        if ((pst_Config->past_Calibration[0].f32_Electrical < pst_Config->st_ElectricalMapRange.f32_Min) ||
            (pst_Config->past_Calibration[pst_Config->ui8_CalibrationPointCount - 1U].f32_Electrical >
             pst_Config->st_ElectricalMapRange.f32_Max))
        {
            return E_IO_CONFIG_INVALID_MAPPING;
        }
    }
    else if (pst_Config->e_MappingType == E_IO_MAPPING_NONE)
    {
        if (!fn_IOConfig_RangeValid(&pst_Config->st_ProcessOperatingRange))
        {
            return E_IO_CONFIG_INVALID_MAPPING;
        }
    }
    else
    {
        return E_IO_CONFIG_INVALID_MAPPING;
    }

    if (pst_Config->ui8_DecimalPlaces > 3U)
    {
        return E_IO_CONFIG_INVALID_TYPE;
    }

    return E_IO_CONFIG_OK;
}

E_IO_ConfigStatus fn_IOConfig_ValidateAll(void)
{
    size_t sz_SignalCount = 0U;
    size_t sz_UnitCount = 0U;
    size_t sz_I;
    size_t sz_J;
    const S_IO_SignalConfig *past_Table = fn_IOConfig_GetTable(&sz_SignalCount);
    const S_IO_UnitConfig *past_Units = fn_IOConfig_GetUnitTable(&sz_UnitCount);
    uint16_t aaui16_UnitTypeCount[APP_CFG_BECKHOFF_UNIT_COUNT][4] = {{0U}};
    uint16_t aui16_TotalTypeCount[4] = {0U, 0U, 0U, 0U};

    if ((past_Table == NULL) || (past_Units == NULL) ||
        (sz_SignalCount == 0U) || (sz_UnitCount != APP_CFG_BECKHOFF_UNIT_COUNT))
    {
        return E_IO_CONFIG_NULL;
    }
    if (sz_SignalCount > APP_CFG_MAX_SIGNAL_COUNT)
    {
        return E_IO_CONFIG_CAPACITY_EXCEEDED;
    }

    for (sz_I = 0U; sz_I < sz_UnitCount; ++sz_I)
    {
        if ((past_Units[sz_I].ui8_UnitId == 0U) || (past_Units[sz_I].pc_Name == NULL) ||
            (past_Units[sz_I].pc_Description == NULL))
        {
            return E_IO_CONFIG_INVALID_UNIT;
        }
        for (sz_J = sz_I + 1U; sz_J < sz_UnitCount; ++sz_J)
        {
            if (past_Units[sz_I].ui8_UnitId == past_Units[sz_J].ui8_UnitId)
            {
                return E_IO_CONFIG_DUPLICATE_UNIT;
            }
        }
    }

    for (sz_I = 0U; sz_I < sz_SignalCount; ++sz_I)
    {
        E_IO_ConfigStatus e_Status = fn_IOConfig_ValidateOne(&past_Table[sz_I]);
        size_t sz_UnitIndex = sz_UnitCount;
        if (e_Status != E_IO_CONFIG_OK)
        {
            return e_Status;
        }

        ++aui16_TotalTypeCount[past_Table[sz_I].e_SignalType];
        for (sz_J = 0U; sz_J < sz_UnitCount; ++sz_J)
        {
            if (past_Units[sz_J].ui8_UnitId == past_Table[sz_I].ui8_UnitId)
            {
                sz_UnitIndex = sz_J;
                break;
            }
        }
        if (sz_UnitIndex >= sz_UnitCount)
        {
            return E_IO_CONFIG_INVALID_UNIT;
        }
        ++aaui16_UnitTypeCount[sz_UnitIndex][past_Table[sz_I].e_SignalType];

        for (sz_J = sz_I + 1U; sz_J < sz_SignalCount; ++sz_J)
        {
            if (past_Table[sz_I].ui16_SignalId == past_Table[sz_J].ui16_SignalId)
            {
                return E_IO_CONFIG_DUPLICATE_ID;
            }
            if ((past_Table[sz_I].ui8_UnitId == past_Table[sz_J].ui8_UnitId) &&
                (past_Table[sz_I].e_SignalType == past_Table[sz_J].e_SignalType) &&
                (past_Table[sz_I].ui8_ReadFunctionCode == past_Table[sz_J].ui8_ReadFunctionCode))
            {
                const bool b_Digital = (past_Table[sz_I].e_SignalType == E_IO_SIGNAL_DI) ||
                                       (past_Table[sz_I].e_SignalType == E_IO_SIGNAL_DO);
                const bool b_SamePoint = b_Digital
                    ? ((past_Table[sz_I].ui16_RegisterAddress == past_Table[sz_J].ui16_RegisterAddress) &&
                       (past_Table[sz_I].ui8_BitIndex == past_Table[sz_J].ui8_BitIndex))
                    : (past_Table[sz_I].ui16_RegisterAddress == past_Table[sz_J].ui16_RegisterAddress);
                if (b_SamePoint)
                {
                    return E_IO_CONFIG_DUPLICATE_MAPPING;
                }
            }
            if (past_Table[sz_I].b_HmiVisible && past_Table[sz_J].b_HmiVisible &&
                (past_Table[sz_I].ui8_UnitId == past_Table[sz_J].ui8_UnitId) &&
                (past_Table[sz_I].e_SignalType == past_Table[sz_J].e_SignalType) &&
                (past_Table[sz_I].ui8_HmiSlot == past_Table[sz_J].ui8_HmiSlot))
            {
                return E_IO_CONFIG_INVALID_HMI_SLOT;
            }
        }
    }

    if ((aui16_TotalTypeCount[E_IO_SIGNAL_DI] > APP_CFG_MAX_DI_COUNT) ||
        (aui16_TotalTypeCount[E_IO_SIGNAL_DO] > APP_CFG_MAX_DO_COUNT) ||
        (aui16_TotalTypeCount[E_IO_SIGNAL_AI] > APP_CFG_MAX_AI_COUNT) ||
        (aui16_TotalTypeCount[E_IO_SIGNAL_AO] > APP_CFG_MAX_AO_COUNT))
    {
        return E_IO_CONFIG_CAPACITY_EXCEEDED;
    }

    for (sz_I = 0U; sz_I < sz_UnitCount; ++sz_I)
    {
        if ((aaui16_UnitTypeCount[sz_I][E_IO_SIGNAL_DI] != past_Units[sz_I].ui8_DiCount) ||
            (aaui16_UnitTypeCount[sz_I][E_IO_SIGNAL_DO] != past_Units[sz_I].ui8_DoCount) ||
            (aaui16_UnitTypeCount[sz_I][E_IO_SIGNAL_AI] != past_Units[sz_I].ui8_AiCount) ||
            (aaui16_UnitTypeCount[sz_I][E_IO_SIGNAL_AO] != past_Units[sz_I].ui8_AoCount))
        {
            return E_IO_CONFIG_UNIT_TOPOLOGY_MISMATCH;
        }
    }

    return E_IO_CONFIG_OK;
}
