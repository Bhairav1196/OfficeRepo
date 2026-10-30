/**
 * @file    test_io_config.c
 * @brief   Host tests for I/O configuration, scaling, mapping and limits.
 * @author  Prathamesh Chipkar
 * @date    09-Sep-2026
 * @version 2.0.0-alpha.9.0
 */
#include "io_config.h"
#include "io_service.h"
#include "io_signal_ids.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool fn_Close(float a, float b)
{
    return fabsf(a - b) < 0.001F;
}

static S_IO_SignalConfig fn_FinAngleConfig(void)
{
    S_IO_SignalConfig st = {0};
    st.ui16_SignalId = 9000U;
    st.pc_Name = "PORT FIN ANGLE";
    st.e_SignalType = E_IO_SIGNAL_AI;
    st.b_Enabled = true;
    st.b_Writable = false;
    st.ui8_UnitId = 1U;
    st.ui8_ReadFunctionCode = 0x04U;
    st.ui16_RegisterAddress = 5000U;
    st.e_DataType = E_IO_DATATYPE_INT16;
    st.e_AnalogMode = E_IO_ANALOG_CURRENT_BIPOLAR_20MA;
    st.f32_RegisterToElectricalScale = 0.01F;
    st.st_ElectricalHardwareRange = (S_IO_Range){-20.0F, 20.0F};
    st.st_ElectricalOperatingRange = (S_IO_Range){-18.0F, 18.0F};
    st.st_ElectricalMapRange = (S_IO_Range){-20.0F, 20.0F};
    st.e_MappingType = E_IO_MAPPING_LINEAR;
    st.st_ProcessMapRange = (S_IO_Range){-35.0F, 35.0F};
    st.st_ProcessOperatingRange = (S_IO_Range){-30.0F, 30.0F};
    st.pc_ProcessUnit = "deg";
    st.ui8_DecimalPlaces = 3U;
    return st;
}

static void fn_TestConfiguration(void)
{
    const S_IO_Range st_Current = fn_IOConfig_GetModeHardwareRange(E_IO_ANALOG_CURRENT_BIPOLAR_20MA);
    const S_IO_Range st_Voltage = fn_IOConfig_GetModeHardwareRange(E_IO_ANALOG_VOLTAGE_BIPOLAR_10V);
    assert(fn_IOConfig_ValidateAll() == E_IO_CONFIG_OK);
    assert(strcmp(fn_IOConfig_GetElectricalUnit(E_IO_ANALOG_CURRENT_BIPOLAR_20MA), "mA") == 0);
    assert(strcmp(fn_IOConfig_GetElectricalUnit(E_IO_ANALOG_VOLTAGE_BIPOLAR_10V), "V") == 0);
    assert(fn_Close(st_Current.f32_Min, -20.0F) && fn_Close(st_Current.f32_Max, 20.0F));
    assert(fn_Close(st_Voltage.f32_Min, -10.0F) && fn_Close(st_Voltage.f32_Max, 10.0F));
}


static void fn_TestExactSingleUnitTopology(void)
{
    size_t sz_UnitCount = 0U;
    size_t sz_SignalCount = 0U;
    const S_IO_UnitConfig *past_Units = fn_IOConfig_GetUnitTable(&sz_UnitCount);
    const S_IO_SignalConfig *past_Signals = fn_IOConfig_GetTable(&sz_SignalCount);
    const S_IO_SignalConfig *pst_Signal;
    size_t sz_Di = 0U;
    size_t sz_Do = 0U;
    size_t sz_Ai = 0U;
    size_t sz_Ao = 0U;

    assert(past_Units != NULL);
    assert(past_Signals != NULL);
    assert(sz_UnitCount == 1U);
    assert(sz_SignalCount == 40U);

    assert(past_Units[0].ui8_UnitId == 1U);
    assert(past_Units[0].ui8_DiCount == 16U);
    assert(past_Units[0].ui8_DoCount == 16U);
    assert(past_Units[0].ui8_AiCount == 4U);
    assert(past_Units[0].ui8_AoCount == 4U);

    for (size_t i = 0U; i < sz_SignalCount; ++i)
    {
        switch (past_Signals[i].e_SignalType)
        {
            case E_IO_SIGNAL_DI: ++sz_Di; break;
            case E_IO_SIGNAL_DO: ++sz_Do; break;
            case E_IO_SIGNAL_AI: ++sz_Ai; break;
            case E_IO_SIGNAL_AO: ++sz_Ao; break;
            default: assert(0);
        }
    }
    assert(sz_Di == 16U);
    assert(sz_Do == 16U);
    assert(sz_Ai == 4U);
    assert(sz_Ao == 4U);

    /* Unit-specific HMI addressing must expose all 16 digital slots. */
    assert(fn_IOConfig_FindUnitHmiSlot(1U, E_IO_SIGNAL_DI, 0U) != NULL);
    assert(fn_IOConfig_FindUnitHmiSlot(1U, E_IO_SIGNAL_DI, 15U) != NULL);
    assert(fn_IOConfig_FindUnitHmiSlot(1U, E_IO_SIGNAL_DO, 15U) != NULL);
    assert(fn_IOConfig_FindUnitHmiSlot(1U, E_IO_SIGNAL_AO, 0U) != NULL);
    assert(fn_IOConfig_FindUnitHmiSlot(2U, E_IO_SIGNAL_DI, 0U) == NULL);

    /* The one unit defaults to +/-10 V AI and +/-20 mA AO. */
    pst_Signal = fn_IOConfig_FindById(IO_ID_U1_AI(1U));
    assert(pst_Signal != NULL);
    assert(pst_Signal->e_AnalogMode == E_IO_ANALOG_VOLTAGE_BIPOLAR_10V);
    assert(fn_Close(pst_Signal->st_ElectricalHardwareRange.f32_Min, -10.0F));
    assert(fn_Close(pst_Signal->st_ElectricalHardwareRange.f32_Max, 10.0F));
    assert(pst_Signal->ui8_DecimalPlaces == 3U);

    pst_Signal = fn_IOConfig_FindById(IO_ID_U1_AO(1U));
    assert(pst_Signal != NULL);
    assert(pst_Signal->e_AnalogMode == E_IO_ANALOG_CURRENT_BIPOLAR_20MA);
    assert(fn_Close(pst_Signal->st_ElectricalHardwareRange.f32_Min, -20.0F));
    assert(fn_Close(pst_Signal->st_ElectricalHardwareRange.f32_Max, 20.0F));
    assert(pst_Signal->ui8_DecimalPlaces == 3U);
}

static void fn_TestFinMappingIndependence(void)
{
    const S_IO_SignalConfig st = fn_FinAngleConfig();
    /* Mapping remains based on +/-20 mA even though operating range is +/-18 mA. */
    assert(fn_Close(fn_IOService_ElectricalToProcess(&st, 20.0F), 35.0F));
    assert(fn_Close(fn_IOService_ElectricalToProcess(&st, 10.0F), 17.5F));
    assert(fn_Close(fn_IOService_ElectricalToProcess(&st, 0.0F), 0.0F));
    assert(fn_Close(fn_IOService_ElectricalToProcess(&st, -20.0F), -35.0F));
    assert(fn_Close(fn_IOService_ProcessToElectrical(&st, 17.5F), 10.0F));
    assert(fn_Close(fn_IOService_ProcessToElectrical(&st, 35.0F), 20.0F));
}

static void fn_TestDefaultAoRangeAndScaling(void)
{
    int16_t i16_Register = 0;
    float f32_Applied = 0.0F;
    assert(fn_IOService_Init() == E_IO_CONFIG_OK);
    assert(fn_IOService_ProcessToRegister(IO_ID_U1_AO(1U), 20.0F, &i16_Register, &f32_Applied) == E_IO_STATUS_OK);
    assert(i16_Register == 2000);
    assert(fn_Close(f32_Applied, 20.0F));
    assert(fn_IOService_ProcessToRegister(IO_ID_U1_AO(1U), -20.0F, &i16_Register, NULL) == E_IO_STATUS_OK);
    assert(i16_Register == -2000);
    assert(fn_IOService_ProcessToRegister(IO_ID_U1_AO(1U), 20.001F, &i16_Register, NULL) == E_IO_STATUS_OUT_OF_RANGE);
    assert(fn_IOService_ProcessToRegister(IO_ID_U1_AO(1U), -20.001F, &i16_Register, NULL) == E_IO_STATUS_OUT_OF_RANGE);
}

static void fn_TestAiOutOfRangePreservesMeasurement(void)
{
    const int16_t ai16_InRange[4] = {10000, 0, 0, 0};
    const int16_t ai16_OutOfRange[4] = {12000, 0, 0, 0};
    S_IO_SignalValue st_Value;
    assert(fn_IOService_Init() == E_IO_CONFIG_OK);
    fn_IOService_UpdateAnalogRegisters(1U, E_IO_SIGNAL_AI, 1024U, ai16_InRange, 4U, 100U);
    assert(fn_IOService_GetValue(IO_ID_U1_AI(1U), &st_Value));
    assert(st_Value.e_Quality == E_IO_QUALITY_GOOD);
    assert(fn_Close(st_Value.f32_ProcessValue, 10.0F));

    fn_IOService_UpdateAnalogRegisters(1U, E_IO_SIGNAL_AI, 1024U, ai16_OutOfRange, 4U, 200U);
    assert(fn_IOService_GetValue(IO_ID_U1_AI(1U), &st_Value));
    assert(st_Value.e_Quality == E_IO_QUALITY_OUT_OF_RANGE);
    assert(fn_Close(st_Value.f32_ElectricalValue, 12.0F));
    assert(fn_Close(st_Value.f32_ProcessValue, 12.0F));
}

static void fn_TestPiecewiseMapping(void)
{
    static const S_IO_CalibrationPoint ast_Points[] = {
        {-20.0F, -35.0F},
        {-10.0F, -16.0F},
        {  0.0F,   0.0F},
        { 10.0F,  18.0F},
        { 20.0F,  35.0F}
    };
    S_IO_SignalConfig st = fn_FinAngleConfig();
    st.e_MappingType = E_IO_MAPPING_PIECEWISE_LINEAR;
    st.past_Calibration = ast_Points;
    st.ui8_CalibrationPointCount = (uint8_t)(sizeof(ast_Points) / sizeof(ast_Points[0]));
    assert(fn_Close(fn_IOService_ElectricalToProcess(&st, 5.0F), 9.0F));
    assert(fn_Close(fn_IOService_ProcessToElectrical(&st, 9.0F), 5.0F));
}

int main(void)
{
    fn_TestConfiguration();
    fn_TestExactSingleUnitTopology();
    fn_TestFinMappingIndependence();
    fn_TestDefaultAoRangeAndScaling();
    fn_TestAiOutOfRangePreservesMeasurement();
    fn_TestPiecewiseMapping();
    puts("io configuration/scaling tests: PASS");
    return 0;
}
