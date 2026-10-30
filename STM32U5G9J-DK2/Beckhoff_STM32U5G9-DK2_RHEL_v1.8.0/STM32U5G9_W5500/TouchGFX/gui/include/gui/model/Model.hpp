#ifndef MODEL_HPP
#define MODEL_HPP

#include "hmi_service.h"

/**
 * @file    Model.hpp
 * @brief   TouchGFX model for one Beckhoff bus coupler with simultaneous AI/AO data.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#define HMI_UNIT_COUNT           (1U)
#define HMI_DIGITAL_SLOT_COUNT   (16U)
#define HMI_ANALOG_SLOT_COUNT    (4U)

/** @brief Complete operator frame for the currently selected Beckhoff unit. */
struct HmiMainFrame
{
    S_HMI_SystemStatus status;
    S_HMI_UnitInfo unit[HMI_UNIT_COUNT];
    S_HMI_SignalView di[HMI_DIGITAL_SLOT_COUNT];
    S_HMI_SignalView dout[HMI_DIGITAL_SLOT_COUNT];
    S_HMI_SignalView ai[HMI_ANALOG_SLOT_COUNT];
    S_HMI_SignalView ao[HMI_ANALOG_SLOT_COUNT];
    bool unitValid[HMI_UNIT_COUNT];
    bool diValid[HMI_DIGITAL_SLOT_COUNT];
    bool doValid[HMI_DIGITAL_SLOT_COUNT];
    bool aiValid[HMI_ANALOG_SLOT_COUNT];
    bool aoValid[HMI_ANALOG_SLOT_COUNT];
    bool statusValid;
    uint8_t activeUnitId;
};

class ModelListener;

class Model
{
public:
    Model();
    void bind(ModelListener* listener);
    void tick();
    bool getFrame(HmiMainFrame& out) const;
    E_IO_Status requestDigital(uint16_t signalId, bool state);
    E_IO_Status requestAnalog(uint16_t signalId, float processValue);

private:
    void updateFrame();

    ModelListener* modelListener;
    HmiMainFrame frame;
    bool haveFrame;
};

#endif /* MODEL_HPP */
