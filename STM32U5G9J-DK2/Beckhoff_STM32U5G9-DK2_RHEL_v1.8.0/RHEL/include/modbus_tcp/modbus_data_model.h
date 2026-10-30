/**
 * @file modbus_data_model.h
 * @brief Binds Modbus tables to the dual-EK9500 route manager.
 * @author Prathamesh Chipkar
 */
#ifndef MODB_DATA_MODEL_H
#define MODB_DATA_MODEL_H

#include "gateway/route_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Production Modbus data model context. */
typedef struct
{
    S_GW_ROUTE_MANAGER *pst_RouteManager;
} S_MODB_DATA_MODEL;

/** @brief Initialize the Modbus table binding. */
E_GW_RESULT fn_MODB_DataModelInitialize(S_MODB_DATA_MODEL *pst_Model,
                                        S_GW_ROUTE_MANAGER *pst_RouteManager);

#ifdef __cplusplus
}
#endif

#endif
