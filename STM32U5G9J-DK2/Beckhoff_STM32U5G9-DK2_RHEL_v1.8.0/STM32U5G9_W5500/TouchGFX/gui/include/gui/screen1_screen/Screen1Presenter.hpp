#ifndef SCREEN1_PRESENTER_HPP
#define SCREEN1_PRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

/**
 * @file    Screen1Presenter.hpp
 * @brief   Presenter for the single-coupler Beckhoff operator HMI.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

class Screen1View;

class Screen1Presenter : public touchgfx::Presenter, public ModelListener
{
public:
    explicit Screen1Presenter(Screen1View& v);
    virtual ~Screen1Presenter() {}
    virtual void activate();
    virtual void deactivate();
    virtual void hmiFrameChanged(const HmiMainFrame& frame);
    E_IO_Status requestDigital(uint16_t signalId, bool state);
    E_IO_Status requestAnalog(uint16_t signalId, float processValue);

private:
    Screen1Presenter();
    Screen1View& view;
};

#endif /* SCREEN1_PRESENTER_HPP */
