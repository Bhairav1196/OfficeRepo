/**
 * @file el2409.c
 * @brief EL2409 digital-output process-image read/write codec.
 * @author Prathamesh Chipkar
 */
#include "ead/terminals.h"

ead_result_t fn_El2409Get(const uint8_t *data, size_t size, size_t channel, bool *value)
{
    size_t bit;
    if ((data == NULL) || (value == NULL) || (size < 2U) ||
        (channel < 1U) || (channel > 16U)) return EAD_ERR_ARGUMENT;
    bit = channel - 1U;
    *value = (data[bit / 8U] & (uint8_t)(1U << (bit % 8U))) != 0U;
    return EAD_OK;
}

ead_result_t fn_El2409Mask(const uint8_t *data, size_t size, uint16_t *mask)
{
    if ((data == NULL) || (mask == NULL) || (size < 2U)) return EAD_ERR_ARGUMENT;
    *mask = (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8U);
    return EAD_OK;
}

ead_result_t fn_El2409Set(uint8_t *data, size_t size, size_t channel, bool value)
{
    size_t bit;
    uint8_t mask;
    if ((data == NULL) || (size < 2U) || (channel < 1U) || (channel > 16U))
        return EAD_ERR_ARGUMENT;
    bit = channel - 1U;
    mask = (uint8_t)(1U << (bit % 8U));
    if (value) data[bit / 8U] = (uint8_t)(data[bit / 8U] | mask);
    else data[bit / 8U] = (uint8_t)(data[bit / 8U] & (uint8_t)(~mask));
    return EAD_OK;
}

ead_result_t fn_El2409SetMask(uint8_t *data, size_t size, uint16_t mask)
{
    if ((data == NULL) || (size < 2U)) return EAD_ERR_ARGUMENT;
    data[0] = (uint8_t)(mask & UINT16_C(0x00FF));
    data[1] = (uint8_t)((mask >> 8U) & UINT16_C(0x00FF));
    return EAD_OK;
}
