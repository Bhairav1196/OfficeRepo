/**
 * @file    Screen1View.cpp
 * @brief   LADSL / SWAN six-page dark maritime operator HMI implementation.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#include <gui/screen1_screen/Screen1View.hpp>
#include <touchgfx/Color.hpp>
#include <touchgfx/Unicode.hpp>
#include <cstring>

namespace
{
const touchgfx::colortype COLOR_ACTIVE  = touchgfx::Color::getColorFromRGB(0, 229, 255);
const touchgfx::colortype COLOR_ON      = touchgfx::Color::getColorFromRGB(46, 214, 126);
const touchgfx::colortype COLOR_IDLE    = touchgfx::Color::getColorFromRGB(45, 58, 73);
const touchgfx::colortype COLOR_WARNING = touchgfx::Color::getColorFromRGB(255, 183, 77);
const touchgfx::colortype COLOR_FAULT   = touchgfx::Color::getColorFromRGB(255, 59, 48);
const touchgfx::colortype COLOR_TEXT    = touchgfx::Color::getColorFromRGB(245, 249, 252);
const touchgfx::colortype COLOR_MUTED   = touchgfx::Color::getColorFromRGB(143, 166, 181);
const touchgfx::colortype COLOR_BG      = touchgfx::Color::getColorFromRGB(11, 15, 25);
const touchgfx::colortype COLOR_PANEL   = touchgfx::Color::getColorFromRGB(17, 27, 42);
const touchgfx::colortype COLOR_GLASS   = touchgfx::Color::getColorFromRGB(22, 38, 57);

static uint32_t fn_Pow10(uint8_t decimals)
{
    static const uint32_t values[4] = {1U, 10U, 100U, 1000U};
    return values[(decimals <= 3U) ? decimals : 3U];
}

static void fn_AppendAscii(touchgfx::Unicode::UnicodeChar* out,
                           uint16_t length,
                           const char* text)
{
    uint16_t index = 0U;
    if ((out == 0) || (length == 0U) || (text == 0))
    {
        return;
    }
    while ((index + 1U < length) && (out[index] != 0U))
    {
        ++index;
    }
    while ((index + 1U < length) && (*text != '\0'))
    {
        out[index++] = static_cast<touchgfx::Unicode::UnicodeChar>(static_cast<unsigned char>(*text++));
    }
    out[index] = 0U;
}
}

Screen1View::Screen1View()
    : haveFrame(false), activePage(0U)
{
    std::memset(&currentFrame, 0, sizeof(currentFrame));
}

void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();
    headerBands[0]=&headerBand01; headerBands[1]=&headerBand02; headerBands[2]=&headerBand03;
    navTiles[0]=&navTile01; navTiles[1]=&navTile02; navTiles[2]=&navTile03; navTiles[3]=&navTile04; navTiles[4]=&navTile05; navTiles[5]=&navTile06; 
    navLabels[0]=&navLabel01; navLabels[1]=&navLabel02; navLabels[2]=&navLabel03; navLabels[3]=&navLabel04; navLabels[4]=&navLabel05; navLabels[5]=&navLabel06; 
    aiTracks[0]=&aiTrack01; aiTracks[1]=&aiTrack02; aiTracks[2]=&aiTrack03; aiTracks[3]=&aiTrack04; 
    aiFills[0]=&aiFill01; aiFills[1]=&aiFill02; aiFills[2]=&aiFill03; aiFills[3]=&aiFill04; 
    aoCards[0]=&aoCard01; aoCards[1]=&aoCard02; aoCards[2]=&aoCard03; aoCards[3]=&aoCard04; 
    aoTracks[0]=&aoTrack01; aoTracks[1]=&aoTrack02; aoTracks[2]=&aoTrack03; aoTracks[3]=&aoTrack04; 
    aoFills[0]=&aoFill01; aoFills[1]=&aoFill02; aoFills[2]=&aoFill03; aoFills[3]=&aoFill04; 
    aoNames[0]=&aoName01; aoNames[1]=&aoName02; aoNames[2]=&aoName03; aoNames[3]=&aoName04; 
    aoValues[0]=&aoValue01; aoValues[1]=&aoValue02; aoValues[2]=&aoValue03; aoValues[3]=&aoValue04; 
    bindLiveTextBuffers();
    configureIoPage();
    buildMaritimeFramework();
    showPage(0U);
}

void Screen1View::tearDownScreen()
{
    Screen1ViewBase::tearDownScreen();
}

void Screen1View::bindLiveTextBuffers()
{
    tcpValue.setWildcard(tcpValueBuffer);
    uptimeValue.setWildcard(uptimeValueBuffer);
    unit1Label.setWildcard(unitNameBuffer[0]);
    unit2Label.setWildcard(unitNameBuffer[1]);
    unitDescription.setWildcard(unitDescriptionBuffer);
    analogTitle.setWildcard(analogTitleBuffer);
    diName01.setWildcard(diNameBuffer[0]); diValue01.setWildcard(diValueBuffer[0]);
    diName02.setWildcard(diNameBuffer[1]); diValue02.setWildcard(diValueBuffer[1]);
    diName03.setWildcard(diNameBuffer[2]); diValue03.setWildcard(diValueBuffer[2]);
    diName04.setWildcard(diNameBuffer[3]); diValue04.setWildcard(diValueBuffer[3]);
    diName05.setWildcard(diNameBuffer[4]); diValue05.setWildcard(diValueBuffer[4]);
    diName06.setWildcard(diNameBuffer[5]); diValue06.setWildcard(diValueBuffer[5]);
    diName07.setWildcard(diNameBuffer[6]); diValue07.setWildcard(diValueBuffer[6]);
    diName08.setWildcard(diNameBuffer[7]); diValue08.setWildcard(diValueBuffer[7]);
    diName09.setWildcard(diNameBuffer[8]); diValue09.setWildcard(diValueBuffer[8]);
    diName10.setWildcard(diNameBuffer[9]); diValue10.setWildcard(diValueBuffer[9]);
    diName11.setWildcard(diNameBuffer[10]); diValue11.setWildcard(diValueBuffer[10]);
    diName12.setWildcard(diNameBuffer[11]); diValue12.setWildcard(diValueBuffer[11]);
    diName13.setWildcard(diNameBuffer[12]); diValue13.setWildcard(diValueBuffer[12]);
    diName14.setWildcard(diNameBuffer[13]); diValue14.setWildcard(diValueBuffer[13]);
    diName15.setWildcard(diNameBuffer[14]); diValue15.setWildcard(diValueBuffer[14]);
    diName16.setWildcard(diNameBuffer[15]); diValue16.setWildcard(diValueBuffer[15]);
    doName01.setWildcard(doNameBuffer[0]); doValue01.setWildcard(doValueBuffer[0]);
    doName02.setWildcard(doNameBuffer[1]); doValue02.setWildcard(doValueBuffer[1]);
    doName03.setWildcard(doNameBuffer[2]); doValue03.setWildcard(doValueBuffer[2]);
    doName04.setWildcard(doNameBuffer[3]); doValue04.setWildcard(doValueBuffer[3]);
    doName05.setWildcard(doNameBuffer[4]); doValue05.setWildcard(doValueBuffer[4]);
    doName06.setWildcard(doNameBuffer[5]); doValue06.setWildcard(doValueBuffer[5]);
    doName07.setWildcard(doNameBuffer[6]); doValue07.setWildcard(doValueBuffer[6]);
    doName08.setWildcard(doNameBuffer[7]); doValue08.setWildcard(doValueBuffer[7]);
    doName09.setWildcard(doNameBuffer[8]); doValue09.setWildcard(doValueBuffer[8]);
    doName10.setWildcard(doNameBuffer[9]); doValue10.setWildcard(doValueBuffer[9]);
    doName11.setWildcard(doNameBuffer[10]); doValue11.setWildcard(doValueBuffer[10]);
    doName12.setWildcard(doNameBuffer[11]); doValue12.setWildcard(doValueBuffer[11]);
    doName13.setWildcard(doNameBuffer[12]); doValue13.setWildcard(doValueBuffer[12]);
    doName14.setWildcard(doNameBuffer[13]); doValue14.setWildcard(doValueBuffer[13]);
    doName15.setWildcard(doNameBuffer[14]); doValue15.setWildcard(doValueBuffer[14]);
    doName16.setWildcard(doNameBuffer[15]); doValue16.setWildcard(doValueBuffer[15]);
    analogName01.setWildcard(analogNameBuffer[0]); analogValue01.setWildcard(analogValueBuffer[0]);
    analogName02.setWildcard(analogNameBuffer[1]); analogValue02.setWildcard(analogValueBuffer[1]);
    analogName03.setWildcard(analogNameBuffer[2]); analogValue03.setWildcard(analogValueBuffer[2]);
    analogName04.setWildcard(analogNameBuffer[3]); analogValue04.setWildcard(analogValueBuffer[3]);
}

void Screen1View::configureIoPage()
{
    touchgfx::Drawable* diRows[HMI_DIGITAL_SLOT_COUNT] = {&diRow01, &diRow02, &diRow03, &diRow04, &diRow05, &diRow06, &diRow07, &diRow08, &diRow09, &diRow10, &diRow11, &diRow12, &diRow13, &diRow14, &diRow15, &diRow16};
    touchgfx::Drawable* doRows[HMI_DIGITAL_SLOT_COUNT] = {&doRow01, &doRow02, &doRow03, &doRow04, &doRow05, &doRow06, &doRow07, &doRow08, &doRow09, &doRow10, &doRow11, &doRow12, &doRow13, &doRow14, &doRow15, &doRow16};
    touchgfx::Box* diStates[HMI_DIGITAL_SLOT_COUNT] = {&diState01, &diState02, &diState03, &diState04, &diState05, &diState06, &diState07, &diState08, &diState09, &diState10, &diState11, &diState12, &diState13, &diState14, &diState15, &diState16};
    touchgfx::Box* doStates[HMI_DIGITAL_SLOT_COUNT] = {&doState01, &doState02, &doState03, &doState04, &doState05, &doState06, &doState07, &doState08, &doState09, &doState10, &doState11, &doState12, &doState13, &doState14, &doState15, &doState16};
    touchgfx::TextAreaWithOneWildcard* diValues[HMI_DIGITAL_SLOT_COUNT] = {&diValue01, &diValue02, &diValue03, &diValue04, &diValue05, &diValue06, &diValue07, &diValue08, &diValue09, &diValue10, &diValue11, &diValue12, &diValue13, &diValue14, &diValue15, &diValue16};
    touchgfx::TextAreaWithOneWildcard* doValues[HMI_DIGITAL_SLOT_COUNT] = {&doValue01, &doValue02, &doValue03, &doValue04, &doValue05, &doValue06, &doValue07, &doValue08, &doValue09, &doValue10, &doValue11, &doValue12, &doValue13, &doValue14, &doValue15, &doValue16};
    touchgfx::Box* aiCards[HMI_ANALOG_SLOT_COUNT] = {&analogCard01, &analogCard02, &analogCard03, &analogCard04};
    touchgfx::Box* aiTops[HMI_ANALOG_SLOT_COUNT] = {&analogCardTop01, &analogCardTop02, &analogCardTop03, &analogCardTop04};

    background.setColor(COLOR_BG);
    headerMetal.setVisible(false); titleText.setVisible(false);
    tcpCaption.setVisible(false); tcpValue.setVisible(false);
    uptimeCaption.setVisible(false); uptimeValue.setVisible(false);
    unit1Button.setVisible(false); unit2Button.setVisible(false);
    unit1Label.setVisible(false); unit2Label.setVisible(false);
    unit1Indicator.setVisible(false); unit2Indicator.setVisible(false);
    unitDescription.setVisible(false); tabsSeparator.setVisible(false);
    analogPanel.setVisible(false); analogAccent.setVisible(false); analogTitle.setVisible(false);

    diPanel.setColor(COLOR_PANEL); doPanel.setColor(COLOR_PANEL);
    diAccent.setColor(COLOR_ACTIVE); doAccent.setColor(COLOR_ACTIVE);
    for (uint8_t slot = 0U; slot < HMI_DIGITAL_SLOT_COUNT; ++slot)
    {
        diRows[slot]->setVisible(false); doRows[slot]->setVisible(false);
        diStates[slot]->setColor(COLOR_IDLE); doStates[slot]->setColor(COLOR_IDLE);
        diValues[slot]->setVisible(false); doValues[slot]->setVisible(false);
    }
    for (uint8_t slot = 0U; slot < HMI_ANALOG_SLOT_COUNT; ++slot)
    {
        aiCards[slot]->setColor(COLOR_PANEL);
        aiTops[slot]->setColor(COLOR_ACTIVE);
    }
    aoMinus01.setVisible(false); aoPlus01.setVisible(false);
    aoMinus02.setVisible(false); aoPlus02.setVisible(false);
    aoMinus03.setVisible(false); aoPlus03.setVisible(false);
    aoMinus04.setVisible(false); aoPlus04.setVisible(false);
    aoMinusLabel01.setVisible(false); aoPlusLabel01.setVisible(false);
    aoMinusLabel02.setVisible(false); aoPlusLabel02.setVisible(false);
    aoMinusLabel03.setVisible(false); aoPlusLabel03.setVisible(false);
    aoMinusLabel04.setVisible(false); aoPlusLabel04.setVisible(false);
}

void Screen1View::buildMaritimeFramework()
{
    static const char* const navText[6] = {"I/O", "MOTOR", "SENSORS", "FIN LCP", "MCR", "BRIDGE"};

    headerStatus.setWildcard(headerStatusBuffer);
    for (uint8_t i = 0U; i < 6U; ++i)
    {
        copyUtf8(navLabelBuffer[i], 18U, navText[i]);
        navLabels[i]->setWildcard(navLabelBuffer[i]);
    }
    for (uint8_t i = 0U; i < HMI_ANALOG_SLOT_COUNT; ++i)
    {
        aoNames[i]->setWildcard(aoNameBuffer[i]);
        aoValues[i]->setWildcard(aoValueBuffer[i]);
    }
    copyUtf8(aiSectionTitleBuffer, 24U, "ANALOG INPUTS");
    aiSectionTitle.setWildcard(aiSectionTitleBuffer);
    copyUtf8(aoSectionTitleBuffer, 24U, "ANALOG OUTPUTS");
    aoSectionTitle.setWildcard(aoSectionTitleBuffer);
    aoStepLabel.setWildcard(aoStepLabelBuffer);
    updateAoStepLabel();

    pageOverlay.setPosition(0, 80, 800, 400); pageOverlay.setColor(COLOR_BG); add(pageOverlay);
    for (uint8_t i = 0U; i < 6U; ++i)
    {
        const int16_t x = static_cast<int16_t>(16 + ((i % 2U) * 392));
        const int16_t y = static_cast<int16_t>(142 + ((i / 2U) * 104));
        pagePanels[i].setPosition(x, y, 376, 92); pagePanels[i].setColor(COLOR_PANEL); add(pagePanels[i]);
    }
    pageTitle.setWildcard(pageTitleBuffer); pageTitle.setTypedText(analogTitle.getTypedText());
    pageTitle.setPosition(18, 92, 500, 28); pageTitle.setColor(COLOR_TEXT); add(pageTitle);
    pageSubtitle.setWildcard(pageSubtitleBuffer); pageSubtitle.setTypedText(unitDescription.getTypedText());
    pageSubtitle.setPosition(18, 119, 764, 20); pageSubtitle.setColor(COLOR_MUTED); add(pageSubtitle);
    for (uint8_t i = 0U; i < 12U; ++i)
    {
        const int16_t panel = static_cast<int16_t>(i / 2U);
        const int16_t x = static_cast<int16_t>(28 + ((panel % 2) * 392) + ((i % 2U) * 184));
        const int16_t y = static_cast<int16_t>(154 + ((panel / 2) * 104));
        pageMetricNames[i].setWildcard(pageMetricNameBuffer[i]); pageMetricNames[i].setTypedText(analogName01.getTypedText());
        pageMetricNames[i].setPosition(x, y, 170, 24); pageMetricNames[i].setColor(COLOR_MUTED); add(pageMetricNames[i]);
        pageMetricValues[i].setWildcard(pageMetricValueBuffer[i]); pageMetricValues[i].setTypedText(analogValue01.getTypedText());
        pageMetricValues[i].setPosition(x, static_cast<int16_t>(y + 28), 170, 30); pageMetricValues[i].setColor(COLOR_ACTIVE); add(pageMetricValues[i]);
    }
}

void Screen1View::showPage(uint8_t page)
{
    activePage = (page < 6U) ? page : 0U;
    const bool ioVisible = (activePage == 0U);
    pageOverlay.setVisible(!ioVisible); pageTitle.setVisible(!ioVisible); pageSubtitle.setVisible(!ioVisible);
    for (uint8_t i = 0U; i < 6U; ++i)
    {
        navTiles[i]->setColor((i == activePage) ? touchgfx::Color::getColorFromRGB(16, 83, 103) : COLOR_GLASS);
        navLabels[i]->setColor((i == activePage) ? COLOR_TEXT : COLOR_MUTED);
        pagePanels[i].setVisible(!ioVisible);
    }
    for (uint8_t i = 0U; i < 12U; ++i)
    {
        pageMetricNames[i].setVisible(!ioVisible); pageMetricValues[i].setVisible(!ioVisible);
    }
    for (uint8_t i = 0U; i < HMI_ANALOG_SLOT_COUNT; ++i)
    {
        aiTracks[i]->setVisible(ioVisible); aiFills[i]->setVisible(ioVisible);
        aoCards[i]->setVisible(ioVisible); aoTracks[i]->setVisible(ioVisible); aoFills[i]->setVisible(ioVisible);
        aoNames[i]->setVisible(ioVisible); aoValues[i]->setVisible(ioVisible);
    }
    aiSectionTitle.setVisible(ioVisible); aoSectionTitle.setVisible(ioVisible);
    aoStepButton.setVisible(ioVisible); aoStepLabel.setVisible(ioVisible);
    if (!ioVisible)
    {
        aoMinus01.setVisible(false); aoPlus01.setVisible(false);
        aoMinus02.setVisible(false); aoPlus02.setVisible(false);
        aoMinus03.setVisible(false); aoPlus03.setVisible(false);
        aoMinus04.setVisible(false); aoPlus04.setVisible(false);
        aoMinusLabel01.setVisible(false); aoPlusLabel01.setVisible(false);
        aoMinusLabel02.setVisible(false); aoPlusLabel02.setVisible(false);
        aoMinusLabel03.setVisible(false); aoPlusLabel03.setVisible(false);
        aoMinusLabel04.setVisible(false); aoPlusLabel04.setVisible(false);
    }
    if (ioVisible && haveFrame)
    {
        updateAnalogSlots(currentFrame);
    }
    updatePageContent();
    invalidate();
}

void Screen1View::updatePageContent()
{
    static const char* const titles[5] = {"MOTOR STARTER PANEL", "ICGS VESSEL SENSORS HUB", "FIN STABILIZER LOCAL CONTROL", "MACHINERY CONTROL ROOM", "BRIDGE CONSOLE"};
    static const char* const subtitles[5] = {"Pump and fan control / protection overview", "Environmental, tank and structural telemetry", "Port and starboard hydraulic motion diagnostics", "Propulsion, gearbox and generator interrogation", "Command-level navigation and safety overview"};
    static const char* const names[5][12] = {
        {"HYD PUMP 1", "CONTACTOR", "HYD PUMP 2", "OVERLOAD", "VENT FAN P", "CURRENT", "VENT FAN S", "RUN HOURS", "START", "STOP", "RESET", "INTERLOCK"},
        {"BILGE FWD", "BILGE AFT", "FUEL TANK", "BALLAST TANK", "EXHAUST P", "EXHAUST S", "ROOM TEMP", "HUMIDITY", "SEA WATER", "FRESH WATER", "FIRE LOOP", "DOOR STATE"},
        {"PORT ANGLE", "STBD ANGLE", "PORT PRESS", "STBD PRESS", "PORT OIL", "STBD OIL", "PORT MODE", "STBD MODE", "ROLL", "ROLL RATE", "PORT READY", "STBD READY"},
        {"MAIN ENG RPM", "GEARBOX OIL", "DG1 VOLTAGE", "DG1 FREQUENCY", "DG1 LOAD", "DG2 LOAD", "BUS VOLTAGE", "BUS FREQUENCY", "FUEL RATE", "COOLANT TEMP", "ACTIVE ALARMS", "LOAD SHARE"},
        {"HEADING", "SPEED OVER GROUND", "ROLL", "PITCH", "NAV STATUS", "FIN STATUS", "PROPULSION", "POWER", "WEATHER", "DEPTH", "MASTER ALARM", "WATCHKEEP"}
    };
    static const char* const values[5][12] = {
        {"ISOLATED", "OPEN", "ISOLATED", "HEALTHY", "STOPPED", "--- A", "STOPPED", "0 h", "READY", "AVAILABLE", "AVAILABLE", "NOT PROVEN"},
        {"NORMAL", "NORMAL", "--- %", "--- %", "--- C", "--- C", "--- C", "--- %", "NORMAL", "NORMAL", "HEALTHY", "CLOSED"},
        {"--- deg", "--- deg", "--- bar", "--- bar", "--- C", "--- C", "PARKED", "PARKED", "--- deg", "--- deg/s", "NO DATA", "NO DATA"},
        {"--- rpm", "--- bar", "--- V", "--- Hz", "--- kW", "--- kW", "--- V", "--- Hz", "--- L/h", "--- C", "0", "STANDBY"},
        {"--- deg", "--- kn", "--- deg", "--- deg", "NO DATA", "PARKED", "NO DATA", "NO DATA", "NO DATA", "--- m", "NORMAL", "ACTIVE"}
    };

    if (activePage == 0U) return;
    const uint8_t index = static_cast<uint8_t>(activePage - 1U);
    copyUtf8(pageTitleBuffer, 48U, titles[index]); copyUtf8(pageSubtitleBuffer, 80U, subtitles[index]);
    for (uint8_t i = 0U; i < 12U; ++i)
    {
        copyUtf8(pageMetricNameBuffer[i], 32U, names[index][i]);
        copyUtf8(pageMetricValueBuffer[i], 32U, values[index][i]);
        pageMetricValues[i].setColor((i >= 10U) ? COLOR_WARNING : COLOR_ACTIVE);
    }
}

bool Screen1View::contains(const touchgfx::Drawable& drawable, int16_t x, int16_t y)
{
    const touchgfx::Rect rect = drawable.getAbsoluteRect();
    return (x >= rect.x) && (y >= rect.y) &&
           (x < (rect.x + rect.width)) && (y < (rect.y + rect.height));
}

touchgfx::colortype Screen1View::qualityColor(E_IO_Quality quality, bool active)
{
    switch (quality)
    {
        case E_IO_QUALITY_GOOD:
            return active ? COLOR_ON : COLOR_IDLE;
        case E_IO_QUALITY_OUT_OF_RANGE:
        case E_IO_QUALITY_STALE:
            return COLOR_WARNING;
        case E_IO_QUALITY_COMM_ERROR:
        case E_IO_QUALITY_CONFIG_ERROR:
            return COLOR_FAULT;
        case E_IO_QUALITY_DISABLED:
        default:
            return COLOR_IDLE;
    }
}

void Screen1View::copyUtf8(touchgfx::Unicode::UnicodeChar* out,
                           uint16_t length,
                           const char* text)
{
    if ((out == 0) || (length == 0U))
    {
        return;
    }
    if (text == 0)
    {
        out[0] = 0U;
        return;
    }
    (void)touchgfx::Unicode::fromUTF8(reinterpret_cast<const uint8_t*>(text), out, length);
}

void Screen1View::formatProcessValue(touchgfx::Unicode::UnicodeChar* out,
                                     uint16_t length,
                                     const S_HMI_SignalView& signal)
{
    uint8_t decimals;
    uint32_t factor;
    float magnitude;
    uint32_t scaled;
    uint32_t whole;
    uint32_t fraction;
    char sign;

    if ((out == 0) || (length == 0U))
    {
        return;
    }
    if (!signal.b_Enabled)
    {
        copyUtf8(out, length, "DISABLED");
        return;
    }
    if (!signal.b_Valid && (signal.e_Quality != E_IO_QUALITY_OUT_OF_RANGE))
    {
        copyUtf8(out, length, "---");
        return;
    }

    decimals = (signal.ui8_DecimalPlaces <= 3U) ? signal.ui8_DecimalPlaces : 3U;
    factor = fn_Pow10(decimals);
    magnitude = signal.f32_ProcessValue;
    sign = '\0';
    if (magnitude < 0.0F)
    {
        sign = '-';
        magnitude = -magnitude;
    }
    else if (signal.b_ShowPlusSign)
    {
        sign = '+';
    }

    scaled = static_cast<uint32_t>((magnitude * static_cast<float>(factor)) + 0.5F);
    whole = scaled / factor;
    fraction = scaled % factor;

    if (decimals == 0U)
    {
        if (sign != '\0') touchgfx::Unicode::snprintf(out, length, "%c%u", sign, static_cast<unsigned int>(whole));
        else touchgfx::Unicode::snprintf(out, length, "%u", static_cast<unsigned int>(whole));
    }
    else if (decimals == 1U)
    {
        if (sign != '\0') touchgfx::Unicode::snprintf(out, length, "%c%u.%01u", sign, static_cast<unsigned int>(whole), static_cast<unsigned int>(fraction));
        else touchgfx::Unicode::snprintf(out, length, "%u.%01u", static_cast<unsigned int>(whole), static_cast<unsigned int>(fraction));
    }
    else if (decimals == 2U)
    {
        if (sign != '\0') touchgfx::Unicode::snprintf(out, length, "%c%u.%02u", sign, static_cast<unsigned int>(whole), static_cast<unsigned int>(fraction));
        else touchgfx::Unicode::snprintf(out, length, "%u.%02u", static_cast<unsigned int>(whole), static_cast<unsigned int>(fraction));
    }
    else
    {
        if (sign != '\0') touchgfx::Unicode::snprintf(out, length, "%c%u.%03u", sign, static_cast<unsigned int>(whole), static_cast<unsigned int>(fraction));
        else touchgfx::Unicode::snprintf(out, length, "%u.%03u", static_cast<unsigned int>(whole), static_cast<unsigned int>(fraction));
    }

    if ((signal.pc_Unit != 0) && (signal.pc_Unit[0] != '\0'))
    {
        fn_AppendAscii(out, length, " ");
        fn_AppendAscii(out, length, signal.pc_Unit);
    }
}

void Screen1View::updateFromFrame(const HmiMainFrame& frame)
{
    for (uint8_t slot = 0U; slot < HMI_ANALOG_SLOT_COUNT; ++slot)
    {
        if (aoCommandPending[slot])
        {
            const float difference = frame.ao[slot].f32_ProcessValue -
                                     static_cast<float>(aoCommandCentiMa[slot]) * 0.01F;
            if (!frame.aoValid[slot] || !frame.ao[slot].b_Valid ||
                (difference > -0.005F && difference < 0.005F) ||
                (++aoPendingFrames[slot] >= 20U))
            {
                aoCommandPending[slot] = false;
                aoPendingFrames[slot] = 0U;
            }
        }
    }
    currentFrame = frame;
    haveFrame = true;
    updateSystemStatus(frame);
    updateUnitSelection(frame);
    updateDigitalSlots(frame);
    updateAnalogSlots(frame);
}

void Screen1View::updateSystemStatus(const HmiMainFrame& frame)
{
    if (!frame.statusValid || (frame.status.e_ConfigStatus != E_IO_CONFIG_OK))
    {
        copyUtf8(tcpValueBuffer, 24U, "CONFIG ERROR");
        tcpValue.setColor(COLOR_FAULT);
        copyUtf8(headerStatusBuffer, 32U, "CONFIG ERROR");
        headerStatus.setColor(COLOR_FAULT);
    }
    else if (frame.status.b_TcpConnected)
    {
        copyUtf8(tcpValueBuffer, 24U, "CONNECTED");
        tcpValue.setColor(COLOR_ACTIVE);
        touchgfx::Unicode::snprintf(headerStatusBuffer, 32U, "LINK OK  |  %u s",
                                   static_cast<unsigned int>(frame.status.ui32_CommunicationUptimeSec));
        headerStatus.setColor(COLOR_ACTIVE);
    }
    else
    {
        copyUtf8(tcpValueBuffer, 24U, "DISCONNECTED");
        tcpValue.setColor(frame.status.b_EthernetLinkUp ? COLOR_WARNING : COLOR_FAULT);
        copyUtf8(headerStatusBuffer, 32U, frame.status.b_EthernetLinkUp ? "TCP OFFLINE" : "ETHERNET OFFLINE");
        headerStatus.setColor(frame.status.b_EthernetLinkUp ? COLOR_WARNING : COLOR_FAULT);
    }
    touchgfx::Unicode::snprintf(uptimeValueBuffer, 24U, "%u s",
                               static_cast<unsigned int>(frame.status.ui32_CommunicationUptimeSec));
    tcpValue.invalidate();
    uptimeValue.invalidate();
    headerStatus.invalidate();
}

void Screen1View::updateUnitSelection(const HmiMainFrame& frame)
{
    copyUtf8(unitNameBuffer[0], 24U, "AI + AO");
    copyUtf8(unitNameBuffer[1], 24U, "UNIT 1");
    copyUtf8(unitDescriptionBuffer, 56U,
             frame.unitValid[0] ? frame.unit[0].pc_Description : "STATION CONFIGURATION ERROR");

    unit1Label.invalidate();
    unit2Label.invalidate();
    unitDescription.invalidate();
    unit1Indicator.invalidate();
    unit2Indicator.invalidate();
}

void Screen1View::updateDigitalSlots(const HmiMainFrame& frame)
{
    touchgfx::TextAreaWithOneWildcard* diNames[HMI_DIGITAL_SLOT_COUNT] = {&diName01, &diName02, &diName03, &diName04, &diName05, &diName06, &diName07, &diName08, &diName09, &diName10, &diName11, &diName12, &diName13, &diName14, &diName15, &diName16};
    touchgfx::TextAreaWithOneWildcard* diValues[HMI_DIGITAL_SLOT_COUNT] = {&diValue01, &diValue02, &diValue03, &diValue04, &diValue05, &diValue06, &diValue07, &diValue08, &diValue09, &diValue10, &diValue11, &diValue12, &diValue13, &diValue14, &diValue15, &diValue16};
    touchgfx::Box* diStates[HMI_DIGITAL_SLOT_COUNT] = {&diState01, &diState02, &diState03, &diState04, &diState05, &diState06, &diState07, &diState08, &diState09, &diState10, &diState11, &diState12, &diState13, &diState14, &diState15, &diState16};
    touchgfx::TextAreaWithOneWildcard* doNames[HMI_DIGITAL_SLOT_COUNT] = {&doName01, &doName02, &doName03, &doName04, &doName05, &doName06, &doName07, &doName08, &doName09, &doName10, &doName11, &doName12, &doName13, &doName14, &doName15, &doName16};
    touchgfx::TextAreaWithOneWildcard* doValues[HMI_DIGITAL_SLOT_COUNT] = {&doValue01, &doValue02, &doValue03, &doValue04, &doValue05, &doValue06, &doValue07, &doValue08, &doValue09, &doValue10, &doValue11, &doValue12, &doValue13, &doValue14, &doValue15, &doValue16};
    touchgfx::Box* doStates[HMI_DIGITAL_SLOT_COUNT] = {&doState01, &doState02, &doState03, &doState04, &doState05, &doState06, &doState07, &doState08, &doState09, &doState10, &doState11, &doState12, &doState13, &doState14, &doState15, &doState16};

    for (uint8_t slot = 0U; slot < HMI_DIGITAL_SLOT_COUNT; ++slot)
    {
        if (frame.diValid[slot])
        {
            touchgfx::Unicode::snprintf(diNameBuffer[slot], 32U, "%02u", static_cast<unsigned int>(slot + 1U));
            if (!frame.di[slot].b_Enabled) copyUtf8(diValueBuffer[slot], 12U, "DISABLED");
            else if (!frame.di[slot].b_Valid) copyUtf8(diValueBuffer[slot], 12U, "---");
            else copyUtf8(diValueBuffer[slot], 12U, frame.di[slot].b_DigitalState ? "ON" : "OFF");
            diStates[slot]->setColor(qualityColor(frame.di[slot].e_Quality,
                                                  frame.di[slot].b_Valid && frame.di[slot].b_DigitalState));
            diValues[slot]->setColor(qualityColor(frame.di[slot].e_Quality,
                                                  frame.di[slot].b_Valid && frame.di[slot].b_DigitalState));
        }
        else
        {
            copyUtf8(diNameBuffer[slot], 32U, "NOT CONFIGURED");
            copyUtf8(diValueBuffer[slot], 12U, "---");
            diStates[slot]->setColor(COLOR_IDLE);
            diValues[slot]->setColor(COLOR_MUTED);
        }

        if (frame.doValid[slot])
        {
            touchgfx::Unicode::snprintf(doNameBuffer[slot], 32U, "%02u", static_cast<unsigned int>(slot + 1U));
            if (!frame.dout[slot].b_Enabled) copyUtf8(doValueBuffer[slot], 12U, "DISABLED");
            else if (!frame.dout[slot].b_Valid) copyUtf8(doValueBuffer[slot], 12U, "---");
            else copyUtf8(doValueBuffer[slot], 12U, frame.dout[slot].b_DigitalState ? "ON" : "OFF");
            doStates[slot]->setColor(qualityColor(frame.dout[slot].e_Quality,
                                                  frame.dout[slot].b_Valid && frame.dout[slot].b_DigitalState));
            doValues[slot]->setColor(qualityColor(frame.dout[slot].e_Quality,
                                                  frame.dout[slot].b_Valid && frame.dout[slot].b_DigitalState));
        }
        else
        {
            copyUtf8(doNameBuffer[slot], 32U, "NOT CONFIGURED");
            copyUtf8(doValueBuffer[slot], 12U, "---");
            doStates[slot]->setColor(COLOR_IDLE);
            doValues[slot]->setColor(COLOR_MUTED);
        }

        diNames[slot]->invalidate(); diValues[slot]->invalidate(); diStates[slot]->invalidate();
        doNames[slot]->invalidate(); doValues[slot]->invalidate(); doStates[slot]->invalidate();
    }
}

void Screen1View::updateAnalogSlots(const HmiMainFrame& frame)
{
    touchgfx::TextAreaWithOneWildcard* names[HMI_ANALOG_SLOT_COUNT] = {&analogName01, &analogName02, &analogName03, &analogName04};
    touchgfx::TextAreaWithOneWildcard* values[HMI_ANALOG_SLOT_COUNT] = {&analogValue01, &analogValue02, &analogValue03, &analogValue04};
    touchgfx::Button* minusButtons[HMI_ANALOG_SLOT_COUNT] = {&aoMinus01, &aoMinus02, &aoMinus03, &aoMinus04};
    touchgfx::Button* plusButtons[HMI_ANALOG_SLOT_COUNT] = {&aoPlus01, &aoPlus02, &aoPlus03, &aoPlus04};
    touchgfx::TextArea* minusLabels[HMI_ANALOG_SLOT_COUNT] = {&aoMinusLabel01, &aoMinusLabel02, &aoMinusLabel03, &aoMinusLabel04};
    touchgfx::TextArea* plusLabels[HMI_ANALOG_SLOT_COUNT] = {&aoPlusLabel01, &aoPlusLabel02, &aoPlusLabel03, &aoPlusLabel04};

    copyUtf8(analogTitleBuffer, 32U, "ANALOG I/O");
    analogTitle.invalidate();

    for (uint8_t slot = 0U; slot < HMI_ANALOG_SLOT_COUNT; ++slot)
    {
        if (frame.aiValid[slot])
        {
            copyUtf8(analogNameBuffer[slot], 38U, frame.ai[slot].pc_Name);
            formatProcessValue(analogValueBuffer[slot], 32U, frame.ai[slot]);
            values[slot]->setColor(qualityColor(frame.ai[slot].e_Quality, frame.ai[slot].b_Valid));
            float normalized = (frame.ai[slot].f32_ProcessValue + 10.0F) / 20.0F;
            if (normalized < 0.0F) normalized = 0.0F; else if (normalized > 1.0F) normalized = 1.0F;
            const int16_t fill = static_cast<int16_t>(normalized * 68.0F);
            aiFills[slot]->setPosition(static_cast<int16_t>(443 + (slot * 94)), static_cast<int16_t>(220 - fill), 18, fill);
            aiFills[slot]->setColor(qualityColor(frame.ai[slot].e_Quality, frame.ai[slot].b_Valid));
        }
        else
        {
            copyUtf8(analogNameBuffer[slot], 38U, "NOT CONFIGURED");
            copyUtf8(analogValueBuffer[slot], 32U, "---");
            values[slot]->setColor(COLOR_MUTED);
            aiFills[slot]->setPosition(static_cast<int16_t>(443 + (slot * 94)), 220, 18, 0);
        }

        if (frame.aoValid[slot])
        {
            copyUtf8(aoNameBuffer[slot], 38U, frame.ao[slot].pc_Name);
            formatProcessValue(aoValueBuffer[slot], 32U, frame.ao[slot]);
            aoValues[slot]->setColor(qualityColor(frame.ao[slot].e_Quality, frame.ao[slot].b_Valid));
            float normalized = (frame.ao[slot].f32_ProcessValue + 20.0F) / 40.0F;
            if (normalized < 0.0F) normalized = 0.0F; else if (normalized > 1.0F) normalized = 1.0F;
            const int16_t fill = static_cast<int16_t>(normalized * 56.0F);
            aoFills[slot]->setPosition(static_cast<int16_t>(443 + (slot * 94)), static_cast<int16_t>(404 - fill), 18, fill);
            aoFills[slot]->setColor(qualityColor(frame.ao[slot].e_Quality, frame.ao[slot].b_Valid));
        }
        else
        {
            copyUtf8(aoNameBuffer[slot], 38U, "AO"); copyUtf8(aoValueBuffer[slot], 32U, "---");
            aoValues[slot]->setColor(COLOR_MUTED);
            aoFills[slot]->setPosition(static_cast<int16_t>(443 + (slot * 94)), 404, 18, 0);
        }

        const bool showControl = (activePage == 0U) && frame.aoValid[slot] &&
                                 frame.ao[slot].b_Valid && frame.ao[slot].b_Enabled &&
                                 frame.ao[slot].b_Writable;
        minusButtons[slot]->setVisible(showControl);
        plusButtons[slot]->setVisible(showControl);
        minusLabels[slot]->setVisible(showControl);
        plusLabels[slot]->setVisible(showControl);

        names[slot]->invalidate(); values[slot]->invalidate();
        minusButtons[slot]->invalidate(); plusButtons[slot]->invalidate();
        minusLabels[slot]->invalidate(); plusLabels[slot]->invalidate();
        aoNames[slot]->invalidate(); aoValues[slot]->invalidate(); aiFills[slot]->invalidate(); aoFills[slot]->invalidate();
    }
}

void Screen1View::adjustAnalogSlot(uint8_t slot, float direction)
{
    if (!haveFrame || (presenter == 0) || (activePage != 0U) ||
        (slot >= HMI_ANALOG_SLOT_COUNT) || !currentFrame.aoValid[slot] ||
        !currentFrame.ao[slot].b_Valid || !currentFrame.ao[slot].b_Enabled ||
        !currentFrame.ao[slot].b_Writable)
    {
        return;
    }

    const float measured = currentFrame.ao[slot].f32_ProcessValue;
    const int32_t measuredCentiMa = static_cast<int32_t>(measured * 100.0F +
                                                        ((measured >= 0.0F) ? 0.5F : -0.5F));
    const int32_t baseline = aoCommandPending[slot] ? aoCommandCentiMa[slot] : measuredCentiMa;
    const int32_t delta = aoFineStep ? 1 : 10;
    const int32_t requested = baseline + ((direction > 0.0F) ? delta : -delta);
    /* The configured AO operating range is -20.00 to +20.00 mA. Never
     * silently clamp a rejected actuator command at the GUI boundary. */
    if ((requested < -2000) || (requested > 2000))
    {
        return;
    }
    if (presenter->requestAnalog(currentFrame.ao[slot].ui16_SignalId,
                                 static_cast<float>(requested) * 0.01F) == E_IO_STATUS_OK)
    {
        aoCommandCentiMa[slot] = requested;
        aoCommandPending[slot] = true;
        aoPendingFrames[slot] = 0U;
    }
}

void Screen1View::updateAoStepLabel()
{
    copyUtf8(aoStepLabelBuffer, 24U, aoFineStep ? "FINE  0.01 mA" : "COARSE  0.10 mA");
    aoStepLabel.invalidate();
}

void Screen1View::toggleAoStep()
{
    aoFineStep = !aoFineStep;
    updateAoStepLabel();
}

void Screen1View::selectAnalogInputs()
{
    showPage(0U);
}

void Screen1View::selectAnalogOutputs()
{
    showPage(0U);
}

void Screen1View::adjustAoMinus01() { adjustAnalogSlot(0U, -1.0F); }
void Screen1View::adjustAoPlus01()  { adjustAnalogSlot(0U, +1.0F); }
void Screen1View::adjustAoMinus02() { adjustAnalogSlot(1U, -1.0F); }
void Screen1View::adjustAoPlus02()  { adjustAnalogSlot(1U, +1.0F); }
void Screen1View::adjustAoMinus03() { adjustAnalogSlot(2U, -1.0F); }
void Screen1View::adjustAoPlus03()  { adjustAnalogSlot(2U, +1.0F); }
void Screen1View::adjustAoMinus04() { adjustAnalogSlot(3U, -1.0F); }
void Screen1View::adjustAoPlus04()  { adjustAnalogSlot(3U, +1.0F); }

void Screen1View::handleClickEvent(const touchgfx::ClickEvent& event)
{
    Screen1ViewBase::handleClickEvent(event);
    if (event.getType() != touchgfx::ClickEvent::RELEASED)
    {
        return;
    }

    const int16_t x = event.getX();
    const int16_t y = event.getY();
    for (uint8_t page = 0U; page < 6U; ++page)
    {
        if (contains(*navTiles[page], x, y)) { showPage(page); return; }
    }
    if (!haveFrame || (presenter == 0) || (activePage != 0U)) return;
    const touchgfx::Drawable* doStates[HMI_DIGITAL_SLOT_COUNT] = {&doState01, &doState02, &doState03, &doState04, &doState05, &doState06, &doState07, &doState08, &doState09, &doState10, &doState11, &doState12, &doState13, &doState14, &doState15, &doState16};

    for (uint8_t slot = 0U; slot < HMI_DIGITAL_SLOT_COUNT; ++slot)
    {
        if (contains(*doStates[slot], x, y) && currentFrame.doValid[slot] &&
            currentFrame.dout[slot].b_Enabled && currentFrame.dout[slot].b_Writable)
        {
            (void)presenter->requestDigital(currentFrame.dout[slot].ui16_SignalId,
                                            !currentFrame.dout[slot].b_DigitalState);
            return;
        }
    }
}
