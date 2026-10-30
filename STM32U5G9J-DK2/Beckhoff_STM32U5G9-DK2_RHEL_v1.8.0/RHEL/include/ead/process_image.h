/**
 * @file process_image.h
 * @brief Thread-safe input/output process-image storage with sequence and revision tracking.
 * @author Prathamesh Chipkar
 */
#ifndef EAD_PROCESS_IMAGE_H
#define EAD_PROCESS_IMAGE_H

#include "ead/types.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Synchronized process-image buffers and update metadata. */
typedef struct
{
    uint8_t input[EAD_MAX_ASSEMBLY_SIZE];
    uint8_t output[EAD_MAX_ASSEMBLY_SIZE];
    size_t input_size;
    size_t output_size;
    uint64_t input_sequence;
    uint64_t output_revision;
    uint64_t last_input_ns;
    bool has_input;
    pthread_mutex_t mutex;
    bool mutex_initialized;
} ead_process_image_t;

/** @brief Initialize mutexes, buffers and sequence counters for a process image. */
ead_result_t fn_EadProcessImageInit(ead_process_image_t *image,
                                    size_t input_size,
                                    size_t output_size);
/** @brief Destroy synchronization objects and release process-image storage. */
void fn_EadProcessImageDestroy(ead_process_image_t *image);
/** @brief Atomically replace the latest T-to-O input image. */
ead_result_t fn_EadProcessImageReplaceInput(ead_process_image_t *image,
                                            const uint8_t *data,
                                            size_t size);
/** @brief Atomically replace the complete O-to-T output image. */
ead_result_t fn_EadProcessImageReplaceOutput(ead_process_image_t *image,
                                             const uint8_t *data,
                                             size_t size);
/** @brief Copy a consistent input-image snapshot for readers. */
ead_result_t fn_EadProcessImageSnapshotInput(ead_process_image_t *image,
                                             uint8_t *data,
                                             size_t capacity,
                                             size_t *size);
/** @brief Copy a consistent output-image snapshot for readers. */
ead_result_t fn_EadProcessImageSnapshotOutput(ead_process_image_t *image,
                                              uint8_t *data,
                                              size_t capacity,
                                              size_t *size);
/** @brief Atomically patch a bounded byte range in the output image. */
ead_result_t fn_EadProcessImageWriteOutput(ead_process_image_t *image,
                                           size_t offset,
                                           const uint8_t *data,
                                           size_t size);
/** @brief Return the monotonically increasing output revision counter. */
uint64_t fn_EadProcessImageOutputRevision(ead_process_image_t *image);
/** @brief Return the most recently accepted input sequence number. */
uint64_t fn_EadProcessImageInputSequence(ead_process_image_t *image);
/** @brief Return the age in milliseconds of the most recent input image. */
uint64_t fn_EadProcessImageInputAgeMs(ead_process_image_t *image);
/** @brief Report whether at least one valid input image has been received. */
bool fn_EadProcessImageHasInput(ead_process_image_t *image);

#ifdef __cplusplus
}
#endif

#endif
