/**
 * @file test_ao_scaling.c
 * @brief Regression tests for the canonical EL4074 Modbus-to-REAL32 current scaling contract.
 * @author Prathamesh Chipkar
 */
#include "modbus_tcp/modbus_holding_registers.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define TEST_TOLERANCE (1.0e-12)

static int fn_CheckRegisterToEngineering(int16_t i16_Command, double d_Expected)
{
    double d_Actual = 0.0;
    const E_GW_RESULT e_Result = fn_MODB_AoRegisterToEngineering(
        (uint16_t)i16_Command, &d_Actual);
    if ((e_Result != GW_OK) || (fabs(d_Actual - d_Expected) > TEST_TOLERANCE))
    {
        (void)fprintf(stderr,
                      "AO decode failed: command=%d result=%d actual=%.12f expected=%.12f\n",
                      (int)i16_Command, (int)e_Result, d_Actual, d_Expected);
        return 1;
    }
    return 0;
}

static int fn_CheckEngineeringToRegister(double d_Engineering, int16_t i16_Expected)
{
    uint16_t ui16_Word = 0U;
    const E_GW_RESULT e_Result = fn_MODB_AoEngineeringToRegister(
        d_Engineering, &ui16_Word);
    if ((e_Result != GW_OK) || ((int16_t)ui16_Word != i16_Expected))
    {
        (void)fprintf(stderr,
                      "AO encode failed: engineering=%.12f result=%d actual=%d expected=%d\n",
                      d_Engineering, (int)e_Result, (int)(int16_t)ui16_Word,
                      (int)i16_Expected);
        return 1;
    }
    return 0;
}

int main(void)
{
    static const struct
    {
        int16_t i16_Command;
        double d_Amperes;
    } ast_Cases[] = {
        {-2000, -0.02000},
        {-500, -0.00500},
        {-1, -0.00001},
        {0, 0.0},
        {1, 0.00001},
        {132, 0.00132},
        {899, 0.00899},
        {2000, 0.02000}
    };
    size_t sz_Index;
    double d_Dummy = 0.0;
    uint16_t ui16_Dummy = 0U;

    for (sz_Index = 0U; sz_Index < (sizeof(ast_Cases) / sizeof(ast_Cases[0]));
         ++sz_Index)
    {
        if (fn_CheckRegisterToEngineering(ast_Cases[sz_Index].i16_Command,
                                          ast_Cases[sz_Index].d_Amperes) != 0)
        {
            return 1;
        }
        if (fn_CheckEngineeringToRegister(ast_Cases[sz_Index].d_Amperes,
                                          ast_Cases[sz_Index].i16_Command) != 0)
        {
            return 1;
        }
    }

    if (fn_MODB_AoRegisterToEngineering((uint16_t)(int16_t)2001, &d_Dummy) !=
        GW_ERR_RANGE)
    {
        return 1;
    }
    if (fn_MODB_AoRegisterToEngineering((uint16_t)(int16_t)-2001, &d_Dummy) !=
        GW_ERR_RANGE)
    {
        return 1;
    }
    if (fn_MODB_AoRegisterToEngineering(0U, NULL) != GW_ERR_ARGUMENT)
    {
        return 1;
    }
    if (fn_MODB_AoEngineeringToRegister(0.02001, &ui16_Dummy) != GW_ERR_RANGE)
    {
        return 1;
    }
    if (fn_MODB_AoEngineeringToRegister(-0.02001, &ui16_Dummy) != GW_ERR_RANGE)
    {
        return 1;
    }
    if (fn_MODB_AoEngineeringToRegister(NAN, &ui16_Dummy) != GW_ERR_ARGUMENT)
    {
        return 1;
    }
    if (fn_MODB_AoEngineeringToRegister(INFINITY, &ui16_Dummy) != GW_ERR_ARGUMENT)
    {
        return 1;
    }
    if (fn_MODB_AoEngineeringToRegister(0.0, NULL) != GW_ERR_ARGUMENT)
    {
        return 1;
    }

    (void)puts("AO scaling regression: PASS (132 -> 0.00132 A -> 1.32 mA)");
    return 0;
}
