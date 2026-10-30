/**
 * @file modbus_coils.h
 * @brief Modbus Coil table mapped to EL2409 digital outputs.
 * @author Prathamesh Chipkar
 */
#ifndef MODB_COILS_H
#define MODB_COILS_H

#include "modbus_tcp/modbus_data_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Read packed Coil states using zero-based Modbus addresses. */
E_GW_RESULT fn_MODB_CoilsRead(S_MODB_DATA_MODEL *pst_Model,
                              uint8_t ui8_UnitId,
                              uint16_t ui16_Start,
                              uint16_t ui16_Quantity,
                              uint8_t *pui8_Packed,
                              size_t sz_PackedCapacity);

/** @brief Write packed Coil states to EL2409 outputs. */
E_GW_RESULT fn_MODB_CoilsWrite(S_MODB_DATA_MODEL *pst_Model,
                               uint8_t ui8_UnitId,
                               uint16_t ui16_Start,
                               uint16_t ui16_Quantity,
                               const uint8_t *pui8_Packed,
                               uint32_t ui32_WriterIpv4);

#ifdef __cplusplus
}
#endif

#endif
