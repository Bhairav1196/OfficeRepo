#ifndef IO_CONFIG_TYPES_H
#define IO_CONFIG_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @file    io_config_types.h
 * @brief   Strongly typed configuration model for Beckhoff units and process signals.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Supported application signal classes. */
typedef enum
{
    E_IO_SIGNAL_DI = 0,
    E_IO_SIGNAL_DO,
    E_IO_SIGNAL_AI,
    E_IO_SIGNAL_AO
} E_IO_SignalType;

/** @brief Register payload datatype after Modbus decoding. */
typedef enum
{
    E_IO_DATATYPE_BOOL = 0,
    E_IO_DATATYPE_INT16,
    E_IO_DATATYPE_UINT16,
    E_IO_DATATYPE_INT32,
    E_IO_DATATYPE_UINT32,
    E_IO_DATATYPE_FLOAT32
} E_IO_DataType;

/** @brief Electrical operating mode used by analog channels. */
typedef enum
{
    E_IO_ANALOG_NOT_APPLICABLE = 0,
    E_IO_ANALOG_CURRENT_BIPOLAR_20MA,
    E_IO_ANALOG_CURRENT_0_20MA,
    E_IO_ANALOG_CURRENT_4_20MA,
    E_IO_ANALOG_VOLTAGE_BIPOLAR_10V,
    E_IO_ANALOG_VOLTAGE_0_10V,
    E_IO_ANALOG_CURRENT_CUSTOM,
    E_IO_ANALOG_VOLTAGE_CUSTOM
} E_IO_AnalogMode;

/** @brief Conversion method between electrical and process engineering domains. */
typedef enum
{
    E_IO_MAPPING_NONE = 0,
    E_IO_MAPPING_LINEAR,
    E_IO_MAPPING_PIECEWISE_LINEAR
} E_IO_MappingType;

/** @brief Policy applied when an AO command exceeds its configured operating range. */
typedef enum
{
    E_IO_WRITE_REJECT = 0,
    E_IO_WRITE_CLAMP
} E_IO_WriteRangePolicy;

/** @brief Runtime quality associated with a process signal. */
typedef enum
{
    E_IO_QUALITY_GOOD = 0,
    E_IO_QUALITY_OUT_OF_RANGE,
    E_IO_QUALITY_COMM_ERROR,
    E_IO_QUALITY_DISABLED,
    E_IO_QUALITY_CONFIG_ERROR,
    E_IO_QUALITY_STALE
} E_IO_Quality;

/** @brief Configuration validation result. */
typedef enum
{
    E_IO_CONFIG_OK = 0,
    E_IO_CONFIG_NULL,
    E_IO_CONFIG_DUPLICATE_ID,
    E_IO_CONFIG_DUPLICATE_MAPPING,
    E_IO_CONFIG_DUPLICATE_UNIT,
    E_IO_CONFIG_INVALID_UNIT,
    E_IO_CONFIG_INVALID_ADDRESS,
    E_IO_CONFIG_INVALID_TYPE,
    E_IO_CONFIG_INVALID_MODE,
    E_IO_CONFIG_INVALID_RANGE,
    E_IO_CONFIG_INVALID_MAPPING,
    E_IO_CONFIG_INVALID_ACCESS,
    E_IO_CONFIG_INVALID_SCALE,
    E_IO_CONFIG_INVALID_HMI_SLOT,
    E_IO_CONFIG_UNIT_TOPOLOGY_MISMATCH,
    E_IO_CONFIG_CAPACITY_EXCEEDED
} E_IO_ConfigStatus;

/** @brief Process-I/O service return status. */
typedef enum
{
    E_IO_STATUS_OK = 0,
    E_IO_STATUS_INVALID_ARGUMENT,
    E_IO_STATUS_DISABLED,
    E_IO_STATUS_NOT_WRITABLE,
    E_IO_STATUS_OUT_OF_RANGE,
    E_IO_STATUS_CONFIG_ERROR,
    E_IO_STATUS_NOT_FOUND
} E_IO_Status;

/** @brief Inclusive floating-point range. */
typedef struct
{
    float f32_Min;
    float f32_Max;
} S_IO_Range;

/** @brief One calibration point for piecewise-linear conversion. */
typedef struct
{
    float f32_Electrical;
    float f32_Process;
} S_IO_CalibrationPoint;

/**
 * @brief One Beckhoff bus-coupler logical unit presented by the application.
 *
 * @details The topology table is the HMI and validation source of truth. The
 *          current project intentionally defines exactly two units:
 *          Unit 1 = 16 DI + 16 DO + 4 AI + 4 AO.
 */
typedef struct
{
    uint8_t     ui8_UnitId;
    const char *pc_Name;
    const char *pc_Description;
    bool        b_Enabled;
    uint8_t     ui8_DiCount;
    uint8_t     ui8_DoCount;
    uint8_t     ui8_AiCount;
    uint8_t     ui8_AoCount;
} S_IO_UnitConfig;

/** @brief Complete immutable configuration for one process signal. */
typedef struct
{
    uint16_t              ui16_SignalId;
    const char           *pc_Name;
    E_IO_SignalType       e_SignalType;
    bool                  b_Enabled;
    bool                  b_Writable;

    uint8_t               ui8_UnitId;
    uint8_t               ui8_ReadFunctionCode;
    uint16_t              ui16_RegisterAddress;
    uint8_t               ui8_BitIndex;
    E_IO_DataType         e_DataType;

    E_IO_AnalogMode       e_AnalogMode;
    float                 f32_RegisterToElectricalScale;
    float                 f32_RegisterToElectricalOffset;
    S_IO_Range            st_ElectricalHardwareRange;
    S_IO_Range            st_ElectricalOperatingRange;
    S_IO_Range            st_ElectricalMapRange;

    E_IO_MappingType      e_MappingType;
    S_IO_Range            st_ProcessMapRange;
    S_IO_Range            st_ProcessOperatingRange;
    const S_IO_CalibrationPoint *past_Calibration;
    uint8_t               ui8_CalibrationPointCount;

    const char           *pc_ProcessUnit;
    uint8_t               ui8_DecimalPlaces;
    float                 f32_HmiStep;
    E_IO_WriteRangePolicy e_WriteRangePolicy;

    bool                  b_HmiVisible;
    uint8_t               ui8_HmiSlot;
} S_IO_SignalConfig;

#ifdef __cplusplus
}
#endif

#endif /* IO_CONFIG_TYPES_H */
