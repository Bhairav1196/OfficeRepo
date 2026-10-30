/**
 * @file    test_touchgfx_binding.cpp
 * @brief   Host binding test for the single-unit configuration-driven TouchGFX MVC layer.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */
#include <cassert>
#include <cstring>
#include <string>
#include <gui/model/Model.hpp>
#include <gui/screen1_screen/Screen1Presenter.hpp>
#include <gui/screen1_screen/Screen1View.hpp>
#include "hmi_service.h"

static S_HMI_SystemStatus g_status = {true, true, 1234U, E_IO_CONFIG_OK};
static S_HMI_UnitInfo g_unit;
static S_HMI_SignalView g_di[16];
static S_HMI_SignalView g_do[16];
static S_HMI_SignalView g_ai[4];
static S_HMI_SignalView g_ao[4];
static uint16_t g_digitalRequestId;
static bool g_digitalRequestState;
static uint16_t g_analogRequestId;
static float g_analogRequestValue;

extern "C" bool fn_HMI_GetSystemStatus(S_HMI_SystemStatus *pst_Status)
{
    if (pst_Status == nullptr) return false;
    *pst_Status = g_status;
    return true;
}

extern "C" bool fn_HMI_GetUnitInfo(uint8_t ui8_UnitId, S_HMI_UnitInfo *pst_Info)
{
    if ((pst_Info == nullptr) || (ui8_UnitId != 1U)) return false;
    *pst_Info = g_unit;
    return true;
}

extern "C" bool fn_HMI_GetUnitSlot(uint8_t ui8_UnitId,
                                     E_IO_SignalType e_Type,
                                     uint8_t ui8_Slot,
                                     S_HMI_SignalView *pst_View)
{
    if ((pst_View == nullptr) || (ui8_UnitId != 1U)) return false;
    if ((e_Type == E_IO_SIGNAL_DI) && (ui8_Slot < 16U)) *pst_View = g_di[ui8_Slot];
    else if ((e_Type == E_IO_SIGNAL_DO) && (ui8_Slot < 16U)) *pst_View = g_do[ui8_Slot];
    else if ((e_Type == E_IO_SIGNAL_AI) && (ui8_Slot < 4U)) *pst_View = g_ai[ui8_Slot];
    else if ((e_Type == E_IO_SIGNAL_AO) && (ui8_Slot < 4U)) *pst_View = g_ao[ui8_Slot];
    else return false;
    return true;
}

extern "C" E_IO_Status fn_HMI_RequestDigital(uint16_t ui16_SignalId, bool b_State)
{
    g_digitalRequestId = ui16_SignalId;
    g_digitalRequestState = b_State;
    return E_IO_STATUS_OK;
}

extern "C" E_IO_Status fn_HMI_RequestAnalog(uint16_t ui16_SignalId, float f32_ProcessValue)
{
    g_analogRequestId = ui16_SignalId;
    g_analogRequestValue = f32_ProcessValue;
    return E_IO_STATUS_OK;
}

extern "C" bool fn_HMI_GetDiagnosticSnapshot(S_AppDiagnosticSnapshot*) { return false; }

static std::string fn_Narrow(const touchgfx::Unicode::UnicodeChar *pui16_Text)
{
    std::string out;
    while ((pui16_Text != nullptr) && (*pui16_Text != 0U)) out.push_back(static_cast<char>(*pui16_Text++));
    return out;
}

static S_HMI_SignalView fn_Signal(uint16_t id, uint8_t unitId, const char* name, const char* unit,
                                  E_IO_SignalType type, float value, bool state, bool writable)
{
    S_HMI_SignalView s{};
    s.ui16_SignalId = id;
    s.ui8_UnitId = unitId;
    s.pc_Name = name;
    s.pc_Unit = unit;
    s.e_Type = type;
    s.e_Quality = E_IO_QUALITY_GOOD;
    s.f32_ProcessValue = value;
    s.b_Valid = true;
    s.b_DigitalState = state;
    s.b_Writable = writable;
    s.b_Enabled = true;
    s.b_ShowPlusSign = (type == E_IO_SIGNAL_AI) || (type == E_IO_SIGNAL_AO);
    s.ui8_DecimalPlaces = 3U;
    s.f32_HmiStep = 0.100F;
    return s;
}

class Screen1ViewProbe : public Screen1View
{
public:
    using touchgfx::View<Screen1Presenter>::bind;
    std::string tcp() const { return fn_Narrow(tcpValueBuffer); }
    std::string uptime() const { return fn_Narrow(uptimeValueBuffer); }
    std::string unitDescription() const { return fn_Narrow(unitDescriptionBuffer); }
    std::string diName(uint8_t slot) const { return fn_Narrow(diNameBuffer[slot]); }
    std::string diValue(uint8_t slot) const { return fn_Narrow(diValueBuffer[slot]); }
    std::string doName(uint8_t slot) const { return fn_Narrow(doNameBuffer[slot]); }
    std::string analogName(uint8_t slot) const { return fn_Narrow(analogNameBuffer[slot]); }
    std::string analogValue(uint8_t slot) const { return fn_Narrow(analogValueBuffer[slot]); }
    std::string aoName(uint8_t slot) const { return fn_Narrow(aoNameBuffer[slot]); }
    std::string aoValue(uint8_t slot) const { return fn_Narrow(aoValueBuffer[slot]); }
    touchgfx::Rect do16Rect() const { return doState16.getAbsoluteRect(); }
};

int main()
{
    g_unit = {1U, "BECKHOFF I/O", "EK9500  |  16 DI  |  16 DO  |  4 AI  |  4 AO", 16U, 16U, 4U, 4U, true};

    for (uint8_t i = 0U; i < 16U; ++i)
    {
        g_di[i] = fn_Signal(static_cast<uint16_t>(1000U + i), 1U, "DI SIGNAL", "",
                            E_IO_SIGNAL_DI, 0.0F, false, false);
        g_do[i] = fn_Signal(static_cast<uint16_t>(1100U + i), 1U, "DO SIGNAL", "",
                            E_IO_SIGNAL_DO, 0.0F, false, true);
    }
    for (uint8_t i = 0U; i < 4U; ++i)
    {
        g_ai[i] = fn_Signal(static_cast<uint16_t>(2000U + i), 1U, "ANALOG INPUT", "V",
                            E_IO_SIGNAL_AI, (i == 0U) ? 7.5F : 2.5F, false, false);
        g_ao[i] = fn_Signal(static_cast<uint16_t>(3000U + i), 1U, "ANALOG OUTPUT", "mA",
                            E_IO_SIGNAL_AO, (i == 0U) ? 1.32F : 5.0F, false, true);
    }
    g_di[0].b_DigitalState = true;
    g_di[15].b_DigitalState = true;
    g_do[15].b_DigitalState = true;

    Model model;
    Screen1ViewProbe view;
    Screen1Presenter presenter(view);
    presenter.bind(&model);
    model.bind(&presenter);
    view.bind(&presenter);
    view.setupScreen();
    model.tick();

    assert(view.tcp() == "CONNECTED");
    assert(view.uptime() == "1234 s");
    assert(view.unitDescription() == "EK9500  |  16 DI  |  16 DO  |  4 AI  |  4 AO");
    assert(view.diName(15U) == "16");
    assert(view.diValue(15U) == "ON");
    assert(view.analogName(0U) == "ANALOG INPUT");
    assert(view.analogValue(0U) == "+7.500 V");

    assert(view.doName(15U) == "16");
    assert(view.aoName(0U) == "ANALOG OUTPUT");
    assert(view.aoValue(0U) == "+1.320 mA");

    view.showPage(3U);
    view.showPage(0U);

    const touchgfx::Rect d = view.do16Rect();
    view.handleClickEvent(touchgfx::ClickEvent(touchgfx::ClickEvent::RELEASED,
                                               static_cast<int16_t>(d.x + 1),
                                               static_cast<int16_t>(d.y + 1)));
    assert(g_digitalRequestId == g_do[15].ui16_SignalId);
    assert(!g_digitalRequestState);

    view.adjustAoPlus01();
    assert(g_analogRequestId == g_ao[0].ui16_SignalId);
    assert((g_analogRequestValue > 1.419F) && (g_analogRequestValue < 1.421F));
    return 0;
}
