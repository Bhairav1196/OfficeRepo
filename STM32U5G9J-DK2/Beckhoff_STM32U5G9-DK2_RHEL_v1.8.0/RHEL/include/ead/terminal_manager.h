/**
 * @file terminal_manager.h
 * @brief Typed terminal/channel access over the shared EK9500 process image.
 * @author Prathamesh Chipkar
 */
#ifndef EAD_TERMINAL_MANAGER_H
#define EAD_TERMINAL_MANAGER_H

#include "ead/layout.h"
#include "ead/process_image.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Binds a process-image instance to its validated terminal layout. */
typedef struct
{
    const ead_layout_t *layout;
    ead_process_image_t *image;
} ead_terminal_manager_t;

/** @brief Bind a terminal manager to a validated layout and shared process image. */
void fn_EadTerminalManagerInit(ead_terminal_manager_t *manager,
                               const ead_layout_t *layout,
                               ead_process_image_t *image);
/** @brief Read one terminal-local digital input/output channel. */
ead_result_t fn_EadGetDigital(const ead_terminal_manager_t *manager,
                              const char *name,
                              size_t channel,
                              bool *value);
/** @brief Read all digital channels of one terminal instance as a bit mask. */
ead_result_t fn_EadGetDigitalMask(const ead_terminal_manager_t *manager,
                                  const char *name,
                                  uint16_t *mask);
/** @brief Set one terminal-local digital output channel. */
ead_result_t fn_EadSetDigital(ead_terminal_manager_t *manager,
                              const char *name,
                              size_t channel,
                              bool value);
/** @brief Replace all digital outputs of one terminal instance from a bit mask. */
ead_result_t fn_EadSetDigitalMask(ead_terminal_manager_t *manager,
                                  const char *name,
                                  uint16_t mask);
/** @brief Read one terminal-local analog channel and its status/sample metadata. */
ead_result_t fn_EadGetAnalog(const ead_terminal_manager_t *manager,
                             const char *name,
                             size_t channel,
                             ead_analog_sample_t *sample);
/** @brief Write one terminal-local analog output engineering value. */
ead_result_t fn_EadSetAnalog(ead_terminal_manager_t *manager,
                             const char *name,
                             size_t channel,
                             double value);
/** @brief Read one flattened logical digital channel across repeated terminals. */
ead_result_t fn_EadGetDigitalLogical(const ead_terminal_manager_t *manager,
                                     ead_address_domain_t domain,
                                     size_t logical_channel,
                                     bool *value);
/** @brief Write one flattened logical digital output channel. */
ead_result_t fn_EadSetDigitalLogical(ead_terminal_manager_t *manager,
                                     ead_address_domain_t domain,
                                     size_t logical_channel,
                                     bool value);
/** @brief Read one flattened logical analog channel. */
ead_result_t fn_EadGetAnalogLogical(const ead_terminal_manager_t *manager,
                                    ead_address_domain_t domain,
                                    size_t logical_channel,
                                    ead_analog_sample_t *sample);
/** @brief Write one flattened logical analog output channel. */
ead_result_t fn_EadSetAnalogLogical(ead_terminal_manager_t *manager,
                                    ead_address_domain_t domain,
                                    size_t logical_channel,
                                    double value);

#ifdef __cplusplus
}
#endif

#endif
