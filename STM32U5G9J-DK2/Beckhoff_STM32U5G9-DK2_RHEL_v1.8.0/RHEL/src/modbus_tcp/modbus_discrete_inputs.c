/**
 * @file modbus_discrete_inputs.c
 * @brief Function 02 data access for EL1409 input channels.
 * @author Prathamesh Chipkar
 */
#include "modbus_tcp/modbus_discrete_inputs.h"

#include <string.h>

E_GW_RESULT fn_MODB_DiscreteInputsRead(S_MODB_DATA_MODEL *pst_Model,
                                       uint8_t ui8_UnitId,
                                       uint16_t ui16_Start,
                                       uint16_t ui16_Quantity,
                                       uint8_t *pui8_Packed,
                                       size_t sz_PackedCapacity)
{
    size_t sz_ByteCount = ((size_t)ui16_Quantity + 7U) / 8U;
    uint16_t ui16_Index;
    if ((pst_Model == NULL) || (pst_Model->pst_RouteManager == NULL) ||
        (pui8_Packed == NULL) || (ui16_Quantity == 0U))
    {
        return GW_ERR_ARGUMENT;
    }
    if (sz_PackedCapacity < sz_ByteCount)
    {
        return GW_ERR_RANGE;
    }
    (void)memset(pui8_Packed, 0, sz_ByteCount);
    for (ui16_Index = 0U; ui16_Index < ui16_Quantity; ++ui16_Index)
    {
        const uint32_t ui32_Address =
            (uint32_t)ui16_Start + (uint32_t)ui16_Index;
        bool b_Value = false;
        E_GW_RESULT e_Result;
        if (ui32_Address > UINT16_MAX)
        {
            return GW_ERR_RANGE;
        }
        e_Result = fn_GW_RouteReadDigitalInput(
            pst_Model->pst_RouteManager, ui8_UnitId,
            (uint16_t)ui32_Address, &b_Value);
        if (e_Result != GW_OK)
        {
            return e_Result;
        }
        if (b_Value)
        {
            pui8_Packed[ui16_Index / 8U] |=
                (uint8_t)(1U << (ui16_Index % 8U));
        }
    }
    return GW_OK;
}
