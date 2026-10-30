/**
 * @file    Model.cpp
 * @brief   TouchGFX model polling the single-coupler target/simulator HMI abstraction.
 * @author  Prathamesh Chipkar
 * @date    10-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

#include <gui/model/Model.hpp>
#include <gui/model/ModelListener.hpp>
#include <cstring>

Model::Model()
    : modelListener(0), haveFrame(false)
{
    std::memset(&frame, 0, sizeof(frame));
}

void Model::bind(ModelListener* listener)
{
    modelListener = listener;
    if (modelListener != 0)
    {
        modelListener->bind(this);
    }
}

void Model::updateFrame()
{
    HmiMainFrame next;
    std::memset(&next, 0, sizeof(next));

    next.activeUnitId = 1U;
    next.statusValid = fn_HMI_GetSystemStatus(&next.status);
    for (uint8_t index = 0U; index < HMI_UNIT_COUNT; ++index)
    {
        next.unitValid[index] = fn_HMI_GetUnitInfo(1U, &next.unit[index]);
    }

    const S_HMI_UnitInfo* activeInfo = 0;
    for (uint8_t index = 0U; index < HMI_UNIT_COUNT; ++index)
    {
        if (next.unitValid[index] && (next.unit[index].ui8_UnitId == 1U))
        {
            activeInfo = &next.unit[index];
            break;
        }
    }

    for (uint8_t slot = 0U; slot < HMI_DIGITAL_SLOT_COUNT; ++slot)
    {
        next.diValid[slot] = fn_HMI_GetUnitSlot(1U, E_IO_SIGNAL_DI, slot, &next.di[slot]);
        next.doValid[slot] = fn_HMI_GetUnitSlot(1U, E_IO_SIGNAL_DO, slot, &next.dout[slot]);
    }

    for (uint8_t slot = 0U; slot < HMI_ANALOG_SLOT_COUNT; ++slot)
    {
        next.aiValid[slot] = (activeInfo != 0) &&
            fn_HMI_GetUnitSlot(1U, E_IO_SIGNAL_AI, slot, &next.ai[slot]);
        next.aoValid[slot] = (activeInfo != 0) &&
            fn_HMI_GetUnitSlot(1U, E_IO_SIGNAL_AO, slot, &next.ao[slot]);
    }

    frame = next;
    haveFrame = true;
}

void Model::tick()
{
    updateFrame();
    if (modelListener != 0)
    {
        modelListener->hmiFrameChanged(frame);
    }
}

bool Model::getFrame(HmiMainFrame& out) const
{
    if (!haveFrame)
    {
        return false;
    }
    out = frame;
    return true;
}

E_IO_Status Model::requestDigital(uint16_t signalId, bool state)
{
    return fn_HMI_RequestDigital(signalId, state);
}

E_IO_Status Model::requestAnalog(uint16_t signalId, float processValue)
{
    return fn_HMI_RequestAnalog(signalId, processValue);
}
