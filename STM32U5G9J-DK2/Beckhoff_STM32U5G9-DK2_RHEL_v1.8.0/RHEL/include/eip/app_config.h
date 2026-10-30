/**
 * @file app_config.h
 * @brief Runtime network parameters consumed by the EtherNet/IP transport layer.
 * @author Prathamesh Chipkar
 */
#ifndef EIP_APP_CONFIG_H
#define EIP_APP_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Network and cyclic-I/O settings required by the EtherNet/IP layer. */
typedef struct
{
    char target_ip[64];
    char local_ip[64];
    uint16_t target_udp_port;
    uint16_t local_udp_port;
    uint16_t config_assembly;
    uint16_t input_assembly;
    uint16_t output_assembly;
    uint32_t o2t_rpi_us;
    uint32_t t2o_rpi_us;
    uint8_t timeout_multiplier;
    uint16_t originator_vendor_id;
    uint32_t originator_serial;
    bool o2t_run_idle_header;
    bool t2o_run_idle_header;
} app_config_t;

#endif
