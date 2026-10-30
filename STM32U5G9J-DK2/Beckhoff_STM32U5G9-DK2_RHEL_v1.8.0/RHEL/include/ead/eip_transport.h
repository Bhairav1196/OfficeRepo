/**
 * @file eip_transport.h
 * @brief Native EtherNet/IP explicit and Class 1 I/O transport API for an EK9500 route.
 * @author Prathamesh Chipkar
 */
#ifndef EAD_EIP_TRANSPORT_H
#define EAD_EIP_TRANSPORT_H

#include "ead/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque native EtherNet/IP transport context owning TCP/UDP session state. */
typedef struct ead_eip_transport ead_eip_transport_t;
typedef void (*ead_eip_input_callback_t)(const uint8_t *data,
                                         size_t size,
                                         uint32_t sequence,
                                         void *context);

/** @brief Allocate and initialize one EtherNet/IP transport context. */
ead_eip_transport_t *fn_EadEipCreate(void);
/** @brief Release an EtherNet/IP transport context and owned socket resources. */
void fn_EadEipDestroy(ead_eip_transport_t *transport);
/** @brief Open TCP encapsulation and register an EtherNet/IP session with the EK9500. */
ead_result_t fn_EadEipConnect(ead_eip_transport_t *transport,
                              const ead_scanner_config_t *config,
                              size_t input_size,
                              size_t output_size,
                              ead_eip_input_callback_t callback,
                              void *callback_context,
                              char *error,
                              size_t error_size);
/** @brief Establish the Class 1 I/O connection using the configured assemblies and RPI. */
ead_result_t fn_EadEipOpenIo(ead_eip_transport_t *transport,
                             const uint8_t *initial_output,
                             size_t size,
                             char *error,
                             size_t error_size);
/** @brief Close the active Class 1 I/O connection while retaining the encapsulation session. */
ead_result_t fn_EadEipCloseIo(ead_eip_transport_t *transport,
                              char *error,
                              size_t error_size);
/** @brief Replace the pending O-to-T process image sent by cyclic I/O. */
ead_result_t fn_EadEipSetOutput(ead_eip_transport_t *transport,
                                const uint8_t *data,
                                size_t size);
/** @brief Service one transport cycle, receive T-to-O data and transmit due O-to-T data. */
ead_result_t fn_EadEipPoll(ead_eip_transport_t *transport,
                           uint32_t timeout_ms,
                           char *error,
                           size_t error_size);
/** @brief Read a CIP Assembly instance through an explicit message. */
ead_result_t fn_EadEipReadAssembly(ead_eip_transport_t *transport,
                                   uint16_t instance,
                                   uint8_t *data,
                                   size_t capacity,
                                   size_t *size,
                                   char *error,
                                   size_t error_size);
/** @brief Write a CIP Assembly instance through an explicit message. */
ead_result_t fn_EadEipWriteAssembly(ead_eip_transport_t *transport,
                                    uint16_t instance,
                                    const uint8_t *data,
                                    size_t size,
                                    char *error,
                                    size_t error_size);
/** @brief Report whether the Class 1 I/O connection is currently established. */
bool fn_EadEipIsIoOpen(const ead_eip_transport_t *transport);
/** @brief Close I/O, unregister the session and close all transport sockets. */
void fn_EadEipDisconnect(ead_eip_transport_t *transport);

#ifdef __cplusplus
}
#endif

#endif
