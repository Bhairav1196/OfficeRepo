/**
 * @file modbus_discrete_inputs.h
 * @brief Modbus Discrete Input table mapped to EL1409 digital inputs.
 * @author Prathamesh Chipkar
 */
#ifndef MODB_DISCRETE_INPUTS_H
#define MODB_DISCRETE_INPUTS_H

#include "modbus_tcp/modbus_data_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Read packed Discrete Input states using zero-based addresses. */
E_GW_RESULT fn_MODB_DiscreteInputsRead(S_MODB_DATA_MODEL *pst_Model,
                                       uint8_t ui8_UnitId,
                                       uint16_t ui16_Start,
                                       uint16_t ui16_Quantity,
                                       uint8_t *pui8_Packed,
                                       size_t sz_PackedCapacity);

#ifdef __cplusplus
}
#endif

#endif
