/**
 * @file el3074.c
 * @brief EL3074 signed analog-input and status process-image decoder.
 * @author Prathamesh Chipkar
 */
#include "ead/terminals.h"

static int16_t fn_ReadLeI16(const uint8_t *data, size_t offset)
{
    uint16_t value = (uint16_t)data[offset] |
                     (uint16_t)((uint16_t)data[offset + 1U] << 8U);
    return (int16_t)value;
}

static uint16_t fn_ReadLeU16(const uint8_t *data, size_t offset)
{
    return (uint16_t)data[offset] |
           (uint16_t)((uint16_t)data[offset + 1U] << 8U);
}

static bool fn_IsBipolar(ead_analog_mode_t mode)
{
    return (mode == EAD_ANALOG_BIPOLAR_10V) ||
           (mode == EAD_ANALOG_BIPOLAR_20MA);
}

static ead_result_t fn_RawToEngineering(int16_t raw,
                                        const ead_terminal_config_t *config,
                                        double *engineering)
{
    double normalized;
    if ((config == NULL) || (engineering == NULL) || (config->full_scale_raw <= 0))
        return EAD_ERR_ARGUMENT;
    normalized = (double)raw / (double)config->full_scale_raw;
    if (fn_IsBipolar(config->analog_mode))
    {
        double midpoint = (config->engineering_min + config->engineering_max) / 2.0;
        double half_span = (config->engineering_max - config->engineering_min) / 2.0;
        *engineering = midpoint + (normalized * half_span);
    }
    else
    {
        *engineering = config->engineering_min +
                       (normalized * (config->engineering_max - config->engineering_min));
    }
    return EAD_OK;
}

ead_result_t fn_El3074Decode(const uint8_t *data,
                             size_t size,
                             size_t channel,
                             const ead_terminal_config_t *config,
                             ead_analog_sample_t *sample)
{
    size_t stride;
    size_t base;
    size_t value_position;
    if ((data == NULL) || (config == NULL) || (sample == NULL) ||
        (channel < 1U) || (channel > 4U)) return EAD_ERR_ARGUMENT;
    stride = config->channel_stride;
    if (stride == 0U)
        stride = (config->pdo_layout == EAD_PDO_COMPACT_INT16) ? 2U : 4U;
    base = (channel - 1U) * stride;
    value_position = base + config->value_offset;
    if ((value_position > size) || ((size - value_position) < 2U)) return EAD_ERR_RANGE;
    sample->raw = fn_ReadLeI16(data, value_position);
    sample->status = 0U;
    if (config->pdo_layout == EAD_PDO_STATUS_VALUE_INT16)
    {
        size_t status_position = base + config->status_offset;
        if ((status_position > size) || ((size - status_position) < 2U)) return EAD_ERR_RANGE;
        sample->status = fn_ReadLeU16(data, status_position);
    }
    return fn_RawToEngineering(sample->raw, config, &sample->engineering);
}
