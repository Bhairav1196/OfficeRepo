/**
 * @file process_image.c
 * @brief Thread-safe process-image implementation and freshness/revision accounting.
 * @author Prathamesh Chipkar
 */
#define _POSIX_C_SOURCE 200809L
#include "ead/process_image.h"

#include <string.h>
#include <time.h>

static uint64_t fn_MonotonicNs(void)
{
    struct timespec now;
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    return ((uint64_t)now.tv_sec * UINT64_C(1000000000)) + (uint64_t)now.tv_nsec;
}

ead_result_t fn_EadProcessImageInit(ead_process_image_t *image,
                                    size_t input_size,
                                    size_t output_size)
{
    if ((image == NULL) || (input_size > EAD_MAX_ASSEMBLY_SIZE) ||
        (output_size > EAD_MAX_ASSEMBLY_SIZE)) return EAD_ERR_ARGUMENT;
    (void)memset(image, 0, sizeof(*image));
    image->input_size = input_size;
    image->output_size = output_size;
    if (pthread_mutex_init(&image->mutex, NULL) != 0) return EAD_ERR_IO;
    image->mutex_initialized = true;
    return EAD_OK;
}

void fn_EadProcessImageDestroy(ead_process_image_t *image)
{
    if ((image != NULL) && image->mutex_initialized)
    {
        (void)pthread_mutex_destroy(&image->mutex);
        image->mutex_initialized = false;
    }
}

ead_result_t fn_EadProcessImageReplaceInput(ead_process_image_t *image,
                                            const uint8_t *data,
                                            size_t size)
{
    if ((image == NULL) || (data == NULL) || (size != image->input_size)) return EAD_ERR_ARGUMENT;
    (void)pthread_mutex_lock(&image->mutex);
    (void)memcpy(image->input, data, size);
    ++image->input_sequence;
    image->last_input_ns = fn_MonotonicNs();
    image->has_input = true;
    (void)pthread_mutex_unlock(&image->mutex);
    return EAD_OK;
}

ead_result_t fn_EadProcessImageReplaceOutput(ead_process_image_t *image,
                                             const uint8_t *data,
                                             size_t size)
{
    if ((image == NULL) || (data == NULL) || (size != image->output_size)) return EAD_ERR_ARGUMENT;
    (void)pthread_mutex_lock(&image->mutex);
    if (memcmp(image->output, data, size) != 0)
    {
        (void)memcpy(image->output, data, size);
        ++image->output_revision;
    }
    (void)pthread_mutex_unlock(&image->mutex);
    return EAD_OK;
}

static ead_result_t fn_Snapshot(ead_process_image_t *image,
                               const uint8_t *source,
                               size_t source_size,
                               uint8_t *data,
                               size_t capacity,
                               size_t *size)
{
    if ((image == NULL) || (data == NULL) || (size == NULL) || (capacity < source_size)) return EAD_ERR_ARGUMENT;
    (void)pthread_mutex_lock(&image->mutex);
    (void)memcpy(data, source, source_size);
    *size = source_size;
    (void)pthread_mutex_unlock(&image->mutex);
    return EAD_OK;
}

ead_result_t fn_EadProcessImageSnapshotInput(ead_process_image_t *image,
                                             uint8_t *data,
                                             size_t capacity,
                                             size_t *size)
{
    return fn_Snapshot(image, image->input, image->input_size, data, capacity, size);
}

ead_result_t fn_EadProcessImageSnapshotOutput(ead_process_image_t *image,
                                              uint8_t *data,
                                              size_t capacity,
                                              size_t *size)
{
    return fn_Snapshot(image, image->output, image->output_size, data, capacity, size);
}

ead_result_t fn_EadProcessImageWriteOutput(ead_process_image_t *image,
                                           size_t offset,
                                           const uint8_t *data,
                                           size_t size)
{
    if ((image == NULL) || (data == NULL) || (offset > image->output_size) ||
        (size > (image->output_size - offset))) return EAD_ERR_ARGUMENT;
    (void)pthread_mutex_lock(&image->mutex);
    if (memcmp(&image->output[offset], data, size) != 0)
    {
        (void)memcpy(&image->output[offset], data, size);
        ++image->output_revision;
    }
    (void)pthread_mutex_unlock(&image->mutex);
    return EAD_OK;
}

uint64_t fn_EadProcessImageOutputRevision(ead_process_image_t *image)
{
    uint64_t value;
    (void)pthread_mutex_lock(&image->mutex);
    value = image->output_revision;
    (void)pthread_mutex_unlock(&image->mutex);
    return value;
}

uint64_t fn_EadProcessImageInputSequence(ead_process_image_t *image)
{
    uint64_t value;
    (void)pthread_mutex_lock(&image->mutex);
    value = image->input_sequence;
    (void)pthread_mutex_unlock(&image->mutex);
    return value;
}

uint64_t fn_EadProcessImageInputAgeMs(ead_process_image_t *image)
{
    uint64_t last;
    (void)pthread_mutex_lock(&image->mutex);
    last = image->last_input_ns;
    (void)pthread_mutex_unlock(&image->mutex);
    if (last == 0U) return UINT64_MAX;
    return (fn_MonotonicNs() - last) / UINT64_C(1000000);
}

bool fn_EadProcessImageHasInput(ead_process_image_t *image)
{
    bool value;
    (void)pthread_mutex_lock(&image->mutex);
    value = image->has_input;
    (void)pthread_mutex_unlock(&image->mutex);
    return value;
}
