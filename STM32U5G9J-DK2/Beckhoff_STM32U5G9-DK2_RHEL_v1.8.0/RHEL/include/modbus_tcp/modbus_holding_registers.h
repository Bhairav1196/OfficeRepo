/**
 * @file modbus_holding_registers.h
 * @brief Modbus Holding Register table mapped to EL4074 analog outputs.
 * @author Prathamesh Chipkar
 */
#ifndef MODB_HOLDING_REGISTERS_H
#define MODB_HOLDING_REGISTERS_H

#include "modbus_tcp/modbus_data_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief First zero-based Holding Register containing an AO command. */
#define MODB_HR_AO_COMMAND_BASE (0U)

/** @brief Scale from signed Modbus register to volts for +/-10 V AO mode. */
#define MODB_HR_AO_VOLTAGE_SCALE_FACTOR (100.0)
/** @brief Lowest signed AO register command for -10.00 V. */
#define MODB_HR_AO_VOLTAGE_COMMAND_MIN_CENTI_V (-1000)
/** @brief Highest signed AO register command for +10.00 V. */
#define MODB_HR_AO_VOLTAGE_COMMAND_MAX_CENTI_V (1000)

/** @brief Scale from signed Modbus register to amperes for current AO modes. */
#define MODB_HR_AO_CURRENT_SCALE_FACTOR (100000.0)
/** @brief Lowest signed AO register command for 4.00 mA current-loop mode. */
#define MODB_HR_AO_CURRENT_4_20_COMMAND_MIN_CENTI_MA (400)
/** @brief Highest signed AO register command for 20.00 mA current-loop mode. */
#define MODB_HR_AO_CURRENT_4_20_COMMAND_MAX_CENTI_MA (2000)
/** @brief Lowest valid signed AO register command for -20 mA. */
#define MODB_HR_AO_BIPOLAR_CURRENT_COMMAND_MIN_CENTI_MA (-2000)
/** @brief Highest valid signed AO register command for +20 mA. */
#define MODB_HR_AO_BIPOLAR_CURRENT_COMMAND_MAX_CENTI_MA (2000)

/** Backward-compatible aliases retained for earlier current-mode tests/docs. */
#define MODB_HR_AO_SCALE_FACTOR MODB_HR_AO_CURRENT_SCALE_FACTOR
#define MODB_HR_AO_COMMAND_MIN MODB_HR_AO_BIPOLAR_CURRENT_COMMAND_MIN_CENTI_MA
#define MODB_HR_AO_COMMAND_MAX MODB_HR_AO_BIPOLAR_CURRENT_COMMAND_MAX_CENTI_MA

/**
 * @brief Convert one bipolar-current Modbus AO register word to amperes.
 *
 * This compatibility function keeps the v1.2.4/v1.5.0 behaviour for unit
 * tests and older tools: signed register command / 100000.0 = amperes.
 * The live gateway path uses the configured EL4074 @c analog_mode instead.
 */
E_GW_RESULT fn_MODB_AoRegisterToEngineering(uint16_t ui16_RegisterWord,
                                             double *pd_Engineering);

/**
 * @brief Convert one bipolar-current AO engineering value back to a register word.
 */
E_GW_RESULT fn_MODB_AoEngineeringToRegister(double d_Engineering,
                                             uint16_t *pui16_RegisterWord);

/**
 * @brief Read EL4074 AO commands in the configured profile scale.
 *
 * - @c bipolar_10v: signed 0.01 V/count, -1000..+1000.
 * - @c bipolar_20ma: signed 0.01 mA/count, -2000..+2000.
 * - @c current_4_20ma: deprecated RC4 compatibility profile, 400..2000.
 */
E_GW_RESULT fn_MODB_HoldingRegistersRead(S_MODB_DATA_MODEL *pst_Model,
                                         uint8_t ui8_UnitId,
                                         uint16_t ui16_Start,
                                         uint16_t ui16_Quantity,
                                         uint16_t *pui16_Values);

/**
 * @brief Write EL4074 AO commands supplied in the configured profile scale.
 */
E_GW_RESULT fn_MODB_HoldingRegistersWrite(S_MODB_DATA_MODEL *pst_Model,
                                          uint8_t ui8_UnitId,
                                          uint16_t ui16_Start,
                                          uint16_t ui16_Quantity,
                                          const uint16_t *pui16_Values,
                                          uint32_t ui32_WriterIpv4);

#ifdef __cplusplus
}
#endif

#endif
