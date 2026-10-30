/**
 * @file modbus_input_registers.h
 * @brief Modbus Input Register table mapped to EL3074 values and I/O status.
 * @author Prathamesh Chipkar
 */
#ifndef MODB_INPUT_REGISTERS_H
#define MODB_INPUT_REGISTERS_H

#include "modbus_tcp/modbus_data_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief First EL3074 raw signed sample register. */
#define MODB_IR_AI_RAW_BASE             (0U)
/** @brief First EL3074 scaled engineering register. */
#define MODB_IR_AI_ENGINEERING_BASE     (1024U)
/** @brief Backward-compatible alias retained for the original bipolar-voltage profile. */
#define MODB_IR_AI_MV_BASE              MODB_IR_AI_ENGINEERING_BASE
/** @brief First EL3074 status-word register. */
#define MODB_IR_AI_STATUS_BASE          (2048U)
#define MODB_IR_AO_STATUS_BASE          (3072U)

/**
 * @brief Read Input Registers using the documented zero-based map.
 *
 * Engineering block 1024..1027 is profile-dependent:
 * - @c bipolar_10v: signed mV/count.
 * - @c bipolar_20ma: signed 0.01 mA/count, -20.00..+20.00 mA.
 */
E_GW_RESULT fn_MODB_InputRegistersRead(S_MODB_DATA_MODEL *pst_Model,
                                       uint8_t ui8_UnitId,
                                       uint16_t ui16_Start,
                                       uint16_t ui16_Quantity,
                                       uint16_t *pui16_Values);

#ifdef __cplusplus
}
#endif

#endif
