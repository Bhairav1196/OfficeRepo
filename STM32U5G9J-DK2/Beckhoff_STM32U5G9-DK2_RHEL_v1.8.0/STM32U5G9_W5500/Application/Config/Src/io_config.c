/**
 * @file    io_config.c
 * @brief   Authoritative user-editable Beckhoff topology and I/O signal configuration.
 * @author  Prathamesh Chipkar
 * @date    11-Sep-2026
 * @version 2.0.0-alpha.9.0
 *
 * @details
 * This is the primary commissioning configuration file. The current hardware
 * topology is intentionally explicit:
 *   - Unit 1: 16 DI + 16 DO + 4 AI + 4 AO
 *
 * Digital names, analog electrical mode, register conversion, electrical
 * limits, optional process mapping, process limits, display precision and HMI
 * visibility are edited here. TouchGFX consumes this same configuration via
 * hmi_service.h; names and units shall not be duplicated in GUI code.
 *
 * Analog display rule:
 *   - E_IO_MAPPING_NONE: HMI displays the electrical value/unit derived from
 *     e_AnalogMode (mA or V).
 *   - LINEAR/PIECEWISE mapping: HMI displays the configured process value/unit.
 */

#include "io_config.h"
#include "io_signal_ids.h"
#include "app_config.h"

#include <stddef.h>

#define FC_READ_COILS             (0x01U)
#define FC_READ_DISCRETE_INPUTS   (0x02U)
#define FC_READ_HOLDING_REGS      (0x03U)
#define FC_READ_INPUT_REGS        (0x04U)

/** @brief Fixed single-coupler topology presented to the application and HMI. */
static const S_IO_UnitConfig gast_IO_Units[] =
{
    {
        .ui8_UnitId = 1U,
        .pc_Name = "BECKHOFF I/O",
        .pc_Description = "EK9500  |  16 DI  |  16 DO  |  4 AI  |  4 AO",
        .b_Enabled = true,
        .ui8_DiCount = 16U,
        .ui8_DoCount = 16U,
        .ui8_AiCount = 4U,
        .ui8_AoCount = 4U
    }
};

#define DIGITAL_CFG(id_, name_, type_, unit_, fc_, bit_, slot_, writable_) \
    { \
        .ui16_SignalId = (id_), .pc_Name = (name_), .e_SignalType = (type_), \
        .b_Enabled = true, .b_Writable = (writable_), .ui8_UnitId = (unit_), \
        .ui8_ReadFunctionCode = (fc_), .ui16_RegisterAddress = 0U, .ui8_BitIndex = (bit_), \
        .e_DataType = E_IO_DATATYPE_BOOL, .e_AnalogMode = E_IO_ANALOG_NOT_APPLICABLE, \
        .f32_RegisterToElectricalScale = 1.0F, .f32_RegisterToElectricalOffset = 0.0F, \
        .st_ElectricalHardwareRange = {0.0F, 1.0F}, .st_ElectricalOperatingRange = {0.0F, 1.0F}, \
        .st_ElectricalMapRange = {0.0F, 1.0F}, .e_MappingType = E_IO_MAPPING_NONE, \
        .st_ProcessMapRange = {0.0F, 1.0F}, .st_ProcessOperatingRange = {0.0F, 1.0F}, \
        .past_Calibration = NULL, .ui8_CalibrationPointCount = 0U, .pc_ProcessUnit = "", \
        .ui8_DecimalPlaces = 0U, .f32_HmiStep = 0.0F, .e_WriteRangePolicy = E_IO_WRITE_REJECT, \
        .b_HmiVisible = true, .ui8_HmiSlot = (slot_) \
    }

/*
 * OPTIONAL PROCESS-MAPPING EXAMPLE
 * --------------------------------
 * For a current-mode PORT FIN ANGLE where -20..+20 mA represents -35..+35 deg:
 *   .e_AnalogMode               = E_IO_ANALOG_CURRENT_BIPOLAR_20MA
 *   .f32_RegisterToElectricalScale = <scale appropriate to the Modbus register>
 *   .st_ElectricalHardwareRange = {-20.000F, +20.000F}
 *   .st_ElectricalOperatingRange= {-20.000F, +20.000F}
 *   .st_ElectricalMapRange      = {-20.000F, +20.000F}
 *   .e_MappingType              = E_IO_MAPPING_LINEAR
 *   .st_ProcessMapRange         = {-35.000F, +35.000F}
 *   .st_ProcessOperatingRange   = {-35.000F, +35.000F}
 *   .pc_ProcessUnit             = "deg"
 * TouchGFX will then display +17.500 deg for +10.000 mA.
 */

static const S_IO_SignalConfig gast_IO_Config[] =
{
    DIGITAL_CFG(IO_ID_U1_DI(1U), "UNIT 1 DI 01", E_IO_SIGNAL_DI, 1U, FC_READ_DISCRETE_INPUTS, 0U, 0U, false),
    DIGITAL_CFG(IO_ID_U1_DI(2U), "UNIT 1 DI 02", E_IO_SIGNAL_DI, 1U, FC_READ_DISCRETE_INPUTS, 1U, 1U, false),
    DIGITAL_CFG(IO_ID_U1_DI(3U), "UNIT 1 DI 03", E_IO_SIGNAL_DI, 1U, FC_READ_DISCRETE_INPUTS, 2U, 2U, false),
    DIGITAL_CFG(IO_ID_U1_DI(4U), "UNIT 1 DI 04", E_IO_SIGNAL_DI, 1U, FC_READ_DISCRETE_INPUTS, 3U, 3U, false),
    DIGITAL_CFG(IO_ID_U1_DI(5U), "UNIT 1 DI 05", E_IO_SIGNAL_DI, 1U, FC_READ_DISCRETE_INPUTS, 4U, 4U, false),
    DIGITAL_CFG(IO_ID_U1_DI(6U), "UNIT 1 DI 06", E_IO_SIGNAL_DI, 1U, FC_READ_DISCRETE_INPUTS, 5U, 5U, false),
    DIGITAL_CFG(IO_ID_U1_DI(7U), "UNIT 1 DI 07", E_IO_SIGNAL_DI, 1U, FC_READ_DISCRETE_INPUTS, 6U, 6U, false),
    DIGITAL_CFG(IO_ID_U1_DI(8U), "UNIT 1 DI 08", E_IO_SIGNAL_DI, 1U, FC_READ_DISCRETE_INPUTS, 7U, 7U, false),
    DIGITAL_CFG(IO_ID_U1_DI(9U), "UNIT 1 DI 09", E_IO_SIGNAL_DI, 1U, FC_READ_DISCRETE_INPUTS, 8U, 8U, false),
    DIGITAL_CFG(IO_ID_U1_DI(10U), "UNIT 1 DI 10", E_IO_SIGNAL_DI, 1U, FC_READ_DISCRETE_INPUTS, 9U, 9U, false),
    DIGITAL_CFG(IO_ID_U1_DI(11U), "UNIT 1 DI 11", E_IO_SIGNAL_DI, 1U, FC_READ_DISCRETE_INPUTS, 10U, 10U, false),
    DIGITAL_CFG(IO_ID_U1_DI(12U), "UNIT 1 DI 12", E_IO_SIGNAL_DI, 1U, FC_READ_DISCRETE_INPUTS, 11U, 11U, false),
    DIGITAL_CFG(IO_ID_U1_DI(13U), "UNIT 1 DI 13", E_IO_SIGNAL_DI, 1U, FC_READ_DISCRETE_INPUTS, 12U, 12U, false),
    DIGITAL_CFG(IO_ID_U1_DI(14U), "UNIT 1 DI 14", E_IO_SIGNAL_DI, 1U, FC_READ_DISCRETE_INPUTS, 13U, 13U, false),
    DIGITAL_CFG(IO_ID_U1_DI(15U), "UNIT 1 DI 15", E_IO_SIGNAL_DI, 1U, FC_READ_DISCRETE_INPUTS, 14U, 14U, false),
    DIGITAL_CFG(IO_ID_U1_DI(16U), "UNIT 1 DI 16", E_IO_SIGNAL_DI, 1U, FC_READ_DISCRETE_INPUTS, 15U, 15U, false),
    DIGITAL_CFG(IO_ID_U1_DO(1U), "UNIT 1 DO 01", E_IO_SIGNAL_DO, 1U, FC_READ_COILS, 0U, 0U, true),
    DIGITAL_CFG(IO_ID_U1_DO(2U), "UNIT 1 DO 02", E_IO_SIGNAL_DO, 1U, FC_READ_COILS, 1U, 1U, true),
    DIGITAL_CFG(IO_ID_U1_DO(3U), "UNIT 1 DO 03", E_IO_SIGNAL_DO, 1U, FC_READ_COILS, 2U, 2U, true),
    DIGITAL_CFG(IO_ID_U1_DO(4U), "UNIT 1 DO 04", E_IO_SIGNAL_DO, 1U, FC_READ_COILS, 3U, 3U, true),
    DIGITAL_CFG(IO_ID_U1_DO(5U), "UNIT 1 DO 05", E_IO_SIGNAL_DO, 1U, FC_READ_COILS, 4U, 4U, true),
    DIGITAL_CFG(IO_ID_U1_DO(6U), "UNIT 1 DO 06", E_IO_SIGNAL_DO, 1U, FC_READ_COILS, 5U, 5U, true),
    DIGITAL_CFG(IO_ID_U1_DO(7U), "UNIT 1 DO 07", E_IO_SIGNAL_DO, 1U, FC_READ_COILS, 6U, 6U, true),
    DIGITAL_CFG(IO_ID_U1_DO(8U), "UNIT 1 DO 08", E_IO_SIGNAL_DO, 1U, FC_READ_COILS, 7U, 7U, true),
    DIGITAL_CFG(IO_ID_U1_DO(9U), "UNIT 1 DO 09", E_IO_SIGNAL_DO, 1U, FC_READ_COILS, 8U, 8U, true),
    DIGITAL_CFG(IO_ID_U1_DO(10U), "UNIT 1 DO 10", E_IO_SIGNAL_DO, 1U, FC_READ_COILS, 9U, 9U, true),
    DIGITAL_CFG(IO_ID_U1_DO(11U), "UNIT 1 DO 11", E_IO_SIGNAL_DO, 1U, FC_READ_COILS, 10U, 10U, true),
    DIGITAL_CFG(IO_ID_U1_DO(12U), "UNIT 1 DO 12", E_IO_SIGNAL_DO, 1U, FC_READ_COILS, 11U, 11U, true),
    DIGITAL_CFG(IO_ID_U1_DO(13U), "UNIT 1 DO 13", E_IO_SIGNAL_DO, 1U, FC_READ_COILS, 12U, 12U, true),
    DIGITAL_CFG(IO_ID_U1_DO(14U), "UNIT 1 DO 14", E_IO_SIGNAL_DO, 1U, FC_READ_COILS, 13U, 13U, true),
    DIGITAL_CFG(IO_ID_U1_DO(15U), "UNIT 1 DO 15", E_IO_SIGNAL_DO, 1U, FC_READ_COILS, 14U, 14U, true),
    DIGITAL_CFG(IO_ID_U1_DO(16U), "UNIT 1 DO 16", E_IO_SIGNAL_DO, 1U, FC_READ_COILS, 15U, 15U, true),
    {
        .ui16_SignalId = IO_ID_U1_AI(1U),
        .pc_Name = "UNIT 1 AI 01",
        .e_SignalType = E_IO_SIGNAL_AI,
        .b_Enabled = true,
        .b_Writable = false,
        .ui8_UnitId = 1U,
        .ui8_ReadFunctionCode = FC_READ_INPUT_REGS,
        .ui16_RegisterAddress = 1024U,
        .ui8_BitIndex = 0U,
        .e_DataType = E_IO_DATATYPE_INT16,
        .e_AnalogMode = E_IO_ANALOG_VOLTAGE_BIPOLAR_10V,
        .f32_RegisterToElectricalScale = 0.001F,
        .f32_RegisterToElectricalOffset = 0.0F,
        .st_ElectricalHardwareRange = {-10.000F, +10.000F},
        .st_ElectricalOperatingRange = {-10.000F, +10.000F},
        .st_ElectricalMapRange = {-10.000F, +10.000F},
        .e_MappingType = E_IO_MAPPING_NONE,
        .st_ProcessMapRange = {-10.000F, +10.000F},
        .st_ProcessOperatingRange = {-10.000F, +10.000F},
        .past_Calibration = NULL,
        .ui8_CalibrationPointCount = 0U,
        .pc_ProcessUnit = "",
        .ui8_DecimalPlaces = 3U,
        .f32_HmiStep = 0.0F,
        .e_WriteRangePolicy = E_IO_WRITE_REJECT,
        .b_HmiVisible = true,
        .ui8_HmiSlot = 0U
    },
    {
        .ui16_SignalId = IO_ID_U1_AI(2U),
        .pc_Name = "UNIT 1 AI 02",
        .e_SignalType = E_IO_SIGNAL_AI,
        .b_Enabled = true,
        .b_Writable = false,
        .ui8_UnitId = 1U,
        .ui8_ReadFunctionCode = FC_READ_INPUT_REGS,
        .ui16_RegisterAddress = 1025U,
        .ui8_BitIndex = 0U,
        .e_DataType = E_IO_DATATYPE_INT16,
        .e_AnalogMode = E_IO_ANALOG_VOLTAGE_BIPOLAR_10V,
        .f32_RegisterToElectricalScale = 0.001F,
        .f32_RegisterToElectricalOffset = 0.0F,
        .st_ElectricalHardwareRange = {-10.000F, +10.000F},
        .st_ElectricalOperatingRange = {-10.000F, +10.000F},
        .st_ElectricalMapRange = {-10.000F, +10.000F},
        .e_MappingType = E_IO_MAPPING_NONE,
        .st_ProcessMapRange = {-10.000F, +10.000F},
        .st_ProcessOperatingRange = {-10.000F, +10.000F},
        .past_Calibration = NULL,
        .ui8_CalibrationPointCount = 0U,
        .pc_ProcessUnit = "",
        .ui8_DecimalPlaces = 3U,
        .f32_HmiStep = 0.0F,
        .e_WriteRangePolicy = E_IO_WRITE_REJECT,
        .b_HmiVisible = true,
        .ui8_HmiSlot = 1U
    },
    {
        .ui16_SignalId = IO_ID_U1_AI(3U),
        .pc_Name = "UNIT 1 AI 03",
        .e_SignalType = E_IO_SIGNAL_AI,
        .b_Enabled = true,
        .b_Writable = false,
        .ui8_UnitId = 1U,
        .ui8_ReadFunctionCode = FC_READ_INPUT_REGS,
        .ui16_RegisterAddress = 1026U,
        .ui8_BitIndex = 0U,
        .e_DataType = E_IO_DATATYPE_INT16,
        .e_AnalogMode = E_IO_ANALOG_VOLTAGE_BIPOLAR_10V,
        .f32_RegisterToElectricalScale = 0.001F,
        .f32_RegisterToElectricalOffset = 0.0F,
        .st_ElectricalHardwareRange = {-10.000F, +10.000F},
        .st_ElectricalOperatingRange = {-10.000F, +10.000F},
        .st_ElectricalMapRange = {-10.000F, +10.000F},
        .e_MappingType = E_IO_MAPPING_NONE,
        .st_ProcessMapRange = {-10.000F, +10.000F},
        .st_ProcessOperatingRange = {-10.000F, +10.000F},
        .past_Calibration = NULL,
        .ui8_CalibrationPointCount = 0U,
        .pc_ProcessUnit = "",
        .ui8_DecimalPlaces = 3U,
        .f32_HmiStep = 0.0F,
        .e_WriteRangePolicy = E_IO_WRITE_REJECT,
        .b_HmiVisible = true,
        .ui8_HmiSlot = 2U
    },
    {
        .ui16_SignalId = IO_ID_U1_AI(4U),
        .pc_Name = "UNIT 1 AI 04",
        .e_SignalType = E_IO_SIGNAL_AI,
        .b_Enabled = true,
        .b_Writable = false,
        .ui8_UnitId = 1U,
        .ui8_ReadFunctionCode = FC_READ_INPUT_REGS,
        .ui16_RegisterAddress = 1027U,
        .ui8_BitIndex = 0U,
        .e_DataType = E_IO_DATATYPE_INT16,
        .e_AnalogMode = E_IO_ANALOG_VOLTAGE_BIPOLAR_10V,
        .f32_RegisterToElectricalScale = 0.001F,
        .f32_RegisterToElectricalOffset = 0.0F,
        .st_ElectricalHardwareRange = {-10.000F, +10.000F},
        .st_ElectricalOperatingRange = {-10.000F, +10.000F},
        .st_ElectricalMapRange = {-10.000F, +10.000F},
        .e_MappingType = E_IO_MAPPING_NONE,
        .st_ProcessMapRange = {-10.000F, +10.000F},
        .st_ProcessOperatingRange = {-10.000F, +10.000F},
        .past_Calibration = NULL,
        .ui8_CalibrationPointCount = 0U,
        .pc_ProcessUnit = "",
        .ui8_DecimalPlaces = 3U,
        .f32_HmiStep = 0.0F,
        .e_WriteRangePolicy = E_IO_WRITE_REJECT,
        .b_HmiVisible = true,
        .ui8_HmiSlot = 3U
    },
    {
        .ui16_SignalId = IO_ID_U1_AO(1U),
        .pc_Name = "UNIT 1 AO 01",
        .e_SignalType = E_IO_SIGNAL_AO,
        .b_Enabled = true,
        .b_Writable = true,
        .ui8_UnitId = 1U,
        .ui8_ReadFunctionCode = FC_READ_HOLDING_REGS,
        .ui16_RegisterAddress = 0U,
        .ui8_BitIndex = 0U,
        .e_DataType = E_IO_DATATYPE_INT16,
        .e_AnalogMode = E_IO_ANALOG_CURRENT_BIPOLAR_20MA,
        .f32_RegisterToElectricalScale = 0.01F,
        .f32_RegisterToElectricalOffset = 0.0F,
        .st_ElectricalHardwareRange = {-20.000F, +20.000F},
        .st_ElectricalOperatingRange = {-20.000F, +20.000F},
        .st_ElectricalMapRange = {-20.000F, +20.000F},
        .e_MappingType = E_IO_MAPPING_NONE,
        .st_ProcessMapRange = {-20.000F, +20.000F},
        .st_ProcessOperatingRange = {-20.000F, +20.000F},
        .past_Calibration = NULL,
        .ui8_CalibrationPointCount = 0U,
        .pc_ProcessUnit = "",
        .ui8_DecimalPlaces = 3U,
        .f32_HmiStep = 0.100F,
        .e_WriteRangePolicy = E_IO_WRITE_REJECT,
        .b_HmiVisible = true,
        .ui8_HmiSlot = 0U
    },
    {
        .ui16_SignalId = IO_ID_U1_AO(2U),
        .pc_Name = "UNIT 1 AO 02",
        .e_SignalType = E_IO_SIGNAL_AO,
        .b_Enabled = true,
        .b_Writable = true,
        .ui8_UnitId = 1U,
        .ui8_ReadFunctionCode = FC_READ_HOLDING_REGS,
        .ui16_RegisterAddress = 1U,
        .ui8_BitIndex = 0U,
        .e_DataType = E_IO_DATATYPE_INT16,
        .e_AnalogMode = E_IO_ANALOG_CURRENT_BIPOLAR_20MA,
        .f32_RegisterToElectricalScale = 0.01F,
        .f32_RegisterToElectricalOffset = 0.0F,
        .st_ElectricalHardwareRange = {-20.000F, +20.000F},
        .st_ElectricalOperatingRange = {-20.000F, +20.000F},
        .st_ElectricalMapRange = {-20.000F, +20.000F},
        .e_MappingType = E_IO_MAPPING_NONE,
        .st_ProcessMapRange = {-20.000F, +20.000F},
        .st_ProcessOperatingRange = {-20.000F, +20.000F},
        .past_Calibration = NULL,
        .ui8_CalibrationPointCount = 0U,
        .pc_ProcessUnit = "",
        .ui8_DecimalPlaces = 3U,
        .f32_HmiStep = 0.100F,
        .e_WriteRangePolicy = E_IO_WRITE_REJECT,
        .b_HmiVisible = true,
        .ui8_HmiSlot = 1U
    },
    {
        .ui16_SignalId = IO_ID_U1_AO(3U),
        .pc_Name = "UNIT 1 AO 03",
        .e_SignalType = E_IO_SIGNAL_AO,
        .b_Enabled = true,
        .b_Writable = true,
        .ui8_UnitId = 1U,
        .ui8_ReadFunctionCode = FC_READ_HOLDING_REGS,
        .ui16_RegisterAddress = 2U,
        .ui8_BitIndex = 0U,
        .e_DataType = E_IO_DATATYPE_INT16,
        .e_AnalogMode = E_IO_ANALOG_CURRENT_BIPOLAR_20MA,
        .f32_RegisterToElectricalScale = 0.01F,
        .f32_RegisterToElectricalOffset = 0.0F,
        .st_ElectricalHardwareRange = {-20.000F, +20.000F},
        .st_ElectricalOperatingRange = {-20.000F, +20.000F},
        .st_ElectricalMapRange = {-20.000F, +20.000F},
        .e_MappingType = E_IO_MAPPING_NONE,
        .st_ProcessMapRange = {-20.000F, +20.000F},
        .st_ProcessOperatingRange = {-20.000F, +20.000F},
        .past_Calibration = NULL,
        .ui8_CalibrationPointCount = 0U,
        .pc_ProcessUnit = "",
        .ui8_DecimalPlaces = 3U,
        .f32_HmiStep = 0.100F,
        .e_WriteRangePolicy = E_IO_WRITE_REJECT,
        .b_HmiVisible = true,
        .ui8_HmiSlot = 2U
    },
    {
        .ui16_SignalId = IO_ID_U1_AO(4U),
        .pc_Name = "UNIT 1 AO 04",
        .e_SignalType = E_IO_SIGNAL_AO,
        .b_Enabled = true,
        .b_Writable = true,
        .ui8_UnitId = 1U,
        .ui8_ReadFunctionCode = FC_READ_HOLDING_REGS,
        .ui16_RegisterAddress = 3U,
        .ui8_BitIndex = 0U,
        .e_DataType = E_IO_DATATYPE_INT16,
        .e_AnalogMode = E_IO_ANALOG_CURRENT_BIPOLAR_20MA,
        .f32_RegisterToElectricalScale = 0.01F,
        .f32_RegisterToElectricalOffset = 0.0F,
        .st_ElectricalHardwareRange = {-20.000F, +20.000F},
        .st_ElectricalOperatingRange = {-20.000F, +20.000F},
        .st_ElectricalMapRange = {-20.000F, +20.000F},
        .e_MappingType = E_IO_MAPPING_NONE,
        .st_ProcessMapRange = {-20.000F, +20.000F},
        .st_ProcessOperatingRange = {-20.000F, +20.000F},
        .past_Calibration = NULL,
        .ui8_CalibrationPointCount = 0U,
        .pc_ProcessUnit = "",
        .ui8_DecimalPlaces = 3U,
        .f32_HmiStep = 0.100F,
        .e_WriteRangePolicy = E_IO_WRITE_REJECT,
        .b_HmiVisible = true,
        .ui8_HmiSlot = 3U
    },
};

_Static_assert((sizeof(gast_IO_Units) / sizeof(gast_IO_Units[0])) == APP_CFG_BECKHOFF_UNIT_COUNT,
               "Configured Beckhoff unit count mismatch");
_Static_assert((sizeof(gast_IO_Config) / sizeof(gast_IO_Config[0])) <= APP_CFG_MAX_SIGNAL_COUNT,
               "Configured signal count exceeds APP_CFG_MAX_SIGNAL_COUNT");

const S_IO_UnitConfig *fn_IOConfig_GetUnitTable(size_t *p_sz_Count)
{
    if (p_sz_Count != NULL)
    {
        *p_sz_Count = sizeof(gast_IO_Units) / sizeof(gast_IO_Units[0]);
    }
    return gast_IO_Units;
}

const S_IO_UnitConfig *fn_IOConfig_FindUnit(uint8_t ui8_UnitId)
{
    size_t sz_Index;
    const size_t sz_Count = sizeof(gast_IO_Units) / sizeof(gast_IO_Units[0]);
    for (sz_Index = 0U; sz_Index < sz_Count; ++sz_Index)
    {
        if (gast_IO_Units[sz_Index].ui8_UnitId == ui8_UnitId)
        {
            return &gast_IO_Units[sz_Index];
        }
    }
    return NULL;
}

const S_IO_SignalConfig *fn_IOConfig_GetTable(size_t *p_sz_Count)
{
    if (p_sz_Count != NULL)
    {
        *p_sz_Count = sizeof(gast_IO_Config) / sizeof(gast_IO_Config[0]);
    }
    return gast_IO_Config;
}

const S_IO_SignalConfig *fn_IOConfig_FindById(uint16_t ui16_SignalId)
{
    size_t sz_Index;
    const size_t sz_Count = sizeof(gast_IO_Config) / sizeof(gast_IO_Config[0]);
    for (sz_Index = 0U; sz_Index < sz_Count; ++sz_Index)
    {
        if (gast_IO_Config[sz_Index].ui16_SignalId == ui16_SignalId)
        {
            return &gast_IO_Config[sz_Index];
        }
    }
    return NULL;
}

const S_IO_SignalConfig *fn_IOConfig_FindUnitHmiSlot(uint8_t ui8_UnitId,
                                                     E_IO_SignalType e_Type,
                                                     uint8_t ui8_Slot)
{
    size_t sz_Index;
    const size_t sz_Count = sizeof(gast_IO_Config) / sizeof(gast_IO_Config[0]);
    for (sz_Index = 0U; sz_Index < sz_Count; ++sz_Index)
    {
        const S_IO_SignalConfig *pst_Config = &gast_IO_Config[sz_Index];
        if (pst_Config->b_HmiVisible &&
            (pst_Config->ui8_UnitId == ui8_UnitId) &&
            (pst_Config->e_SignalType == e_Type) &&
            (pst_Config->ui8_HmiSlot == ui8_Slot))
        {
            return pst_Config;
        }
    }
    return NULL;
}

const char *fn_IOConfig_GetElectricalUnit(E_IO_AnalogMode e_Mode)
{
    switch (e_Mode)
    {
        case E_IO_ANALOG_CURRENT_BIPOLAR_20MA:
        case E_IO_ANALOG_CURRENT_0_20MA:
        case E_IO_ANALOG_CURRENT_4_20MA:
        case E_IO_ANALOG_CURRENT_CUSTOM:
            return "mA";
        case E_IO_ANALOG_VOLTAGE_BIPOLAR_10V:
        case E_IO_ANALOG_VOLTAGE_0_10V:
        case E_IO_ANALOG_VOLTAGE_CUSTOM:
            return "V";
        default:
            return "";
    }
}

S_IO_Range fn_IOConfig_GetModeHardwareRange(E_IO_AnalogMode e_Mode)
{
    S_IO_Range st_Range = {0.0F, 0.0F};
    switch (e_Mode)
    {
        case E_IO_ANALOG_CURRENT_BIPOLAR_20MA: st_Range.f32_Min = -20.0F; st_Range.f32_Max = 20.0F; break;
        case E_IO_ANALOG_CURRENT_0_20MA:        st_Range.f32_Min =   0.0F; st_Range.f32_Max = 20.0F; break;
        case E_IO_ANALOG_CURRENT_4_20MA:        st_Range.f32_Min =   4.0F; st_Range.f32_Max = 20.0F; break;
        case E_IO_ANALOG_VOLTAGE_BIPOLAR_10V:  st_Range.f32_Min = -10.0F; st_Range.f32_Max = 10.0F; break;
        case E_IO_ANALOG_VOLTAGE_0_10V:        st_Range.f32_Min =   0.0F; st_Range.f32_Max = 10.0F; break;
        default: break;
    }
    return st_Range;
}

const char *fn_IOConfig_StatusString(E_IO_ConfigStatus e_Status)
{
    switch (e_Status)
    {
        case E_IO_CONFIG_OK: return "OK";
        case E_IO_CONFIG_NULL: return "NULL";
        case E_IO_CONFIG_DUPLICATE_ID: return "DUPLICATE_ID";
        case E_IO_CONFIG_DUPLICATE_MAPPING: return "DUPLICATE_MAPPING";
        case E_IO_CONFIG_DUPLICATE_UNIT: return "DUPLICATE_UNIT";
        case E_IO_CONFIG_INVALID_UNIT: return "INVALID_UNIT";
        case E_IO_CONFIG_INVALID_ADDRESS: return "INVALID_ADDRESS";
        case E_IO_CONFIG_INVALID_TYPE: return "INVALID_TYPE";
        case E_IO_CONFIG_INVALID_MODE: return "INVALID_MODE";
        case E_IO_CONFIG_INVALID_RANGE: return "INVALID_RANGE";
        case E_IO_CONFIG_INVALID_MAPPING: return "INVALID_MAPPING";
        case E_IO_CONFIG_INVALID_ACCESS: return "INVALID_ACCESS";
        case E_IO_CONFIG_INVALID_SCALE: return "INVALID_SCALE";
        case E_IO_CONFIG_INVALID_HMI_SLOT: return "INVALID_HMI_SLOT";
        case E_IO_CONFIG_UNIT_TOPOLOGY_MISMATCH: return "UNIT_TOPOLOGY_MISMATCH";
        case E_IO_CONFIG_CAPACITY_EXCEEDED: return "CAPACITY_EXCEEDED";
        default: return "UNKNOWN";
    }
}
