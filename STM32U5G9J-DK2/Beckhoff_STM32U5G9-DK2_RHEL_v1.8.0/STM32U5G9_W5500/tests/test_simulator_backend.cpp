/**
 * @file    test_simulator_backend.cpp
 * @brief   Host test for the single-unit TouchGFX simulator HMI backend.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */
#include "hmi_service.h"
#include "io_signal_ids.h"
#include <cassert>
#include <cmath>
#include <cstring>

int main()
{
    S_HMI_SystemStatus status{};
    S_HMI_UnitInfo unit{};
    S_HMI_SignalView view{};

    assert(fn_HMI_GetSystemStatus(&status));
    assert(status.b_TcpConnected);
    assert(status.b_EthernetLinkUp);
    assert(status.e_ConfigStatus == E_IO_CONFIG_OK);

    assert(fn_HMI_GetUnitInfo(1U, &unit));
    assert(unit.ui8_DiCount == 16U && unit.ui8_DoCount == 16U && unit.ui8_AiCount == 4U && unit.ui8_AoCount == 4U);
    assert(!fn_HMI_GetUnitInfo(2U, &unit));

    /* Unit 1 exposes all digital, AI and AO slots. */
    assert(fn_HMI_GetUnitSlot(1U, E_IO_SIGNAL_DI, 15U, &view));
    assert(view.ui8_UnitId == 1U);
    assert(fn_HMI_GetUnitSlot(1U, E_IO_SIGNAL_DO, 15U, &view));
    assert(fn_HMI_GetUnitSlot(1U, E_IO_SIGNAL_AI, 0U, &view));
    assert(view.pc_Name != nullptr);
    assert(std::strcmp(view.pc_Unit, "V") == 0);
    assert(view.ui8_DecimalPlaces == 3U);
    assert(fn_HMI_GetUnitSlot(1U, E_IO_SIGNAL_AO, 0U, &view));
    assert(fn_HMI_GetUnitSlot(1U, E_IO_SIGNAL_DO, 15U, &view));
    const uint16_t doId = view.ui16_SignalId;
    const bool oldState = view.b_DigitalState;
    assert(fn_HMI_RequestDigital(doId, !oldState) == E_IO_STATUS_OK);
    assert(fn_HMI_GetUnitSlot(1U, E_IO_SIGNAL_DO, 15U, &view));
    assert(view.b_DigitalState == !oldState);

    assert(fn_HMI_GetUnitSlot(1U, E_IO_SIGNAL_AO, 0U, &view));
    const uint16_t aoId = view.ui16_SignalId;
    assert(std::strcmp(view.pc_Unit, "mA") == 0);
    assert(view.ui8_DecimalPlaces == 3U);
    assert(fn_HMI_RequestAnalog(aoId, 1.320F) == E_IO_STATUS_OK);
    assert(fn_HMI_GetUnitSlot(1U, E_IO_SIGNAL_AO, 0U, &view));
    assert(std::fabs(view.f32_ProcessValue - 1.32F) < 0.001F);
    assert(view.i32_RawValue == 132);
    assert(fn_HMI_RequestAnalog(aoId, 20.001F) == E_IO_STATUS_OUT_OF_RANGE);
    assert(!fn_HMI_GetUnitSlot(2U, E_IO_SIGNAL_AI, 0U, &view));

    assert(fn_HMI_RequestDigital(IO_ID_U1_DI(1U), true) == E_IO_STATUS_NOT_WRITABLE);
    return 0;
}
