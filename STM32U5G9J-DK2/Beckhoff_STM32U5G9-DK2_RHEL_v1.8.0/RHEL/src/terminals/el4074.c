/**
 * @file el4074.c
 * @brief EL4074 status decoding and little-endian REAL32 analog-output encoding.
 * @author Prathamesh Chipkar
 */
#include "ead/terminals.h"

#include <float.h>
#include <math.h>
#include <string.h>

_Static_assert(sizeof(float) == 4U, "EL4074 REAL32 support requires 32-bit float");
_Static_assert(FLT_RADIX == 2, "EL4074 REAL32 support requires a binary float representation");

static bool fn_IsBipolar(ead_analog_mode_t mode)
{
    return (mode == EAD_ANALOG_BIPOLAR_10V) ||
           (mode == EAD_ANALOG_BIPOLAR_20MA);
}

static double fn_Clamp(double value, double minimum, double maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static void fn_EngineeringLimits(const ead_terminal_config_t *config,
                                 double *minimum,
                                 double *maximum)
{
    const double extension = config->allow_extended_range ? 0.071 : 0.0;
    if (fn_IsBipolar(config->analog_mode))
    {
        const double midpoint = (config->engineering_min + config->engineering_max) / 2.0;
        const double half_span = (config->engineering_max - config->engineering_min) / 2.0;
        *minimum = midpoint - (half_span * (1.0 + extension));
        *maximum = midpoint + (half_span * (1.0 + extension));
    }
    else
    {
        const double span = config->engineering_max - config->engineering_min;
        *minimum = config->engineering_min;
        *maximum = config->engineering_max + (span * extension);
    }
}

static ead_result_t fn_ValuePosition(size_t size,
                                     size_t channel,
                                     const ead_terminal_config_t *config,
                                     size_t *position,
                                     size_t *width)
{
    size_t stride;
    if ((config == NULL) || (position == NULL) || (width == NULL) ||
        (channel < 1U) || (channel > 4U)) return EAD_ERR_ARGUMENT;

    *width = (config->pdo_layout == EAD_PDO_REAL32) ? 4U : 2U;
    stride = config->channel_stride;
    if (stride == 0U)
    {
        if (config->pdo_layout == EAD_PDO_COMPACT_INT16) stride = 2U;
        else stride = 4U;
    }

    *position = ((channel - 1U) * stride) + config->value_offset;
    if ((*position > size) || ((size - *position) < *width)) return EAD_ERR_RANGE;
    return EAD_OK;
}

static ead_result_t fn_StatusPosition(size_t size,
                                      size_t channel,
                                      const ead_terminal_config_t *config,
                                      size_t *position)
{
    size_t stride;
    if ((config == NULL) || (position == NULL) ||
        (channel < 1U) || (channel > 4U)) return EAD_ERR_ARGUMENT;
    stride = (config->status_stride != 0U) ? config->status_stride : 2U;
    *position = ((channel - 1U) * stride) + config->status_offset;
    if ((*position > size) || ((size - *position) < 2U)) return EAD_ERR_RANGE;
    return EAD_OK;
}

static ead_result_t fn_EngineeringToRaw(double value,
                                        const ead_terminal_config_t *config,
                                        int16_t *raw)
{
    double normalized;
    double limit;
    long rounded;
    if ((config == NULL) || (raw == NULL) || (config->full_scale_raw <= 0) ||
        (config->engineering_max <= config->engineering_min) || !isfinite(value))
        return EAD_ERR_ARGUMENT;
    limit = config->allow_extended_range ? 1.071 : 1.0;
    if (fn_IsBipolar(config->analog_mode))
    {
        const double midpoint = (config->engineering_min + config->engineering_max) / 2.0;
        const double half_span = (config->engineering_max - config->engineering_min) / 2.0;
        normalized = fn_Clamp((value - midpoint) / half_span, -limit, limit);
    }
    else
    {
        normalized = fn_Clamp((value - config->engineering_min) /
                              (config->engineering_max - config->engineering_min),
                              0.0, limit);
    }
    rounded = lround(normalized * (double)config->full_scale_raw);
    if (rounded < -32768L) rounded = -32768L;
    if (rounded > 32767L) rounded = 32767L;
    *raw = (int16_t)rounded;
    return EAD_OK;
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
        const double midpoint = (config->engineering_min + config->engineering_max) / 2.0;
        const double half_span = (config->engineering_max - config->engineering_min) / 2.0;
        *engineering = midpoint + (normalized * half_span);
    }
    else
    {
        *engineering = config->engineering_min +
                       (normalized * (config->engineering_max - config->engineering_min));
    }
    return EAD_OK;
}

static uint32_t fn_ReadLe32Local(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
}

static void fn_WriteLe32Local(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value & UINT32_C(0x000000FF));
    data[1] = (uint8_t)((value >> 8U) & UINT32_C(0x000000FF));
    data[2] = (uint8_t)((value >> 16U) & UINT32_C(0x000000FF));
    data[3] = (uint8_t)((value >> 24U) & UINT32_C(0x000000FF));
}

ead_result_t fn_El4074Decode(const uint8_t *data,
                             size_t size,
                             size_t channel,
                             const ead_terminal_config_t *config,
                             ead_analog_sample_t *sample)
{
    size_t position;
    size_t width;
    ead_result_t result;
    if ((data == NULL) || (sample == NULL)) return EAD_ERR_ARGUMENT;
    result = fn_ValuePosition(size, channel, config, &position, &width);
    if (result != EAD_OK) return result;

    sample->status = 0U;
    if (config->pdo_layout == EAD_PDO_REAL32)
    {
        uint32_t encoded = fn_ReadLe32Local(&data[position]);
        float value = 0.0F;
        (void)memcpy(&value, &encoded, sizeof(value));
        if (!isfinite(value)) return EAD_ERR_PROTOCOL;
        sample->raw = 0;
        sample->engineering = (double)value;
        return EAD_OK;
    }

    {
        uint16_t value = (uint16_t)data[position] |
                         (uint16_t)((uint16_t)data[position + 1U] << 8U);
        sample->raw = (int16_t)value;
    }
    return fn_RawToEngineering(sample->raw, config, &sample->engineering);
}

ead_result_t fn_El4074Encode(uint8_t *data,
                             size_t size,
                             size_t channel,
                             double value,
                             const ead_terminal_config_t *config)
{
    size_t position;
    size_t width;
    ead_result_t result;
    if ((data == NULL) || !isfinite(value)) return EAD_ERR_ARGUMENT;
    result = fn_ValuePosition(size, channel, config, &position, &width);
    if (result != EAD_OK) return result;

    if (config->pdo_layout == EAD_PDO_REAL32)
    {
        double minimum;
        double maximum;
        float encoded_value;
        uint32_t encoded = 0U;
        fn_EngineeringLimits(config, &minimum, &maximum);
        encoded_value = (float)fn_Clamp(value, minimum, maximum);
        (void)memcpy(&encoded, &encoded_value, sizeof(encoded));
        fn_WriteLe32Local(&data[position], encoded);
        return EAD_OK;
    }

    {
        int16_t raw;
        uint16_t encoded;
        result = fn_EngineeringToRaw(value, config, &raw);
        if (result != EAD_OK) return result;
        encoded = (uint16_t)raw;
        data[position] = (uint8_t)(encoded & UINT16_C(0x00FF));
        data[position + 1U] = (uint8_t)((encoded >> 8U) & UINT16_C(0x00FF));
    }
    return EAD_OK;
}

ead_result_t fn_El4074Status(const uint8_t *data,
                             size_t size,
                             size_t channel,
                             const ead_terminal_config_t *config,
                             uint16_t *status)
{
    size_t position;
    ead_result_t result;
    if ((data == NULL) || (status == NULL)) return EAD_ERR_ARGUMENT;
    result = fn_StatusPosition(size, channel, config, &position);
    if (result != EAD_OK) return result;
    *status = (uint16_t)data[position] |
              (uint16_t)((uint16_t)data[position + 1U] << 8U);
    return EAD_OK;
}
