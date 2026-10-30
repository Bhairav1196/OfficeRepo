/**
 * @file eip_endian.h
 * @brief Endian-safe integer helpers used by EtherNet/IP and CIP serialization.
 * @author Prathamesh Chipkar
 */
#ifndef EIP_ENDIAN_H
#define EIP_ENDIAN_H

#include <stddef.h>
#include <stdint.h>

/** @brief Endian conversion helper: fn_ReadLe16. */
static inline uint16_t fn_ReadLe16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8U));
}

/** @brief Endian conversion helper: fn_ReadLe32. */
static inline uint32_t fn_ReadLe32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) |
           ((uint32_t)p[3] << 24U);
}

/** @brief Endian conversion helper: fn_ReadLe64. */
static inline uint64_t fn_ReadLe64(const uint8_t *p)
{
    return (uint64_t)fn_ReadLe32(p) | ((uint64_t)fn_ReadLe32(p + 4U) << 32U);
}

/** @brief Endian conversion helper: fn_WriteLe16. */
static inline void fn_WriteLe16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value & 0xFFU);
    p[1] = (uint8_t)((value >> 8U) & 0xFFU);
}

/** @brief Endian conversion helper: fn_WriteLe32. */
static inline void fn_WriteLe32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value & 0xFFU);
    p[1] = (uint8_t)((value >> 8U) & 0xFFU);
    p[2] = (uint8_t)((value >> 16U) & 0xFFU);
    p[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

/** @brief Endian conversion helper: fn_WriteLe64. */
static inline void fn_WriteLe64(uint8_t *p, uint64_t value)
{
    fn_WriteLe32(p, (uint32_t)(value & 0xFFFFFFFFULL));
    fn_WriteLe32(p + 4U, (uint32_t)(value >> 32U));
}

/** @brief Endian conversion helper: fn_ReadBe16. */
static inline uint16_t fn_ReadBe16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8U) | (uint16_t)p[1]);
}

#endif
