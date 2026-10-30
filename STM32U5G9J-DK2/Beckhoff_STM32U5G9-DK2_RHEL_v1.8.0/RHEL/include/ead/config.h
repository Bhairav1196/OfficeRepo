/**
 * @file config.h
 * @brief EK9500 terminal-train configuration defaults, parsing and validation API.
 * @author Prathamesh Chipkar
 */
#ifndef EAD_CONFIG_H
#define EAD_CONFIG_H

#include "ead/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Initialize an EK9500 configuration structure with conservative defaults. */
void fn_EadConfigSetDefaults(ead_system_config_t *config);
/** @brief Load and validate a terminal-train configuration file. */
ead_result_t fn_EadConfigLoad(const char *path,
                              ead_system_config_t *config,
                              char *error,
                              size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
