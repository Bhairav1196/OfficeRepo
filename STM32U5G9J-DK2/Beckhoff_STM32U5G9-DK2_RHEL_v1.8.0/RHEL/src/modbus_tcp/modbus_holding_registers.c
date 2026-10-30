/**
 * @file modbus_holding_registers.c
 * @brief Function 03/06/16 mapping for EL4074 output commands.
 * @author Prathamesh Chipkar
 */
#include "modbus_tcp/modbus_holding_registers.h"

#include <math.h>

static bool fn_IsCurrentMode(ead_analog_mode_t e_Mode)
{
    return (e_Mode == EAD_ANALOG_BIPOLAR_20MA) ||
           (e_Mode == EAD_ANALOG_UNIPOLAR_20MA) ||
           (e_Mode == EAD_ANALOG_4_TO_20MA);
}

static E_GW_RESULT fn_GetAoMode(S_MODB_DATA_MODEL *pst_Model,
                                uint8_t ui8_UnitId,
                                uint16_t ui16_Channel,
                                ead_analog_mode_t *pe_Mode)
{
    if ((pst_Model == NULL) || (pst_Model->pst_RouteManager == NULL) ||
        (pe_Mode == NULL))
    {
        return GW_ERR_ARGUMENT;
    }

    return fn_GW_RouteGetAnalogOutputMode(pst_Model->pst_RouteManager,
                                          ui8_UnitId,
                                          ui16_Channel,
                                          pe_Mode);
}

static E_GW_RESULT fn_AoRegisterToEngineeringForMode(uint16_t ui16_RegisterWord,
                                                      ead_analog_mode_t e_Mode,
                                                      double *pd_Engineering)
{
    const int16_t i16_Command = (int16_t)ui16_RegisterWord;

    if (pd_Engineering == NULL)
    {
        return GW_ERR_ARGUMENT;
    }

    switch (e_Mode)
    {
        case EAD_ANALOG_BIPOLAR_10V:
            if ((i16_Command < MODB_HR_AO_VOLTAGE_COMMAND_MIN_CENTI_V) ||
                (i16_Command > MODB_HR_AO_VOLTAGE_COMMAND_MAX_CENTI_V))
            {
                return GW_ERR_RANGE;
            }
            *pd_Engineering = (double)i16_Command /
                              MODB_HR_AO_VOLTAGE_SCALE_FACTOR;
            return GW_OK;

        case EAD_ANALOG_UNIPOLAR_10V:
            if ((i16_Command < 0) ||
                (i16_Command > MODB_HR_AO_VOLTAGE_COMMAND_MAX_CENTI_V))
            {
                return GW_ERR_RANGE;
            }
            *pd_Engineering = (double)i16_Command /
                              MODB_HR_AO_VOLTAGE_SCALE_FACTOR;
            return GW_OK;

        case EAD_ANALOG_4_TO_20MA:
            if ((i16_Command < MODB_HR_AO_CURRENT_4_20_COMMAND_MIN_CENTI_MA) ||
                (i16_Command > MODB_HR_AO_CURRENT_4_20_COMMAND_MAX_CENTI_MA))
            {
                return GW_ERR_RANGE;
            }
            *pd_Engineering = (double)i16_Command /
                              MODB_HR_AO_CURRENT_SCALE_FACTOR;
            return GW_OK;

        case EAD_ANALOG_UNIPOLAR_20MA:
            if ((i16_Command < 0) ||
                (i16_Command > MODB_HR_AO_CURRENT_4_20_COMMAND_MAX_CENTI_MA))
            {
                return GW_ERR_RANGE;
            }
            *pd_Engineering = (double)i16_Command /
                              MODB_HR_AO_CURRENT_SCALE_FACTOR;
            return GW_OK;

        case EAD_ANALOG_BIPOLAR_20MA:
        default:
            if ((i16_Command < MODB_HR_AO_BIPOLAR_CURRENT_COMMAND_MIN_CENTI_MA) ||
                (i16_Command > MODB_HR_AO_BIPOLAR_CURRENT_COMMAND_MAX_CENTI_MA))
            {
                return GW_ERR_RANGE;
            }
            *pd_Engineering = (double)i16_Command /
                              MODB_HR_AO_CURRENT_SCALE_FACTOR;
            return GW_OK;
    }
}

static E_GW_RESULT fn_AoEngineeringToRegisterForMode(double d_Engineering,
                                                      ead_analog_mode_t e_Mode,
                                                      uint16_t *pui16_RegisterWord)
{
    long l_Command;

    if ((pui16_RegisterWord == NULL) || !isfinite(d_Engineering))
    {
        return GW_ERR_ARGUMENT;
    }

    if (fn_IsCurrentMode(e_Mode))
    {
        l_Command = lround(d_Engineering * MODB_HR_AO_CURRENT_SCALE_FACTOR);
    }
    else
    {
        l_Command = lround(d_Engineering * MODB_HR_AO_VOLTAGE_SCALE_FACTOR);
    }

    switch (e_Mode)
    {
        case EAD_ANALOG_BIPOLAR_10V:
            if ((l_Command < MODB_HR_AO_VOLTAGE_COMMAND_MIN_CENTI_V) ||
                (l_Command > MODB_HR_AO_VOLTAGE_COMMAND_MAX_CENTI_V))
            {
                return GW_ERR_RANGE;
            }
            break;

        case EAD_ANALOG_UNIPOLAR_10V:
            if ((l_Command < 0L) ||
                (l_Command > MODB_HR_AO_VOLTAGE_COMMAND_MAX_CENTI_V))
            {
                return GW_ERR_RANGE;
            }
            break;

        case EAD_ANALOG_4_TO_20MA:
            /*
             * FC03 readback may legitimately show 0 during startup before the
             * first commanded 4 mA value has been accepted. Writes still use
             * fn_AoRegisterToEngineeringForMode(), which enforces 400..2000.
             */
            if ((l_Command < 0L) ||
                (l_Command > MODB_HR_AO_CURRENT_4_20_COMMAND_MAX_CENTI_MA))
            {
                return GW_ERR_RANGE;
            }
            break;

        case EAD_ANALOG_UNIPOLAR_20MA:
            if ((l_Command < 0L) ||
                (l_Command > MODB_HR_AO_CURRENT_4_20_COMMAND_MAX_CENTI_MA))
            {
                return GW_ERR_RANGE;
            }
            break;

        case EAD_ANALOG_BIPOLAR_20MA:
        default:
            if ((l_Command < MODB_HR_AO_BIPOLAR_CURRENT_COMMAND_MIN_CENTI_MA) ||
                (l_Command > MODB_HR_AO_BIPOLAR_CURRENT_COMMAND_MAX_CENTI_MA))
            {
                return GW_ERR_RANGE;
            }
            break;
    }

    *pui16_RegisterWord = (uint16_t)(int16_t)l_Command;
    return GW_OK;
}

E_GW_RESULT fn_MODB_AoRegisterToEngineering(uint16_t ui16_RegisterWord,
                                             double *pd_Engineering)
{
    return fn_AoRegisterToEngineeringForMode(ui16_RegisterWord,
                                             EAD_ANALOG_BIPOLAR_20MA,
                                             pd_Engineering);
}

E_GW_RESULT fn_MODB_AoEngineeringToRegister(double d_Engineering,
                                             uint16_t *pui16_RegisterWord)
{
    return fn_AoEngineeringToRegisterForMode(d_Engineering,
                                             EAD_ANALOG_BIPOLAR_20MA,
                                             pui16_RegisterWord);
}

static E_GW_RESULT fn_ReadOne(S_MODB_DATA_MODEL *pst_Model,
                              uint8_t ui8_UnitId,
                              uint16_t ui16_Address,
                              uint16_t *pui16_Value)
{
    S_GW_ROUTE_INFO st_Info;
    ead_analog_sample_t st_Sample;
    ead_analog_mode_t e_Mode = EAD_ANALOG_BIPOLAR_20MA;
    E_GW_RESULT e_Result;
    uint16_t ui16_Channel;

    if ((pst_Model == NULL) || (pst_Model->pst_RouteManager == NULL) ||
        (pui16_Value == NULL))
    {
        return GW_ERR_ARGUMENT;
    }
    e_Result = fn_GW_RouteGetInfo(pst_Model->pst_RouteManager,
                                  ui8_UnitId, &st_Info);
    if (e_Result != GW_OK)
    {
        return e_Result;
    }
    if ((uint32_t)ui16_Address >=
        (MODB_HR_AO_COMMAND_BASE + st_Info.sz_AnalogOutputCount))
    {
        return GW_ERR_RANGE;
    }

    ui16_Channel = (uint16_t)((uint32_t)ui16_Address - MODB_HR_AO_COMMAND_BASE);
    e_Result = fn_GW_RouteReadAnalogOutput(
        pst_Model->pst_RouteManager, ui8_UnitId, ui16_Channel, &st_Sample);
    if (e_Result != GW_OK)
    {
        return e_Result;
    }

    e_Result = fn_GetAoMode(pst_Model, ui8_UnitId, ui16_Channel, &e_Mode);
    if (e_Result != GW_OK)
    {
        return e_Result;
    }

    return fn_AoEngineeringToRegisterForMode(st_Sample.engineering,
                                             e_Mode,
                                             pui16_Value);
}

E_GW_RESULT fn_MODB_HoldingRegistersRead(S_MODB_DATA_MODEL *pst_Model,
                                         uint8_t ui8_UnitId,
                                         uint16_t ui16_Start,
                                         uint16_t ui16_Quantity,
                                         uint16_t *pui16_Values)
{
    uint16_t ui16_Index;
    if ((pst_Model == NULL) || (pst_Model->pst_RouteManager == NULL) ||
        (pui16_Values == NULL) || (ui16_Quantity == 0U))
    {
        return GW_ERR_ARGUMENT;
    }
    for (ui16_Index = 0U; ui16_Index < ui16_Quantity; ++ui16_Index)
    {
        const uint32_t ui32_Address =
            (uint32_t)ui16_Start + (uint32_t)ui16_Index;
        E_GW_RESULT e_Result;
        if (ui32_Address > UINT16_MAX)
        {
            return GW_ERR_RANGE;
        }
        e_Result = fn_ReadOne(pst_Model, ui8_UnitId,
                              (uint16_t)ui32_Address,
                              &pui16_Values[ui16_Index]);
        if (e_Result != GW_OK)
        {
            return e_Result;
        }
    }
    return GW_OK;
}

E_GW_RESULT fn_MODB_HoldingRegistersWrite(S_MODB_DATA_MODEL *pst_Model,
                                          uint8_t ui8_UnitId,
                                          uint16_t ui16_Start,
                                          uint16_t ui16_Quantity,
                                          const uint16_t *pui16_Values,
                                          uint32_t ui32_WriterIpv4)
{
    double ad_Engineering[123U];
    uint16_t ui16_Index;
    S_GW_ROUTE_INFO st_Info;
    E_GW_RESULT e_Result;

    if ((pst_Model == NULL) || (pst_Model->pst_RouteManager == NULL) ||
        (pui16_Values == NULL) || (ui16_Quantity == 0U) ||
        (ui16_Quantity > 123U))
    {
        return GW_ERR_ARGUMENT;
    }
    e_Result = fn_GW_RouteGetInfo(pst_Model->pst_RouteManager,
                                  ui8_UnitId, &st_Info);
    if (e_Result != GW_OK)
    {
        return e_Result;
    }
    if (((uint32_t)ui16_Start + (uint32_t)ui16_Quantity) >
        (MODB_HR_AO_COMMAND_BASE + st_Info.sz_AnalogOutputCount))
    {
        return GW_ERR_RANGE;
    }
    for (ui16_Index = 0U; ui16_Index < ui16_Quantity; ++ui16_Index)
    {
        ead_analog_mode_t e_Mode = EAD_ANALOG_BIPOLAR_20MA;
        const uint16_t ui16_Channel =
            (uint16_t)((uint32_t)ui16_Start + (uint32_t)ui16_Index -
                       MODB_HR_AO_COMMAND_BASE);

        e_Result = fn_GetAoMode(pst_Model, ui8_UnitId, ui16_Channel, &e_Mode);
        if (e_Result != GW_OK)
        {
            return e_Result;
        }
        e_Result = fn_AoRegisterToEngineeringForMode(
            pui16_Values[ui16_Index], e_Mode, &ad_Engineering[ui16_Index]);
        if (e_Result != GW_OK)
        {
            return (e_Result == GW_ERR_RANGE) ? GW_ERR_ARGUMENT : e_Result;
        }
    }
    return fn_GW_RouteWriteAnalogOutputs(
        pst_Model->pst_RouteManager, ui8_UnitId,
        (uint16_t)((uint32_t)ui16_Start - MODB_HR_AO_COMMAND_BASE),
        ui16_Quantity, ad_Engineering, ui32_WriterIpv4);
}
