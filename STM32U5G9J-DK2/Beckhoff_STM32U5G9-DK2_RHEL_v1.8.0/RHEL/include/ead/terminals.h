/**
 * @file terminals.h
 * @brief EL1409, EL2409, EL3074 and EL4074 process-data codec API.
 * @author Prathamesh Chipkar
 */
#ifndef EAD_TERMINALS_H
#define EAD_TERMINALS_H

#include "ead/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Read one EL1409 input bit from a decoded process-image slice. */
ead_result_t fn_El1409Get(const uint8_t *data, size_t size, size_t channel, bool *value);
/** @brief Decode the complete EL1409 input bit mask. */
ead_result_t fn_El1409Mask(const uint8_t *data, size_t size, uint16_t *mask);
/** @brief Read one EL2409 commanded output bit. */
ead_result_t fn_El2409Get(const uint8_t *data, size_t size, size_t channel, bool *value);
/** @brief Decode the complete EL2409 output bit mask. */
ead_result_t fn_El2409Mask(const uint8_t *data, size_t size, uint16_t *mask);
/** @brief Modify one EL2409 output bit in the process-image slice. */
ead_result_t fn_El2409Set(uint8_t *data, size_t size, size_t channel, bool value);
/** @brief Encode a complete EL2409 output bit mask. */
ead_result_t fn_El2409SetMask(uint8_t *data, size_t size, uint16_t mask);
/** @brief Decode one EL3074 signed analog sample and status from the input process image. */
ead_result_t fn_El3074Decode(const uint8_t *data,
                             size_t size,
                             size_t channel,
                             const ead_terminal_config_t *config,
                             ead_analog_sample_t *sample);
/** @brief Decode one EL4074 status/sample representation from the input process image. */
ead_result_t fn_El4074Decode(const uint8_t *data,
                             size_t size,
                             size_t channel,
                             const ead_terminal_config_t *config,
                             ead_analog_sample_t *sample);
/** @brief Encode one engineering command as little-endian REAL32 in the EL4074 output image. */
ead_result_t fn_El4074Encode(uint8_t *data,
                             size_t size,
                             size_t channel,
                             double value,
                             const ead_terminal_config_t *config);
/** @brief Decode one EL4074 channel status word. */
ead_result_t fn_El4074Status(const uint8_t *data,
                             size_t size,
                             size_t channel,
                             const ead_terminal_config_t *config,
                             uint16_t *status);

#ifdef __cplusplus
}
#endif

#endif
