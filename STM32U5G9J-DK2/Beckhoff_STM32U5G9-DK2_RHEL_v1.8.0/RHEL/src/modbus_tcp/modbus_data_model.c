/**
 * @file modbus_data_model.c
 * @brief Initialization of the production Modbus-to-EK9500 data model.
 * @author Prathamesh Chipkar
 */
#include "modbus_tcp/modbus_data_model.h"

E_GW_RESULT fn_MODB_DataModelInitialize(S_MODB_DATA_MODEL *pst_Model,
                                        S_GW_ROUTE_MANAGER *pst_RouteManager)
{
    if ((pst_Model == NULL) || (pst_RouteManager == NULL))
    {
        return GW_ERR_ARGUMENT;
    }
    pst_Model->pst_RouteManager = pst_RouteManager;
    return GW_OK;
}
