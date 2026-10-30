/**
 * @file terminal_manager.c
 * @brief Typed digital and analog terminal-manager access implementation.
 * @author Prathamesh Chipkar
 */
#include "ead/terminal_manager.h"
#include "ead/terminals.h"

#include <string.h>

void fn_EadTerminalManagerInit(ead_terminal_manager_t *manager,
                               const ead_layout_t *layout,
                               ead_process_image_t *image)
{
    if (manager == NULL) return;
    manager->layout = layout;
    manager->image = image;
}

static ead_result_t fn_GetTerminal(const ead_terminal_manager_t *manager,
                                   const char *name,
                                   const ead_terminal_address_t **terminal)
{
    if ((manager == NULL) || (manager->layout == NULL) ||
        (manager->image == NULL) || (name == NULL) || (terminal == NULL))
        return EAD_ERR_ARGUMENT;
    *terminal = fn_EadFindTerminal(manager->layout, name);
    return (*terminal != NULL) ? EAD_OK : EAD_ERR_NOT_FOUND;
}

ead_result_t fn_EadGetDigital(const ead_terminal_manager_t *manager,
                              const char *name,
                              size_t channel,
                              bool *value)
{
    const ead_terminal_address_t *terminal;
    uint8_t image[EAD_MAX_ASSEMBLY_SIZE];
    size_t size = 0U;
    ead_result_t result = fn_GetTerminal(manager, name, &terminal);
    if (result != EAD_OK) return result;
    if ((terminal->descriptor.domain != EAD_DOMAIN_DI) &&
        (terminal->descriptor.domain != EAD_DOMAIN_DO)) return EAD_ERR_ARGUMENT;
    if (terminal->descriptor.domain == EAD_DOMAIN_DI)
    {
        result = fn_EadProcessImageSnapshotInput(manager->image, image, sizeof(image), &size);
        if (result != EAD_OK) return result;
        if ((terminal->input_offset > size) || (terminal->input_bytes > (size - terminal->input_offset))) return EAD_ERR_RANGE;
        return fn_El1409Get(&image[terminal->input_offset], terminal->input_bytes, channel, value);
    }
    result = fn_EadProcessImageSnapshotOutput(manager->image, image, sizeof(image), &size);
    if (result != EAD_OK) return result;
    if ((terminal->output_offset > size) || (terminal->output_bytes > (size - terminal->output_offset))) return EAD_ERR_RANGE;
    return fn_El2409Get(&image[terminal->output_offset], terminal->output_bytes, channel, value);
}

ead_result_t fn_EadGetDigitalMask(const ead_terminal_manager_t *manager,
                                  const char *name,
                                  uint16_t *mask)
{
    const ead_terminal_address_t *terminal;
    uint8_t image[EAD_MAX_ASSEMBLY_SIZE];
    size_t size = 0U;
    ead_result_t result = fn_GetTerminal(manager, name, &terminal);
    if (result != EAD_OK) return result;
    if (terminal->descriptor.domain == EAD_DOMAIN_DI)
    {
        result = fn_EadProcessImageSnapshotInput(manager->image, image, sizeof(image), &size);
        if (result != EAD_OK) return result;
        if ((terminal->input_offset > size) || (terminal->input_bytes > (size - terminal->input_offset))) return EAD_ERR_RANGE;
        return fn_El1409Mask(&image[terminal->input_offset], terminal->input_bytes, mask);
    }
    if (terminal->descriptor.domain == EAD_DOMAIN_DO)
    {
        result = fn_EadProcessImageSnapshotOutput(manager->image, image, sizeof(image), &size);
        if (result != EAD_OK) return result;
        if ((terminal->output_offset > size) || (terminal->output_bytes > (size - terminal->output_offset))) return EAD_ERR_RANGE;
        return fn_El2409Mask(&image[terminal->output_offset], terminal->output_bytes, mask);
    }
    return EAD_ERR_ARGUMENT;
}

ead_result_t fn_EadSetDigital(ead_terminal_manager_t *manager,
                              const char *name,
                              size_t channel,
                              bool value)
{
    const ead_terminal_address_t *terminal;
    uint8_t data[2];
    uint8_t image[EAD_MAX_ASSEMBLY_SIZE];
    size_t size = 0U;
    ead_result_t result = fn_GetTerminal(manager, name, &terminal);
    if (result != EAD_OK) return result;
    if (terminal->descriptor.domain != EAD_DOMAIN_DO) return EAD_ERR_ARGUMENT;
    result = fn_EadProcessImageSnapshotOutput(manager->image, image, sizeof(image), &size);
    if (result != EAD_OK) return result;
    if ((terminal->output_offset > size) || (terminal->output_bytes > (size - terminal->output_offset)) ||
        (terminal->output_bytes < sizeof(data))) return EAD_ERR_RANGE;
    (void)memcpy(data, &image[terminal->output_offset], sizeof(data));
    result = fn_El2409Set(data, sizeof(data), channel, value);
    if (result != EAD_OK) return result;
    return fn_EadProcessImageWriteOutput(manager->image, terminal->output_offset, data, sizeof(data));
}

ead_result_t fn_EadSetDigitalMask(ead_terminal_manager_t *manager,
                                  const char *name,
                                  uint16_t mask)
{
    const ead_terminal_address_t *terminal;
    uint8_t data[2];
    ead_result_t result = fn_GetTerminal(manager, name, &terminal);
    if (result != EAD_OK) return result;
    if ((terminal->descriptor.domain != EAD_DOMAIN_DO) || (terminal->output_bytes < sizeof(data)))
        return EAD_ERR_ARGUMENT;
    result = fn_El2409SetMask(data, sizeof(data), mask);
    if (result != EAD_OK) return result;
    return fn_EadProcessImageWriteOutput(manager->image, terminal->output_offset, data, sizeof(data));
}

ead_result_t fn_EadGetAnalog(const ead_terminal_manager_t *manager,
                             const char *name,
                             size_t channel,
                             ead_analog_sample_t *sample)
{
    const ead_terminal_address_t *terminal;
    uint8_t image[EAD_MAX_ASSEMBLY_SIZE];
    size_t size = 0U;
    ead_result_t result = fn_GetTerminal(manager, name, &terminal);
    if (result != EAD_OK) return result;
    if (terminal->descriptor.domain == EAD_DOMAIN_AI)
    {
        result = fn_EadProcessImageSnapshotInput(manager->image, image, sizeof(image), &size);
        if (result != EAD_OK) return result;
        if ((terminal->input_offset > size) || (terminal->input_bytes > (size - terminal->input_offset))) return EAD_ERR_RANGE;
        return fn_El3074Decode(&image[terminal->input_offset], terminal->input_bytes,
                               channel, &terminal->config, sample);
    }
    if (terminal->descriptor.domain == EAD_DOMAIN_AO)
    {
        result = fn_EadProcessImageSnapshotOutput(manager->image, image, sizeof(image), &size);
        if (result != EAD_OK) return result;
        if ((terminal->output_offset > size) ||
            (terminal->output_bytes > (size - terminal->output_offset))) return EAD_ERR_RANGE;
        result = fn_El4074Decode(&image[terminal->output_offset], terminal->output_bytes,
                                 channel, &terminal->config, sample);
        if (result != EAD_OK) return result;

        if (terminal->input_bytes >= (terminal->descriptor.channels * 2U))
        {
            result = fn_EadProcessImageSnapshotInput(manager->image, image, sizeof(image), &size);
            if (result != EAD_OK) return result;
            if ((terminal->input_offset > size) ||
                (terminal->input_bytes > (size - terminal->input_offset))) return EAD_ERR_RANGE;
            result = fn_El4074Status(&image[terminal->input_offset], terminal->input_bytes,
                                     channel, &terminal->config, &sample->status);
            if (result != EAD_OK) return result;
        }
        return EAD_OK;
    }
    return EAD_ERR_ARGUMENT;
}

ead_result_t fn_EadSetAnalog(ead_terminal_manager_t *manager,
                             const char *name,
                             size_t channel,
                             double value)
{
    const ead_terminal_address_t *terminal;
    uint8_t data[64];
    uint8_t image[EAD_MAX_ASSEMBLY_SIZE];
    size_t size = 0U;
    ead_result_t result = fn_GetTerminal(manager, name, &terminal);
    if (result != EAD_OK) return result;
    if ((terminal->descriptor.domain != EAD_DOMAIN_AO) ||
        (terminal->output_bytes > sizeof(data))) return EAD_ERR_ARGUMENT;
    result = fn_EadProcessImageSnapshotOutput(manager->image, image, sizeof(image), &size);
    if (result != EAD_OK) return result;
    if ((terminal->output_offset > size) || (terminal->output_bytes > (size - terminal->output_offset))) return EAD_ERR_RANGE;
    (void)memcpy(data, &image[terminal->output_offset], terminal->output_bytes);
    result = fn_El4074Encode(data, terminal->output_bytes, channel, value, &terminal->config);
    if (result != EAD_OK) return result;
    return fn_EadProcessImageWriteOutput(manager->image, terminal->output_offset,
                                         data, terminal->output_bytes);
}

ead_result_t fn_EadGetDigitalLogical(const ead_terminal_manager_t *manager,
                                     ead_address_domain_t domain,
                                     size_t logical_channel,
                                     bool *value)
{
    const ead_terminal_address_t *terminal;
    size_t local_channel;
    ead_result_t result;
    if ((domain != EAD_DOMAIN_DI) && (domain != EAD_DOMAIN_DO)) return EAD_ERR_ARGUMENT;
    result = fn_EadResolveLogical(manager->layout, domain, logical_channel,
                                  &terminal, &local_channel);
    if (result != EAD_OK) return result;
    return fn_EadGetDigital(manager, terminal->config.name, local_channel, value);
}

ead_result_t fn_EadSetDigitalLogical(ead_terminal_manager_t *manager,
                                     ead_address_domain_t domain,
                                     size_t logical_channel,
                                     bool value)
{
    const ead_terminal_address_t *terminal;
    size_t local_channel;
    ead_result_t result;
    if (domain != EAD_DOMAIN_DO) return EAD_ERR_ARGUMENT;
    result = fn_EadResolveLogical(manager->layout, domain, logical_channel,
                                  &terminal, &local_channel);
    if (result != EAD_OK) return result;
    return fn_EadSetDigital(manager, terminal->config.name, local_channel, value);
}

ead_result_t fn_EadGetAnalogLogical(const ead_terminal_manager_t *manager,
                                    ead_address_domain_t domain,
                                    size_t logical_channel,
                                    ead_analog_sample_t *sample)
{
    const ead_terminal_address_t *terminal;
    size_t local_channel;
    ead_result_t result;
    if ((domain != EAD_DOMAIN_AI) && (domain != EAD_DOMAIN_AO)) return EAD_ERR_ARGUMENT;
    result = fn_EadResolveLogical(manager->layout, domain, logical_channel,
                                  &terminal, &local_channel);
    if (result != EAD_OK) return result;
    return fn_EadGetAnalog(manager, terminal->config.name, local_channel, sample);
}

ead_result_t fn_EadSetAnalogLogical(ead_terminal_manager_t *manager,
                                    ead_address_domain_t domain,
                                    size_t logical_channel,
                                    double value)
{
    const ead_terminal_address_t *terminal;
    size_t local_channel;
    ead_result_t result;
    if (domain != EAD_DOMAIN_AO) return EAD_ERR_ARGUMENT;
    result = fn_EadResolveLogical(manager->layout, domain, logical_channel,
                                  &terminal, &local_channel);
    if (result != EAD_OK) return result;
    return fn_EadSetAnalog(manager, terminal->config.name, local_channel, value);
}
