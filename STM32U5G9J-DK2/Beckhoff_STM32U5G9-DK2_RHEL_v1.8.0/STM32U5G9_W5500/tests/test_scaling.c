/**
 * @file    test_scaling.c
 * @brief   Host regression tests for analog electrical/process scaling.
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

static bool fn_Close(float f32_A, float f32_B)
{
    return fabsf(f32_A - f32_B) < 0.001F;
}

static void fn_TestVoltageAi(void)
{
    const int16_t ai16_Registers[4] = {10000, -5000, 0, -10000};
    const int16_t ai16_OutOfRange[4] = {10001, 0, 0, 0};
    S_IO_SignalValue st_Value;
    const S_IO_SignalConfig *pst_Config = fn_IOConfig_FindById(IO_ID_U1_AI(1U));

    assert(pst_Config != NULL);
    assert(pst_Config->e_AnalogMode == E_IO_ANALOG_VOLTAGE_BIPOLAR_10V);
    assert(strcmp(fn_IOConfig_GetElectricalUnit(pst_Config->e_AnalogMode), "V") == 0);
    assert(fn_IOService_Init() == E_IO_CONFIG_OK);

    fn_IOService_UpdateAnalogRegisters(1U, E_IO_SIGNAL_AI, 1024U,
                                       ai16_Registers, 4U, 100U);
    assert(fn_IOService_GetValue(IO_ID_U1_AI(1U), &st_Value));
    assert(st_Value.i32_RawValue == 10000);
    assert(fn_Close(st_Value.f32_ElectricalValue, 10.0F));
    assert(fn_Close(st_Value.f32_ProcessValue, 10.0F));
    assert(st_Value.e_Quality == E_IO_QUALITY_GOOD);

    assert(fn_IOService_GetValue(IO_ID_U1_AI(2U), &st_Value));
    assert(fn_Close(st_Value.f32_ElectricalValue, -5.0F));
    assert(fn_Close(st_Value.f32_ProcessValue, -5.0F));

    fn_IOService_UpdateAnalogRegisters(1U, E_IO_SIGNAL_AI, 1024U,
                                       ai16_OutOfRange, 4U, 200U);
    assert(fn_IOService_GetValue(IO_ID_U1_AI(1U), &st_Value));
    assert(fn_Close(st_Value.f32_ElectricalValue, 10.001F));
    assert(st_Value.e_Quality == E_IO_QUALITY_OUT_OF_RANGE);
}

static void fn_TestCurrentAo(void)
{
    int16_t i16_Register = 0;
    float f32_Applied = 0.0F;
    const S_IO_SignalConfig *pst_Config = fn_IOConfig_FindById(IO_ID_U1_AO(1U));

    assert(pst_Config != NULL);
    assert(pst_Config->e_AnalogMode == E_IO_ANALOG_CURRENT_BIPOLAR_20MA);
    assert(strcmp(fn_IOConfig_GetElectricalUnit(pst_Config->e_AnalogMode), "mA") == 0);
    assert(fn_IOService_Init() == E_IO_CONFIG_OK);

    assert(fn_IOService_ProcessToRegister(IO_ID_U1_AO(1U), 1.320F,
                                          &i16_Register, &f32_Applied) == E_IO_STATUS_OK);
    assert(i16_Register == 132);
    assert(fn_Close(f32_Applied, 1.320F));

    assert(fn_IOService_ProcessToRegister(IO_ID_U1_AO(1U), 20.000F,
                                          &i16_Register, NULL) == E_IO_STATUS_OK);
    assert(i16_Register == 2000);
    assert(fn_IOService_ProcessToRegister(IO_ID_U1_AO(1U), -20.000F,
                                          &i16_Register, NULL) == E_IO_STATUS_OK);
    assert(i16_Register == -2000);
    assert(fn_IOService_ProcessToRegister(IO_ID_U1_AO(1U), 20.001F,
                                          &i16_Register, NULL) == E_IO_STATUS_OUT_OF_RANGE);
    assert(fn_IOService_ProcessToRegister(IO_ID_U1_AO(1U), -20.001F,
                                          &i16_Register, NULL) == E_IO_STATUS_OUT_OF_RANGE);
}

static void fn_TestElectricalModeDefinitions(void)
{
    const S_IO_Range st_BipolarCurrent = fn_IOConfig_GetModeHardwareRange(E_IO_ANALOG_CURRENT_BIPOLAR_20MA);
    const S_IO_Range st_Current020 = fn_IOConfig_GetModeHardwareRange(E_IO_ANALOG_CURRENT_0_20MA);
    const S_IO_Range st_Current420 = fn_IOConfig_GetModeHardwareRange(E_IO_ANALOG_CURRENT_4_20MA);
    const S_IO_Range st_BipolarVoltage = fn_IOConfig_GetModeHardwareRange(E_IO_ANALOG_VOLTAGE_BIPOLAR_10V);
    const S_IO_Range st_Voltage010 = fn_IOConfig_GetModeHardwareRange(E_IO_ANALOG_VOLTAGE_0_10V);

    assert(fn_Close(st_BipolarCurrent.f32_Min, -20.0F) && fn_Close(st_BipolarCurrent.f32_Max, 20.0F));
    assert(fn_Close(st_Current020.f32_Min, 0.0F) && fn_Close(st_Current020.f32_Max, 20.0F));
    assert(fn_Close(st_Current420.f32_Min, 4.0F) && fn_Close(st_Current420.f32_Max, 20.0F));
    assert(fn_Close(st_BipolarVoltage.f32_Min, -10.0F) && fn_Close(st_BipolarVoltage.f32_Max, 10.0F));
    assert(fn_Close(st_Voltage010.f32_Min, 0.0F) && fn_Close(st_Voltage010.f32_Max, 10.0F));
}

int main(void)
{
    fn_TestElectricalModeDefinitions();
    fn_TestVoltageAi();
    fn_TestCurrentAo();
    puts("analog scaling tests: PASS");
    return 0;
}
