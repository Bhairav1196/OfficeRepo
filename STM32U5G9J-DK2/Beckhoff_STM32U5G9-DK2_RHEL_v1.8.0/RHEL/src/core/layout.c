/**
 * @file layout.c
 * @brief Terminal-chain process-image layout builder and logical address resolver.
 * @author Prathamesh Chipkar
 */
#include "ead/layout.h"
#include "ead/compiler.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const ead_terminal_descriptor_t st_Descriptors[] = {
    {EAD_TERMINAL_EL1409, EAD_DOMAIN_DI, "EL1409", 16U, 2U, 0U, EAD_PDO_BIT_PACKED},
    {EAD_TERMINAL_EL2409, EAD_DOMAIN_DO, "EL2409", 16U, 0U, 2U, EAD_PDO_BIT_PACKED},
    {EAD_TERMINAL_EL3074, EAD_DOMAIN_AI, "EL3074", 4U, 16U, 0U, EAD_PDO_STATUS_VALUE_INT16},
    /* The commissioned EL4074 mapping exposes one 16-bit status word and one
     * REAL32 output value per channel. */
    {EAD_TERMINAL_EL4074, EAD_DOMAIN_AO, "EL4074", 4U, 8U, 16U, EAD_PDO_REAL32},
    {EAD_TERMINAL_RAW, EAD_DOMAIN_RAW, "RAW", 0U, 0U, 0U, EAD_PDO_RAW}
};

static void fn_SetError(char *error, size_t error_size, const char *format, ...) EAD_PRINTF_FORMAT(3, 4);

static void fn_SetError(char *error, size_t error_size, const char *format, ...)
{
    va_list arguments;
    if ((error == NULL) || (error_size == 0U)) return;
    va_start(arguments, format);
    (void)vsnprintf(error, error_size, format, arguments);
    va_end(arguments);
}

const ead_terminal_descriptor_t *fn_EadTerminalDescriptor(ead_terminal_kind_t kind)
{
    size_t index;
    for (index = 0U; index < (sizeof(st_Descriptors) / sizeof(st_Descriptors[0])); ++index)
    {
        if (st_Descriptors[index].kind == kind) return &st_Descriptors[index];
    }
    return NULL;
}

static int fn_ComparePosition(const void *first, const void *second)
{
    const ead_terminal_config_t *left = (const ead_terminal_config_t *)first;
    const ead_terminal_config_t *right = (const ead_terminal_config_t *)second;
    if (left->position < right->position) return -1;
    if (left->position > right->position) return 1;
    return strcmp(left->name, right->name);
}

static ead_result_t fn_InputBytes(const ead_terminal_config_t *terminal,
                                  const ead_terminal_descriptor_t *descriptor,
                                  size_t *bytes,
                                  char *error,
                                  size_t error_size)
{
    if (terminal->input_bytes_override != 0U)
    {
        *bytes = terminal->input_bytes_override;
        return EAD_OK;
    }
    if (terminal->kind == EAD_TERMINAL_EL3074)
    {
        if (terminal->pdo_layout == EAD_PDO_COMPACT_INT16) *bytes = 8U;
        else if (terminal->pdo_layout == EAD_PDO_STATUS_VALUE_INT16) *bytes = 16U;
        else
        {
            fn_SetError(error, error_size, "%s: unsupported EL3074 PDO layout", terminal->name);
            return EAD_ERR_CONFIG;
        }
        return EAD_OK;
    }
    *bytes = descriptor->default_input_bytes;
    return EAD_OK;
}

static ead_result_t fn_OutputBytes(const ead_terminal_config_t *terminal,
                                   const ead_terminal_descriptor_t *descriptor,
                                   size_t *bytes,
                                   char *error,
                                   size_t error_size)
{
    if (terminal->output_bytes_override != 0U)
    {
        *bytes = terminal->output_bytes_override;
        return EAD_OK;
    }
    if (terminal->kind == EAD_TERMINAL_EL4074)
    {
        if (terminal->pdo_layout == EAD_PDO_COMPACT_INT16) *bytes = 8U;
        else if ((terminal->pdo_layout == EAD_PDO_CONTROL_VALUE_INT16) ||
                 (terminal->pdo_layout == EAD_PDO_REAL32)) *bytes = 16U;
        else
        {
            fn_SetError(error, error_size, "%s: unsupported EL4074 PDO layout", terminal->name);
            return EAD_ERR_CONFIG;
        }
        return EAD_OK;
    }
    *bytes = descriptor->default_output_bytes;
    return EAD_OK;
}

static ead_result_t fn_ValidateAnalog(const ead_terminal_address_t *terminal,
                                      char *error,
                                      size_t error_size)
{
    size_t stride;
    size_t width;
    size_t required;
    if (terminal->config.kind == EAD_TERMINAL_EL3074)
    {
        stride = terminal->config.channel_stride;
        if (stride == 0U)
        {
            stride = (terminal->config.pdo_layout == EAD_PDO_COMPACT_INT16) ? 2U : 4U;
        }
        required = ((terminal->descriptor.channels - 1U) * stride) +
                   terminal->config.value_offset + 2U;
        if (required > terminal->input_bytes)
        {
            fn_SetError(error, error_size, "%s: analog input value mapping requires %zu bytes, configured %zu",
                        terminal->config.name, required, terminal->input_bytes);
            return EAD_ERR_CONFIG;
        }
        if (terminal->config.pdo_layout == EAD_PDO_STATUS_VALUE_INT16)
        {
            size_t status_stride = terminal->config.status_stride;
            if (status_stride == 0U) status_stride = stride;
            required = ((terminal->descriptor.channels - 1U) * status_stride) +
                       terminal->config.status_offset + 2U;
            if (required > terminal->input_bytes)
            {
                fn_SetError(error, error_size, "%s: analog input status mapping exceeds input bytes",
                            terminal->config.name);
                return EAD_ERR_CONFIG;
            }
        }
    }
    else if (terminal->config.kind == EAD_TERMINAL_EL4074)
    {
        stride = terminal->config.channel_stride;
        width = (terminal->config.pdo_layout == EAD_PDO_REAL32) ? 4U : 2U;
        if (stride == 0U)
        {
            stride = (terminal->config.pdo_layout == EAD_PDO_COMPACT_INT16) ? 2U : 4U;
        }
        required = ((terminal->descriptor.channels - 1U) * stride) +
                   terminal->config.value_offset + width;
        if (required > terminal->output_bytes)
        {
            fn_SetError(error, error_size, "%s: analog output value mapping requires %zu bytes, configured %zu",
                        terminal->config.name, required, terminal->output_bytes);
            return EAD_ERR_CONFIG;
        }
        if (terminal->input_bytes >= (terminal->descriptor.channels * 2U))
        {
            size_t status_stride = terminal->config.status_stride;
            if (status_stride == 0U) status_stride = 2U;
            required = ((terminal->descriptor.channels - 1U) * status_stride) +
                       terminal->config.status_offset + 2U;
            if (required > terminal->input_bytes)
            {
                fn_SetError(error, error_size, "%s: analog output status mapping requires %zu bytes, configured %zu",
                            terminal->config.name, required, terminal->input_bytes);
                return EAD_ERR_CONFIG;
            }
        }
    }
    return EAD_OK;
}

static ead_result_t fn_ValidateOverlaps(const ead_layout_t *layout,
                                        bool input,
                                        char *error,
                                        size_t error_size)
{
    size_t first;
    size_t second;
    for (first = 0U; first < layout->terminal_count; ++first)
    {
        size_t first_offset = input ? layout->terminals[first].input_offset : layout->terminals[first].output_offset;
        size_t first_size = input ? layout->terminals[first].input_bytes : layout->terminals[first].output_bytes;
        if (first_size == 0U) continue;
        for (second = first + 1U; second < layout->terminal_count; ++second)
        {
            size_t second_offset = input ? layout->terminals[second].input_offset : layout->terminals[second].output_offset;
            size_t second_size = input ? layout->terminals[second].input_bytes : layout->terminals[second].output_bytes;
            if (second_size == 0U) continue;
            if ((first_offset < (second_offset + second_size)) &&
                (second_offset < (first_offset + first_size)))
            {
                fn_SetError(error, error_size, "%s process-image overlap: %s and %s",
                            input ? "input" : "output",
                            layout->terminals[first].config.name,
                            layout->terminals[second].config.name);
                return EAD_ERR_CONFIG;
            }
        }
    }
    return EAD_OK;
}

static ead_result_t fn_ValidateLogicalRanges(const ead_layout_t *layout,
                                             char *error,
                                             size_t error_size)
{
    size_t first;
    size_t second;
    for (first = 0U; first < layout->terminal_count; ++first)
    {
        size_t first_begin = layout->terminals[first].config.logical_base;
        size_t first_end = first_begin + layout->terminals[first].descriptor.channels;
        for (second = first + 1U; second < layout->terminal_count; ++second)
        {
            size_t second_begin;
            size_t second_end;
            if (layout->terminals[first].descriptor.domain !=
                layout->terminals[second].descriptor.domain) continue;
            second_begin = layout->terminals[second].config.logical_base;
            second_end = second_begin + layout->terminals[second].descriptor.channels;
            if ((first_begin < second_end) && (second_begin < first_end))
            {
                fn_SetError(error, error_size, "logical %s overlap: %s [%zu,%zu] and %s [%zu,%zu]",
                            fn_EadDomainText(layout->terminals[first].descriptor.domain),
                            layout->terminals[first].config.name, first_begin, first_end - 1U,
                            layout->terminals[second].config.name, second_begin, second_end - 1U);
                return EAD_ERR_CONFIG;
            }
        }
    }
    return EAD_OK;
}

ead_result_t fn_EadBuildLayout(const ead_system_config_t *config,
                               ead_layout_t *layout,
                               char *error,
                               size_t error_size)
{
    ead_terminal_config_t sorted[EAD_MAX_TERMINALS];
    size_t sorted_count = 0U;
    size_t index;
    size_t next_input;
    size_t next_output;
    ead_result_t result;

    if ((config == NULL) || (layout == NULL)) return EAD_ERR_ARGUMENT;
    (void)memset(layout, 0, sizeof(*layout));
    if ((error != NULL) && (error_size > 0U)) error[0] = '\0';

    for (index = 0U; index < config->terminal_count; ++index)
    {
        size_t other;
        if (!config->terminals[index].enabled) continue;
        for (other = 0U; other < sorted_count; ++other)
        {
            if (config->terminals[index].position == sorted[other].position)
            {
                fn_SetError(error, error_size, "duplicate terminal position %zu: %s and %s",
                            config->terminals[index].position,
                            config->terminals[index].name, sorted[other].name);
                return EAD_ERR_CONFIG;
            }
            if (strcmp(config->terminals[index].name, sorted[other].name) == 0)
            {
                fn_SetError(error, error_size, "duplicate terminal name %s", config->terminals[index].name);
                return EAD_ERR_CONFIG;
            }
        }
        sorted[sorted_count++] = config->terminals[index];
    }
    if (sorted_count == 0U)
    {
        fn_SetError(error, error_size, "no enabled terminals");
        return EAD_ERR_CONFIG;
    }
    qsort(sorted, sorted_count, sizeof(sorted[0]), fn_ComparePosition);

    next_input = config->scanner.input_header_bytes;
    next_output = config->scanner.output_header_bytes;

    for (index = 0U; index < sorted_count; ++index)
    {
        ead_terminal_address_t *address = &layout->terminals[layout->terminal_count];
        const ead_terminal_descriptor_t *descriptor = fn_EadTerminalDescriptor(sorted[index].kind);
        size_t end;
        if (descriptor == NULL)
        {
            fn_SetError(error, error_size, "%s: unsupported terminal type", sorted[index].name);
            return EAD_ERR_CONFIG;
        }
        (void)memset(address, 0, sizeof(*address));
        address->config = sorted[index];
        address->descriptor = *descriptor;
        result = fn_InputBytes(&address->config, descriptor, &address->input_bytes, error, error_size);
        if (result != EAD_OK) return result;
        result = fn_OutputBytes(&address->config, descriptor, &address->output_bytes, error, error_size);
        if (result != EAD_OK) return result;

        if (address->input_bytes > 0U)
        {
            address->input_offset = (address->config.input_offset_mode == EAD_OFFSET_EXPLICIT) ?
                                    address->config.input_offset : next_input;
            if (address->input_offset < config->scanner.input_header_bytes)
            {
                fn_SetError(error, error_size, "%s: input offset %zu overlaps the %zu-byte EK9500 header",
                            address->config.name, address->input_offset,
                            config->scanner.input_header_bytes);
                return EAD_ERR_CONFIG;
            }
            if (address->input_offset > (SIZE_MAX - address->input_bytes)) return EAD_ERR_RANGE;
            end = address->input_offset + address->input_bytes;
            if (end > next_input) next_input = end;
        }
        if (address->output_bytes > 0U)
        {
            address->output_offset = (address->config.output_offset_mode == EAD_OFFSET_EXPLICIT) ?
                                     address->config.output_offset : next_output;
            if (address->output_offset < config->scanner.output_header_bytes)
            {
                fn_SetError(error, error_size, "%s: output offset %zu overlaps the %zu-byte EK9500 header",
                            address->config.name, address->output_offset,
                            config->scanner.output_header_bytes);
                return EAD_ERR_CONFIG;
            }
            if (address->output_offset > (SIZE_MAX - address->output_bytes)) return EAD_ERR_RANGE;
            end = address->output_offset + address->output_bytes;
            if (end > next_output) next_output = end;
        }
        result = fn_ValidateAnalog(address, error, error_size);
        if (result != EAD_OK) return result;
        ++layout->terminal_count;
    }

    layout->mapped_input_end = next_input;
    layout->mapped_output_end = next_output;
    if ((next_input > (SIZE_MAX - config->scanner.input_tail_padding_bytes)) ||
        (next_output > (SIZE_MAX - config->scanner.output_tail_padding_bytes)))
    {
        return EAD_ERR_RANGE;
    }
    layout->input_size = next_input + config->scanner.input_tail_padding_bytes;
    layout->output_size = next_output + config->scanner.output_tail_padding_bytes;

    result = fn_ValidateOverlaps(layout, true, error, error_size);
    if (result != EAD_OK) return result;
    result = fn_ValidateOverlaps(layout, false, error, error_size);
    if (result != EAD_OK) return result;
    result = fn_ValidateLogicalRanges(layout, error, error_size);
    if (result != EAD_OK) return result;

    if (config->scanner.input_assembly_size != 0U)
    {
        if (config->scanner.strict_assembly_size &&
            (config->scanner.input_assembly_size != layout->input_size))
        {
            fn_SetError(error, error_size,
                        "configured input assembly size (%zu) does not exactly match layout (%zu: mapped=%zu + tail padding=%zu)",
                        config->scanner.input_assembly_size, layout->input_size,
                        layout->mapped_input_end, config->scanner.input_tail_padding_bytes);
            return EAD_ERR_CONFIG;
        }
        if (config->scanner.input_assembly_size < layout->mapped_input_end)
        {
            fn_SetError(error, error_size, "configured input assembly size %zu is smaller than mapped end %zu",
                        config->scanner.input_assembly_size, layout->mapped_input_end);
            return EAD_ERR_CONFIG;
        }
        layout->input_size = config->scanner.input_assembly_size;
    }
    if (config->scanner.output_assembly_size != 0U)
    {
        if (config->scanner.strict_assembly_size &&
            (config->scanner.output_assembly_size != layout->output_size))
        {
            fn_SetError(error, error_size,
                        "configured output assembly size (%zu) does not exactly match layout (%zu: mapped=%zu + tail padding=%zu)",
                        config->scanner.output_assembly_size, layout->output_size,
                        layout->mapped_output_end, config->scanner.output_tail_padding_bytes);
            return EAD_ERR_CONFIG;
        }
        if (config->scanner.output_assembly_size < layout->mapped_output_end)
        {
            fn_SetError(error, error_size, "configured output assembly size %zu is smaller than mapped end %zu",
                        config->scanner.output_assembly_size, layout->mapped_output_end);
            return EAD_ERR_CONFIG;
        }
        layout->output_size = config->scanner.output_assembly_size;
    }

    if ((layout->input_size > EAD_MAX_ASSEMBLY_SIZE) ||
        (layout->output_size > EAD_MAX_ASSEMBLY_SIZE))
    {
        fn_SetError(error, error_size, "calculated assembly size exceeds EAD_MAX_ASSEMBLY_SIZE");
        return EAD_ERR_RANGE;
    }
    return EAD_OK;
}

const ead_terminal_address_t *fn_EadFindTerminal(const ead_layout_t *layout,
                                                  const char *name)
{
    size_t index;
    if ((layout == NULL) || (name == NULL)) return NULL;
    for (index = 0U; index < layout->terminal_count; ++index)
    {
        if (strcmp(layout->terminals[index].config.name, name) == 0)
        {
            return &layout->terminals[index];
        }
    }
    return NULL;
}

ead_result_t fn_EadResolveLogical(const ead_layout_t *layout,
                                  ead_address_domain_t domain,
                                  size_t logical_channel,
                                  const ead_terminal_address_t **terminal,
                                  size_t *local_channel)
{
    size_t index;
    if ((layout == NULL) || (terminal == NULL) || (local_channel == NULL) ||
        (logical_channel == 0U)) return EAD_ERR_ARGUMENT;
    for (index = 0U; index < layout->terminal_count; ++index)
    {
        const ead_terminal_address_t *candidate = &layout->terminals[index];
        size_t begin;
        size_t end;
        if (candidate->descriptor.domain != domain) continue;
        begin = candidate->config.logical_base;
        end = begin + candidate->descriptor.channels;
        if ((logical_channel >= begin) && (logical_channel < end))
        {
            *terminal = candidate;
            *local_channel = logical_channel - begin + 1U;
            return EAD_OK;
        }
    }
    return EAD_ERR_NOT_FOUND;
}

static ead_result_t fn_Append(char *buffer, size_t capacity, size_t *used,
                              const char *format, ...) EAD_PRINTF_FORMAT(4, 5);

static ead_result_t fn_Append(char *buffer, size_t capacity, size_t *used,
                              const char *format, ...)
{
    va_list arguments;
    int written;
    if ((*used >= capacity) || (buffer == NULL)) return EAD_ERR_RANGE;
    va_start(arguments, format);
    written = vsnprintf(buffer + *used, capacity - *used, format, arguments);
    va_end(arguments);
    if ((written < 0) || ((size_t)written >= (capacity - *used))) return EAD_ERR_RANGE;
    *used += (size_t)written;
    return EAD_OK;
}

ead_result_t fn_EadFormatLayout(const ead_layout_t *layout,
                                char *buffer,
                                size_t capacity)
{
    size_t index;
    size_t used = 0U;
    ead_result_t result;
    if ((layout == NULL) || (buffer == NULL) || (capacity == 0U)) return EAD_ERR_ARGUMENT;
    buffer[0] = '\0';
    result = fn_Append(buffer, capacity, &used,
                       "input_size=%zu mapped_input_end=%zu output_size=%zu mapped_output_end=%zu\n",
                       layout->input_size, layout->mapped_input_end,
                       layout->output_size, layout->mapped_output_end);
    if (result != EAD_OK) return result;
    for (index = 0U; index < layout->terminal_count; ++index)
    {
        const ead_terminal_address_t *item = &layout->terminals[index];
        result = fn_Append(buffer, capacity, &used,
                           "pos=%zu name=%s type=%s domain=%s logical=[%zu,%zu] in=",
                           item->config.position, item->config.name,
                           item->descriptor.model, fn_EadDomainText(item->descriptor.domain),
                           item->config.logical_base,
                           item->config.logical_base + item->descriptor.channels - 1U);
        if (result != EAD_OK) return result;
        if (item->input_bytes == 0U) result = fn_Append(buffer, capacity, &used, "[-]");
        else result = fn_Append(buffer, capacity, &used, "[%zu,%zu]", item->input_offset, item->input_bytes);
        if (result != EAD_OK) return result;
        if (item->output_bytes == 0U) result = fn_Append(buffer, capacity, &used, " out=[-]\n");
        else result = fn_Append(buffer, capacity, &used, " out=[%zu,%zu]\n", item->output_offset, item->output_bytes);
        if (result != EAD_OK) return result;
    }
    return EAD_OK;
}
