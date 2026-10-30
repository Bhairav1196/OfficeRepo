#ifndef SCREEN1_VIEW_HPP
#define SCREEN1_VIEW_HPP

#include <gui_generated/screen1_screen/Screen1ViewBase.hpp>
#include <gui/screen1_screen/Screen1Presenter.hpp>
#include <gui/model/Model.hpp>
#include <touchgfx/events/ClickEvent.hpp>
#include <touchgfx/Unicode.hpp>
#include <touchgfx/widgets/Box.hpp>
#include <touchgfx/widgets/TextArea.hpp>
#include <touchgfx/widgets/TextAreaWithWildcard.hpp>

/**
 * @file    Screen1View.hpp
 * @brief   LADSL / SWAN six-page dark maritime operator HMI.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 *
 * @details
 * Page 1 presents 16 DI, 16 DO, 4 AI and 4 AO simultaneously. Five additional
 * pages provide the motor, vessel sensor, fin LCP, MCR and bridge UI framework.
 * The view receives already-converted process values from hmi_service.h and
 * never interprets Modbus addresses or raw register values. Analog units and
 * precision are therefore configuration driven.
 */
class Screen1View : public Screen1ViewBase
{
public:
    Screen1View();
    virtual ~Screen1View() {}

    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void handleClickEvent(const touchgfx::ClickEvent& event);

    /** @brief Select one of the six maritime dashboard pages. */
    void showPage(uint8_t page);

    /** @brief Applies the latest unit-aware HMI frame. */
    void updateFromFrame(const HmiMainFrame& frame);

    /* TouchGFX Designer ActionCustom callbacks for rounded buttons. */
    virtual void selectAnalogInputs();
    virtual void selectAnalogOutputs();
    virtual void adjustAoMinus01();
    virtual void adjustAoPlus01();
    virtual void adjustAoMinus02();
    virtual void adjustAoPlus02();
    virtual void adjustAoMinus03();
    virtual void adjustAoPlus03();
    virtual void adjustAoMinus04();
    virtual void adjustAoPlus04();
    virtual void toggleAoStep();

protected:
    HmiMainFrame currentFrame;
    bool haveFrame;

    touchgfx::Unicode::UnicodeChar tcpValueBuffer[24] = {0};
    touchgfx::Unicode::UnicodeChar uptimeValueBuffer[24] = {0};
    touchgfx::Unicode::UnicodeChar unitNameBuffer[2][24] = {{0}};
    touchgfx::Unicode::UnicodeChar unitDescriptionBuffer[56] = {0};
    touchgfx::Unicode::UnicodeChar analogTitleBuffer[32] = {0};

    touchgfx::Unicode::UnicodeChar diNameBuffer[HMI_DIGITAL_SLOT_COUNT][32] = {{0}};
    touchgfx::Unicode::UnicodeChar diValueBuffer[HMI_DIGITAL_SLOT_COUNT][12] = {{0}};
    touchgfx::Unicode::UnicodeChar doNameBuffer[HMI_DIGITAL_SLOT_COUNT][32] = {{0}};
    touchgfx::Unicode::UnicodeChar doValueBuffer[HMI_DIGITAL_SLOT_COUNT][12] = {{0}};
    touchgfx::Unicode::UnicodeChar analogNameBuffer[HMI_ANALOG_SLOT_COUNT][38] = {{0}};
    touchgfx::Unicode::UnicodeChar analogValueBuffer[HMI_ANALOG_SLOT_COUNT][32] = {{0}};
    touchgfx::Unicode::UnicodeChar aoNameBuffer[HMI_ANALOG_SLOT_COUNT][38] = {{0}};
    touchgfx::Unicode::UnicodeChar aoValueBuffer[HMI_ANALOG_SLOT_COUNT][32] = {{0}};
    touchgfx::Unicode::UnicodeChar aoStepLabelBuffer[24] = {0};
    bool aoFineStep = false;
    bool aoCommandPending[HMI_ANALOG_SLOT_COUNT] = {false};
    uint8_t aoPendingFrames[HMI_ANALOG_SLOT_COUNT] = {0};
    int32_t aoCommandCentiMa[HMI_ANALOG_SLOT_COUNT] = {0};

    /* Designer owns the I/O widgets; these indexed pointers are convenience views. */
    touchgfx::Box* headerBands[3] = {0};
    touchgfx::Box* navTiles[6] = {0};
    touchgfx::TextAreaWithOneWildcard* navLabels[6] = {0};
    touchgfx::Unicode::UnicodeChar headerStatusBuffer[32] = {0};
    touchgfx::Unicode::UnicodeChar navLabelBuffer[6][18] = {{0}};

    touchgfx::Box* aiTracks[HMI_ANALOG_SLOT_COUNT] = {0};
    touchgfx::Box* aiFills[HMI_ANALOG_SLOT_COUNT] = {0};
    touchgfx::Box* aoCards[HMI_ANALOG_SLOT_COUNT] = {0};
    touchgfx::Box* aoTracks[HMI_ANALOG_SLOT_COUNT] = {0};
    touchgfx::Box* aoFills[HMI_ANALOG_SLOT_COUNT] = {0};
    touchgfx::TextAreaWithOneWildcard* aoNames[HMI_ANALOG_SLOT_COUNT] = {0};
    touchgfx::TextAreaWithOneWildcard* aoValues[HMI_ANALOG_SLOT_COUNT] = {0};
    touchgfx::Unicode::UnicodeChar aiSectionTitleBuffer[24] = {0};
    touchgfx::Unicode::UnicodeChar aoSectionTitleBuffer[24] = {0};

    touchgfx::Box pageOverlay;
    touchgfx::Box pagePanels[6];
    touchgfx::TextAreaWithOneWildcard pageTitle;
    touchgfx::TextAreaWithOneWildcard pageSubtitle;
    touchgfx::TextAreaWithOneWildcard pageMetricNames[12];
    touchgfx::TextAreaWithOneWildcard pageMetricValues[12];
    touchgfx::Unicode::UnicodeChar pageTitleBuffer[48] = {0};
    touchgfx::Unicode::UnicodeChar pageSubtitleBuffer[80] = {0};
    touchgfx::Unicode::UnicodeChar pageMetricNameBuffer[12][32] = {{0}};
    touchgfx::Unicode::UnicodeChar pageMetricValueBuffer[12][32] = {{0}};

private:
    void bindLiveTextBuffers();
    void buildMaritimeFramework();
    void configureIoPage();
    void updatePageContent();
    void updateSystemStatus(const HmiMainFrame& frame);
    void updateUnitSelection(const HmiMainFrame& frame);
    void updateDigitalSlots(const HmiMainFrame& frame);
    void updateAnalogSlots(const HmiMainFrame& frame);
    void adjustAnalogSlot(uint8_t slot, float direction);
    void updateAoStepLabel();

    uint8_t activePage;

    static bool contains(const touchgfx::Drawable& drawable, int16_t x, int16_t y);
    static touchgfx::colortype qualityColor(E_IO_Quality quality, bool active);
    static void copyUtf8(touchgfx::Unicode::UnicodeChar* out, uint16_t length, const char* text);
    static void formatProcessValue(touchgfx::Unicode::UnicodeChar* out,
                                   uint16_t length,
                                   const S_HMI_SignalView& signal);
};

#endif /* SCREEN1_VIEW_HPP */
