/**
 * @file el1409.c
 * @brief EL1409 digital-input process-image decoder.
 * @author Prathamesh Chipkar
 */
#include "ead/terminals.h"

ead_result_t fn_El1409Get(const uint8_t *data, size_t size, size_t channel, bool *value)
{
    size_t bit;
    if ((data == NULL) || (value == NULL) || (size < 2U) ||
        (channel < 1U) || (channel > 16U)) return EAD_ERR_ARGUMENT;
    bit = channel - 1U;
    *value = (data[bit / 8U] & (uint8_t)(1U << (bit % 8U))) != 0U;
    return EAD_OK;
}

ead_result_t fn_El1409Mask(const uint8_t *data, size_t size, uint16_t *mask)
{
    if ((data == NULL) || (mask == NULL) || (size < 2U)) return EAD_ERR_ARGUMENT;
    *mask = (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8U);
    return EAD_OK;
}
