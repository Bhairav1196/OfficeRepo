/** @file cip_connection.h
 * @author Prathamesh Chipkar
 *  @brief CIP Connection Manager Forward Open/Close operations.
 */
#ifndef CIP_CONNECTION_H
#define CIP_CONNECTION_H

#include <stddef.h>
#include <stdint.h>

#include "app_config.h"
#include "eip_types.h"

/** @brief Open a point-to-point fixed-size Class 1 I/O connection. */
eip_result_t fn_CipForwardOpen(eip_session_t *session,
                               const app_config_t *config,
                               uint16_t o2t_application_size,
                               uint16_t t2o_application_size,
                               eip_io_connection_t *connection);

/** @brief Close a previously accepted Class 1 I/O connection. */
eip_result_t fn_CipForwardClose(eip_session_t *session,
                                const eip_io_connection_t *connection);

#endif
