#ifndef IO_CONFIG_H
#define IO_CONFIG_H

#include <stddef.h>
#include "io_config_types.h"

/**
 * @file    io_config.h
 * @brief   Public API for the authoritative Beckhoff topology and I/O configuration tables.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#ifdef __cplusplus
extern "C" {
#endif

#define IO_CFG_SIGNAL_ID_INVALID  (0xFFFFU)

/** @brief Return the immutable configured Beckhoff-unit topology table. */
const S_IO_UnitConfig *fn_IOConfig_GetUnitTable(size_t *p_sz_Count);
/** @brief Find one configured Beckhoff unit. */
const S_IO_UnitConfig *fn_IOConfig_FindUnit(uint8_t ui8_UnitId);
/** @brief Return the immutable configured signal table. */
const S_IO_SignalConfig *fn_IOConfig_GetTable(size_t *p_sz_Count);
/** @brief Find one configured signal by stable signal ID. */
const S_IO_SignalConfig *fn_IOConfig_FindById(uint16_t ui16_SignalId);
/** @brief Find one enabled/visible signal in a unit/type-specific HMI slot. */
const S_IO_SignalConfig *fn_IOConfig_FindUnitHmiSlot(uint8_t ui8_UnitId,
                                                     E_IO_SignalType e_Type,
                                                     uint8_t ui8_Slot);
/** @brief Derive the electrical unit from the configured analog mode. */
const char *fn_IOConfig_GetElectricalUnit(E_IO_AnalogMode e_Mode);
/** @brief Return the standard electrical hardware range for a predefined analog mode. */
S_IO_Range fn_IOConfig_GetModeHardwareRange(E_IO_AnalogMode e_Mode);
/** @brief Validate unit topology, signal mappings, ranges, access and HMI slots. */
E_IO_ConfigStatus fn_IOConfig_ValidateAll(void);
/** @brief Convert a configuration validation status to diagnostic text. */
const char *fn_IOConfig_StatusString(E_IO_ConfigStatus e_Status);

#ifdef __cplusplus
}
#endif

#endif /* IO_CONFIG_H */
