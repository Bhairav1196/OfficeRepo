#ifndef MODEL_LISTENER_HPP
#define MODEL_LISTENER_HPP

#include <gui/model/Model.hpp>

/**
 * @file    ModelListener.hpp
 * @brief   Listener contract for configuration-driven HMI frame updates.
 * @author  Prathamesh Chipkar
 * @date    09-Sep-2026
 * @version 2.0.0-alpha.9.0
 */

class ModelListener
{
public:
    ModelListener() : model(0) {}
    virtual ~ModelListener() {}
    void bind(Model* m) { model = m; }
    virtual void hmiFrameChanged(const HmiMainFrame&) {}

protected:
    Model* model;
};

#endif /* MODEL_LISTENER_HPP */
