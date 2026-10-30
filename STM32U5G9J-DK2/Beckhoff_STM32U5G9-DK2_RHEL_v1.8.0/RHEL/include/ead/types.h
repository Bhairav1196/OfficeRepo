/**
 * @file types.h
 * @brief Core result, terminal, layout, process-image and analog sample data types.
 * @author Prathamesh Chipkar
 */
#ifndef EAD_TYPES_H
#define EAD_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EAD_VERSION
#define EAD_VERSION "1.5.0"
#endif
/** Maximum EK9500 terminal positions addressable through classes 0x301..0x3FF. */
#define EAD_MAX_TERMINALS (255U)
#define EAD_NAME_LENGTH (32U)
#define EAD_PATH_LENGTH (256U)
#define EAD_IP_LENGTH (64U)
#define EAD_ERROR_LENGTH (256U)
/** Maximum EK9500 Input or Output Assembly size, including CtrlStatus. */
#define EAD_MAX_ASSEMBLY_SIZE (496U)
#define EAD_MAX_RESPONSE_SIZE (8192U)

/** @brief Core EK9500 gateway enumeration used by configuration, layout, or I/O access. */
typedef enum
{
    EAD_OK = 0,
    EAD_ERR_ARGUMENT = -1,
    EAD_ERR_CONFIG = -2,
    EAD_ERR_RANGE = -3,
    EAD_ERR_IO = -4,
    EAD_ERR_PROTOCOL = -5,
    EAD_ERR_TIMEOUT = -6,
    EAD_ERR_STATE = -7,
    EAD_ERR_NOT_FOUND = -8,
    EAD_ERR_REMOTE = -9
} ead_result_t;

/** @brief Core EK9500 gateway enumeration used by configuration, layout, or I/O access. */
typedef enum
{
    EAD_TERMINAL_EL1409 = 0,
    EAD_TERMINAL_EL2409,
    EAD_TERMINAL_EL3074,
    EAD_TERMINAL_EL4074,
    /** Terminal transported as raw bytes because no typed codec is present. */
    EAD_TERMINAL_RAW
} ead_terminal_kind_t;

/** @brief Core EK9500 gateway enumeration used by configuration, layout, or I/O access. */
typedef enum
{
    EAD_DOMAIN_DI = 0,
    EAD_DOMAIN_DO,
    EAD_DOMAIN_AI,
    EAD_DOMAIN_AO,
    /** Raw process data excluded from typed logical-channel APIs. */
    EAD_DOMAIN_RAW
} ead_address_domain_t;

/** @brief Core EK9500 gateway enumeration used by configuration, layout, or I/O access. */
typedef enum
{
    EAD_OFFSET_AUTO = 0,
    EAD_OFFSET_EXPLICIT
} ead_offset_mode_t;

/** @brief Core EK9500 gateway enumeration used by configuration, layout, or I/O access. */
typedef enum
{
    EAD_PDO_BIT_PACKED = 0,
    EAD_PDO_COMPACT_INT16,
    EAD_PDO_STATUS_VALUE_INT16,
    EAD_PDO_CONTROL_VALUE_INT16,
    EAD_PDO_REAL32,
    /** Opaque bytes retained for safe transport and future codec extension. */
    EAD_PDO_RAW
} ead_pdo_layout_t;

/** @brief Core EK9500 gateway enumeration used by configuration, layout, or I/O access. */
typedef enum
{
    EAD_ANALOG_BIPOLAR_10V = 0,
    EAD_ANALOG_UNIPOLAR_10V,
    EAD_ANALOG_BIPOLAR_20MA,
    EAD_ANALOG_UNIPOLAR_20MA,
    EAD_ANALOG_4_TO_20MA
} ead_analog_mode_t;

/** @brief Core EK9500 gateway enumeration used by configuration, layout, or I/O access. */
typedef enum
{
    EAD_OWNERSHIP_RUNNING = 0,
    EAD_OWNERSHIP_RELEASED,
    EAD_OWNERSHIP_FAULTED
} ead_ownership_mode_t;

/** @brief Core EK9500 gateway data structure used by configuration, layout, or I/O access. */
typedef struct
{
    char profile_name[EAD_NAME_LENGTH];
    char backend[32];
    char adapter_ip[EAD_IP_LENGTH];
    char local_ip[EAD_IP_LENGTH];
    uint16_t tcp_port;
    uint16_t target_udp_port;
    uint16_t local_udp_port;
    uint16_t config_assembly;
    uint16_t input_assembly;
    uint16_t output_assembly;

    size_t input_header_bytes;
    size_t output_header_bytes;
    size_t input_tail_padding_bytes;
    size_t output_tail_padding_bytes;
    size_t input_assembly_size;
    size_t output_assembly_size;

    uint32_t o2t_rpi_us;
    uint32_t t2o_rpi_us;
    uint32_t service_cycle_ms;
    uint32_t input_stale_timeout_ms;
    bool o2t_run_idle_header;
    bool t2o_run_idle_header;

    bool reconnect_enabled;
    uint32_t reconnect_delay_ms;
    uint32_t max_reconnect_attempts;
    uint8_t timeout_multiplier;
    uint16_t originator_vendor_id;
    uint32_t originator_serial_number;

    char socket_path[EAD_PATH_LENGTH];
    char output_state_file[EAD_PATH_LENGTH];
    bool preserve_device_outputs;
    bool persist_outputs;
    bool strict_assembly_size;
    bool console_color;
    uint32_t periodic_status_ms;
    uint32_t released_output_poll_ms;
    bool verify_explicit_output_writes;
} ead_scanner_config_t;

/** @brief Core EK9500 gateway data structure used by configuration, layout, or I/O access. */
typedef struct
{
    char name[EAD_NAME_LENGTH];
    ead_terminal_kind_t kind;
    size_t position;
    bool enabled;

    ead_offset_mode_t input_offset_mode;
    ead_offset_mode_t output_offset_mode;
    size_t input_offset;
    size_t output_offset;
    size_t input_bytes_override;
    size_t output_bytes_override;

    size_t logical_base;
    uint32_t refresh_ms;
    ead_pdo_layout_t pdo_layout;
    ead_analog_mode_t analog_mode;
    double engineering_min;
    double engineering_max;
    int32_t full_scale_raw;
    bool allow_extended_range;
    size_t channel_stride;
    size_t status_stride;
    size_t status_offset;
    size_t value_offset;
} ead_terminal_config_t;

/** @brief Core EK9500 gateway data structure used by configuration, layout, or I/O access. */
typedef struct
{
    ead_scanner_config_t scanner;
    ead_terminal_config_t terminals[EAD_MAX_TERMINALS];
    size_t terminal_count;
} ead_system_config_t;

/** @brief Core EK9500 gateway data structure used by configuration, layout, or I/O access. */
typedef struct
{
    ead_terminal_kind_t kind;
    ead_address_domain_t domain;
    const char *model;
    size_t channels;
    size_t default_input_bytes;
    size_t default_output_bytes;
    ead_pdo_layout_t default_layout;
} ead_terminal_descriptor_t;

/** @brief Core EK9500 gateway data structure used by configuration, layout, or I/O access. */
typedef struct
{
    ead_terminal_config_t config;
    ead_terminal_descriptor_t descriptor;
    size_t input_offset;
    size_t output_offset;
    size_t input_bytes;
    size_t output_bytes;
} ead_terminal_address_t;

/** @brief Core EK9500 gateway data structure used by configuration, layout, or I/O access. */
typedef struct
{
    ead_terminal_address_t terminals[EAD_MAX_TERMINALS];
    size_t terminal_count;
    size_t mapped_input_end;
    size_t mapped_output_end;
    size_t input_size;
    size_t output_size;
} ead_layout_t;

/** @brief Core EK9500 gateway data structure used by configuration, layout, or I/O access. */
typedef struct
{
    int16_t raw;
    uint16_t status;
    double engineering;
} ead_analog_sample_t;

/** @brief Return a stable diagnostic string for a project result code. */
const char *fn_EadResultText(ead_result_t result);
/** @brief Return the display name of a supported terminal kind. */
const char *fn_EadTerminalKindText(ead_terminal_kind_t kind);
/** @brief Return the display name of an input/output domain. */
const char *fn_EadDomainText(ead_address_domain_t domain);
/** @brief Return the display name of an output-ownership state. */
const char *fn_EadOwnershipText(ead_ownership_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif
