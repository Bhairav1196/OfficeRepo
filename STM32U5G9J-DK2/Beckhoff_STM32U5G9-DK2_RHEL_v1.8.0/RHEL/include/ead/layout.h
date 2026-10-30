/**
 * @file layout.h
 * @brief Process-image layout calculation and logical channel address resolution.
 * @author Prathamesh Chipkar
 */
#ifndef EAD_LAYOUT_H
#define EAD_LAYOUT_H

#include "ead/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Return the static process-data descriptor for a supported terminal kind. */
const ead_terminal_descriptor_t *fn_EadTerminalDescriptor(ead_terminal_kind_t kind);
/** @brief Calculate terminal offsets and total input/output process-image sizes. */
ead_result_t fn_EadBuildLayout(const ead_system_config_t *config,
                               ead_layout_t *layout,
                               char *error,
                               size_t error_size);
/** @brief Find one configured terminal instance by terminal kind and ordinal. */
const ead_terminal_address_t *fn_EadFindTerminal(const ead_layout_t *layout,
                                                  const char *name);
/** @brief Resolve a flat logical channel to terminal instance and local channel. */
ead_result_t fn_EadResolveLogical(const ead_layout_t *layout,
                                  ead_address_domain_t domain,
                                  size_t logical_channel,
                                  const ead_terminal_address_t **terminal,
                                  size_t *local_channel);
/** @brief Format a human-readable layout summary into a caller-provided buffer. */
ead_result_t fn_EadFormatLayout(const ead_layout_t *layout,
                                char *buffer,
                                size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
