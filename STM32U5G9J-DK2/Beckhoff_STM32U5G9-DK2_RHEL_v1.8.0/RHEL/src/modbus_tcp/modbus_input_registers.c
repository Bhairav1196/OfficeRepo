/**
 * @file modbus_input_registers.c
 * @brief Function 04 mapping for EL3074 measurements and terminal status.
 * @author Prathamesh Chipkar
 */
#include "modbus_tcp/modbus_input_registers.h"

#include <math.h>

static int16_t fn_ClampS16FromDouble(double d_Value)
{
    long l_Value;

    if (d_Value > 32767.0)
    {
        return INT16_MAX;
    }
    if (d_Value < -32768.0)
    {
        return INT16_MIN;
    }

    l_Value = lround(d_Value);
    if (l_Value > 32767L)
    {
        return INT16_MAX;
    }
    if (l_Value < -32768L)
    {
        return INT16_MIN;
    }
    return (int16_t)l_Value;
}

static int16_t fn_ToScaledEngineeringUnit(double d_Engineering,
                                           ead_analog_mode_t e_Mode)
{
    double d_Scaled;

    switch (e_Mode)
    {
        case EAD_ANALOG_BIPOLAR_20MA:
        case EAD_ANALOG_UNIPOLAR_20MA:
        case EAD_ANALOG_4_TO_20MA:
            /* Internal engineering value is SI amperes. Modbus exposes 0.01 mA/count. */
            d_Scaled = d_Engineering * 100000.0;
            break;

        case EAD_ANALOG_BIPOLAR_10V:
        case EAD_ANALOG_UNIPOLAR_10V:
        default:
            /* Internal engineering value is volts. Modbus exposes mV/count. */
            d_Scaled = d_Engineering * 1000.0;
            break;
    }

    return fn_ClampS16FromDouble(d_Scaled);
}

static E_GW_RESULT fn_ReadOne(S_MODB_DATA_MODEL *pst_Model,
                              uint8_t ui8_UnitId,
                              uint16_t ui16_Address,
                              uint16_t *pui16_Value)
{
    S_GW_ROUTE_INFO st_Info;
    ead_analog_sample_t st_Sample;
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

    if ((uint32_t)ui16_Address <
        (MODB_IR_AI_RAW_BASE + st_Info.sz_AnalogInputCount))
    {
        ui16_Channel = (uint16_t)((uint32_t)ui16_Address -
                                  MODB_IR_AI_RAW_BASE);
        e_Result = fn_GW_RouteReadAnalogInput(pst_Model->pst_RouteManager,
                                              ui8_UnitId, ui16_Channel,
                                              &st_Sample);
        if (e_Result == GW_OK)
        {
            *pui16_Value = (uint16_t)st_Sample.raw;
        }
        return e_Result;
    }
    if (((uint32_t)ui16_Address >= MODB_IR_AI_ENGINEERING_BASE) &&
        ((uint32_t)ui16_Address <
         (MODB_IR_AI_ENGINEERING_BASE + st_Info.sz_AnalogInputCount)))
    {
        ead_analog_mode_t e_Mode = EAD_ANALOG_BIPOLAR_10V;
        ui16_Channel = (uint16_t)((uint32_t)ui16_Address -
                                  MODB_IR_AI_ENGINEERING_BASE);
        e_Result = fn_GW_RouteReadAnalogInput(pst_Model->pst_RouteManager,
                                              ui8_UnitId, ui16_Channel,
                                              &st_Sample);
        if (e_Result != GW_OK)
        {
            return e_Result;
        }
        e_Result = fn_GW_RouteGetAnalogInputMode(pst_Model->pst_RouteManager,
                                                 ui8_UnitId, ui16_Channel,
                                                 &e_Mode);
        if (e_Result != GW_OK)
        {
            return e_Result;
        }
        *pui16_Value = (uint16_t)fn_ToScaledEngineeringUnit(st_Sample.engineering,
                                                             e_Mode);
        return GW_OK;
    }
    if (((uint32_t)ui16_Address >= MODB_IR_AI_STATUS_BASE) &&
        ((uint32_t)ui16_Address <
         (MODB_IR_AI_STATUS_BASE + st_Info.sz_AnalogInputCount)))
    {
        ui16_Channel = (uint16_t)((uint32_t)ui16_Address -
                                  MODB_IR_AI_STATUS_BASE);
        e_Result = fn_GW_RouteReadAnalogInput(pst_Model->pst_RouteManager,
                                              ui8_UnitId, ui16_Channel,
                                              &st_Sample);
        if (e_Result == GW_OK)
        {
            *pui16_Value = st_Sample.status;
        }
        return e_Result;
    }
    if (((uint32_t)ui16_Address >= MODB_IR_AO_STATUS_BASE) &&
        ((uint32_t)ui16_Address <
         (MODB_IR_AO_STATUS_BASE + st_Info.sz_AnalogOutputCount)))
    {
        ui16_Channel = (uint16_t)((uint32_t)ui16_Address -
                                  MODB_IR_AO_STATUS_BASE);
        e_Result = fn_GW_RouteReadAnalogOutput(pst_Model->pst_RouteManager,
                                               ui8_UnitId, ui16_Channel,
                                               &st_Sample);
        if (e_Result == GW_OK)
        {
            *pui16_Value = st_Sample.status;
        }
        return e_Result;
    }
    return GW_ERR_RANGE;
}

E_GW_RESULT fn_MODB_InputRegistersRead(S_MODB_DATA_MODEL *pst_Model,
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
