/**
 * @file    Screen1Presenter.cpp
 * @brief   Presenter implementation for the single-coupler Beckhoff operator HMI.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#include <gui/screen1_screen/Screen1Presenter.hpp>
#include <gui/screen1_screen/Screen1View.hpp>
#include <gui/model/Model.hpp>

Screen1Presenter::Screen1Presenter(Screen1View& v) : view(v) {}

void Screen1Presenter::activate()
{
    if (model != 0)
    {
        HmiMainFrame frame;
        if (model->getFrame(frame))
        {
            view.updateFromFrame(frame);
        }
    }
}

void Screen1Presenter::deactivate() {}

void Screen1Presenter::hmiFrameChanged(const HmiMainFrame& frame)
{
    view.updateFromFrame(frame);
}

E_IO_Status Screen1Presenter::requestDigital(uint16_t signalId, bool state)
{
    return (model != 0) ? model->requestDigital(signalId, state) : E_IO_STATUS_CONFIG_ERROR;
}

E_IO_Status Screen1Presenter::requestAnalog(uint16_t signalId, float processValue)
{
    return (model != 0) ? model->requestAnalog(signalId, processValue) : E_IO_STATUS_CONFIG_ERROR;
}
