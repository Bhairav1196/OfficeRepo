#ifndef IO_SERVICE_H
#define IO_SERVICE_H

#include <stdbool.h>
#include <stdint.h>
#include "io_config_types.h"

/**
 * @file    io_service.h
 * @brief   Configuration-driven process I/O conversion and validation service.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint16_t     ui16_SignalId;
    bool         b_Valid;
    bool         b_DigitalState;
    int32_t      i32_RawValue;
    float        f32_ElectricalValue;
    float        f32_ProcessValue;
    E_IO_Quality e_Quality;
    uint32_t     ui32_TimestampMs;
} S_IO_SignalValue;

/** @brief Validate configuration and initialize all signal states. */
E_IO_ConfigStatus fn_IOService_Init(void);
/** @brief Apply a communication/configuration quality state to all enabled signals. */
void fn_IOService_SetCommunicationQuality(E_IO_Quality e_Quality);
/** @brief Update configured DI/DO channels from one 16-bit Modbus bit mask. */
void fn_IOService_UpdateDigitalMask(uint8_t ui8_UnitId, E_IO_SignalType e_Type,
                                    uint16_t ui16_Mask, uint32_t ui32_TimestampMs);
/** @brief Convert and publish a contiguous AI/AO register block into process-domain values. */
void fn_IOService_UpdateAnalogRegisters(uint8_t ui8_UnitId, E_IO_SignalType e_Type,
                                        uint16_t ui16_StartAddress, const int16_t *pi16_Values,
                                        uint16_t ui16_Quantity, uint32_t ui32_TimestampMs);
/** @brief Read the current runtime value associated with a stable signal ID. */
bool fn_IOService_GetValue(uint16_t ui16_SignalId, S_IO_SignalValue *pst_Value);
/** @brief Validate/map an AO process command into the signed Modbus register representation. */
E_IO_Status fn_IOService_ProcessToRegister(uint16_t ui16_SignalId, float f32_ProcessValue,
                                           int16_t *pi16_RegisterValue,
                                           float *pf32_AppliedProcessValue);
/** @brief Check whether a configured digital signal may be written. */
E_IO_Status fn_IOService_DigitalWriteAllowed(uint16_t ui16_SignalId);
/** @brief Convert electrical units to process/engineering units using configured mapping. */
float fn_IOService_ElectricalToProcess(const S_IO_SignalConfig *pst_Config, float f32_Electrical);
/** @brief Convert process/engineering units back to electrical units for AO operation. */
float fn_IOService_ProcessToElectrical(const S_IO_SignalConfig *pst_Config, float f32_Process);

#ifdef __cplusplus
}
#endif

#endif /* IO_SERVICE_H */
