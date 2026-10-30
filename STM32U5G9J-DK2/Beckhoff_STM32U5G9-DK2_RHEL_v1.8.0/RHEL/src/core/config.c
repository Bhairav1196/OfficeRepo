/**
 * @file config.c
 * @brief Strict terminal-train configuration parser and validation implementation.
 * @author Prathamesh Chipkar
 */
#define _POSIX_C_SOURCE 200809L
#include "ead/config.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define EAD_CONFIG_MAX_SECTIONS (EAD_MAX_TERMINALS + 1U)
#define EAD_CONFIG_MAX_KEYS     (64U)
#define EAD_CONFIG_KEY_LENGTH   (64U)
#define EAD_CONFIG_VALUE_LENGTH (256U)

typedef struct
{
    char key[EAD_CONFIG_KEY_LENGTH];
    char value[EAD_CONFIG_VALUE_LENGTH];
} ead_config_pair_t;

typedef struct
{
    char name[EAD_NAME_LENGTH + 16U];
    ead_config_pair_t pairs[EAD_CONFIG_MAX_KEYS];
    size_t pair_count;
} ead_config_section_t;

static void fn_SetError(char *error, size_t error_size, const char *text)
{
    if ((error != NULL) && (error_size > 0U))
    {
        (void)snprintf(error, error_size, "%s", (text != NULL) ? text : "configuration error");
    }
}

static char *fn_Trim(char *text)
{
    char *end;
    while ((*text != '\0') && isspace((unsigned char)*text)) ++text;
    end = text + strlen(text);
    while ((end > text) && isspace((unsigned char)end[-1])) --end;
    *end = '\0';
    return text;
}

static void fn_Lower(char *text)
{
    while (*text != '\0')
    {
        *text = (char)tolower((unsigned char)*text);
        ++text;
    }
}

static void fn_RemoveBlockComments(char *text, bool *in_block_comment)
{
    char *read_cursor;
    char *write_cursor;

    if ((text == NULL) || (in_block_comment == NULL)) return;
    read_cursor = text;
    write_cursor = text;
    while (*read_cursor != '\0')
    {
        if (*in_block_comment)
        {
            if ((read_cursor[0] == '*') && (read_cursor[1] == '/'))
            {
                *in_block_comment = false;
                read_cursor += 2;
            }
            else
            {
                ++read_cursor;
            }
        }
        else if ((read_cursor[0] == '/') && (read_cursor[1] == '*'))
        {
            *in_block_comment = true;
            read_cursor += 2;
        }
        else
        {
            *write_cursor = *read_cursor;
            ++write_cursor;
            ++read_cursor;
        }
    }
    *write_cursor = '\0';
}

static bool fn_ParseBool(const char *text, bool *value)
{
    if ((text == NULL) || (value == NULL)) return false;
    if ((strcasecmp(text, "true") == 0) || (strcasecmp(text, "yes") == 0) ||
        (strcasecmp(text, "on") == 0) || (strcmp(text, "1") == 0))
    {
        *value = true;
        return true;
    }
    if ((strcasecmp(text, "false") == 0) || (strcasecmp(text, "no") == 0) ||
        (strcasecmp(text, "off") == 0) || (strcmp(text, "0") == 0))
    {
        *value = false;
        return true;
    }
    return false;
}

static bool fn_ParseUnsigned(const char *text, unsigned long long *value)
{
    char *end = NULL;
    unsigned long long parsed;
    if ((text == NULL) || (value == NULL)) return false;
    errno = 0;
    parsed = strtoull(text, &end, 0);
    if ((errno != 0) || (end == text)) return false;
    while ((*end != '\0') && isspace((unsigned char)*end)) ++end;
    if (*end != '\0') return false;
    *value = parsed;
    return true;
}

static bool fn_ParseSigned(const char *text, long long *value)
{
    char *end = NULL;
    long long parsed;
    if ((text == NULL) || (value == NULL)) return false;
    errno = 0;
    parsed = strtoll(text, &end, 0);
    if ((errno != 0) || (end == text)) return false;
    while ((*end != '\0') && isspace((unsigned char)*end)) ++end;
    if (*end != '\0') return false;
    *value = parsed;
    return true;
}

static bool fn_ParseDouble(const char *text, double *value)
{
    char *end = NULL;
    double parsed;
    if ((text == NULL) || (value == NULL)) return false;
    errno = 0;
    parsed = strtod(text, &end);
    if ((errno != 0) || (end == text)) return false;
    while ((*end != '\0') && isspace((unsigned char)*end)) ++end;
    if (*end != '\0') return false;
    *value = parsed;
    return true;
}

static bool fn_IsAllowed(const char *key, const char *const *allowed, size_t count)
{
    size_t index;
    for (index = 0U; index < count; ++index)
    {
        if (strcmp(key, allowed[index]) == 0) return true;
    }
    return false;
}

static ead_result_t fn_ValidateKeys(const ead_config_section_t *section,
                                    const char *const *allowed,
                                    size_t allowed_count,
                                    char *error,
                                    size_t error_size)
{
    size_t index;
    for (index = 0U; index < section->pair_count; ++index)
    {
        if (!fn_IsAllowed(section->pairs[index].key, allowed, allowed_count))
        {
            if ((error != NULL) && (error_size > 0U))
            {
                (void)snprintf(error, error_size, "[%s]: unknown key '%s'",
                               section->name, section->pairs[index].key);
            }
            return EAD_ERR_CONFIG;
        }
    }
    return EAD_OK;
}

void fn_EadConfigSetDefaults(ead_system_config_t *config)
{
    if (config == NULL) return;
    (void)memset(config, 0, sizeof(*config));
    (void)snprintf(config->scanner.profile_name,
                   sizeof(config->scanner.profile_name), "CUSTOM");
    (void)snprintf(config->scanner.backend, sizeof(config->scanner.backend), "native_c");
    (void)snprintf(config->scanner.adapter_ip, sizeof(config->scanner.adapter_ip), "192.168.1.1");
    (void)snprintf(config->scanner.local_ip, sizeof(config->scanner.local_ip), "0.0.0.0");
    config->scanner.tcp_port = 44818U;
    config->scanner.target_udp_port = 2222U;
    config->scanner.local_udp_port = 2222U;
    config->scanner.config_assembly = 128U;
    config->scanner.input_assembly = 129U;
    config->scanner.output_assembly = 130U;
    config->scanner.input_header_bytes = 4U;
    config->scanner.output_header_bytes = 4U;
    config->scanner.input_tail_padding_bytes = 0U;
    config->scanner.output_tail_padding_bytes = 0U;
    config->scanner.input_assembly_size = 0U;
    config->scanner.output_assembly_size = 0U;
    config->scanner.o2t_rpi_us = 10000U;
    config->scanner.t2o_rpi_us = 10000U;
    config->scanner.service_cycle_ms = 2U;
    config->scanner.input_stale_timeout_ms = 500U;
    config->scanner.o2t_run_idle_header = true;
    config->scanner.t2o_run_idle_header = false;
    config->scanner.reconnect_enabled = true;
    config->scanner.reconnect_delay_ms = 1000U;
    config->scanner.max_reconnect_attempts = 0U;
    config->scanner.timeout_multiplier = 3U;
    config->scanner.originator_vendor_id = 1U;
    config->scanner.originator_serial_number = UINT32_C(0x12345678);
    (void)snprintf(config->scanner.socket_path, sizeof(config->scanner.socket_path),
                   "/tmp/ek9500-terminal-suite.sock");
    (void)snprintf(config->scanner.output_state_file,
                   sizeof(config->scanner.output_state_file),
                   "/tmp/ek9500-terminal-suite.outputs");
    config->scanner.preserve_device_outputs = true;
    config->scanner.persist_outputs = true;
    config->scanner.strict_assembly_size = true;
    config->scanner.console_color = true;
    config->scanner.periodic_status_ms = 1000U;
    config->scanner.released_output_poll_ms = 100U;
    config->scanner.verify_explicit_output_writes = true;
}

static ead_result_t fn_ReadIni(const char *path,
                               ead_config_section_t *sections,
                               size_t *section_count,
                               char *error,
                               size_t error_size)
{
    FILE *file;
    char raw[1024];
    ead_config_section_t *current = NULL;
    size_t line_number = 0U;
    bool in_block_comment = false;

    file = fopen(path, "r");
    if (file == NULL)
    {
        if ((error != NULL) && (error_size > 0U))
        {
            (void)snprintf(error, error_size, "cannot open configuration '%s': %s",
                           path, strerror(errno));
        }
        return EAD_ERR_IO;
    }

    while (fgets(raw, sizeof(raw), file) != NULL)
    {
        char *line;
        char *equals;
        ++line_number;
        fn_RemoveBlockComments(raw, &in_block_comment);
        line = fn_Trim(raw);
        if ((*line == '\0') || (*line == '#') || (*line == ';')) continue;

        if (*line == '[')
        {
            char *close = strrchr(line, ']');
            size_t index;
            if ((close == NULL) || (close[1] != '\0'))
            {
                (void)snprintf(error, error_size, "invalid section at line %zu", line_number);
                (void)fclose(file);
                return EAD_ERR_CONFIG;
            }
            *close = '\0';
            line = fn_Trim(line + 1);
            if ((*line == '\0') || (*section_count >= EAD_CONFIG_MAX_SECTIONS))
            {
                (void)snprintf(error, error_size, "invalid or excessive section at line %zu", line_number);
                (void)fclose(file);
                return EAD_ERR_CONFIG;
            }
            for (index = 0U; index < *section_count; ++index)
            {
                if (strcmp(sections[index].name, line) == 0)
                {
                    (void)snprintf(error, error_size, "duplicate section [%s]", line);
                    (void)fclose(file);
                    return EAD_ERR_CONFIG;
                }
            }
            current = &sections[*section_count];
            (void)memset(current, 0, sizeof(*current));
            if (snprintf(current->name, sizeof(current->name), "%s", line) >= (int)sizeof(current->name))
            {
                (void)snprintf(error, error_size, "section name too long at line %zu", line_number);
                (void)fclose(file);
                return EAD_ERR_CONFIG;
            }
            ++(*section_count);
            continue;
        }

        equals = strchr(line, '=');
        if ((equals == NULL) || (current == NULL))
        {
            (void)snprintf(error, error_size, "invalid assignment at line %zu", line_number);
            (void)fclose(file);
            return EAD_ERR_CONFIG;
        }
        *equals = '\0';
        {
            char *key = fn_Trim(line);
            char *value = fn_Trim(equals + 1);
            size_t index;
            fn_Lower(key);
            if ((*key == '\0') || (current->pair_count >= EAD_CONFIG_MAX_KEYS))
            {
                (void)snprintf(error, error_size, "invalid or excessive key at line %zu", line_number);
                (void)fclose(file);
                return EAD_ERR_CONFIG;
            }
            for (index = 0U; index < current->pair_count; ++index)
            {
                if (strcmp(current->pairs[index].key, key) == 0)
                {
                    (void)snprintf(error, error_size, "duplicate key '%s' in [%s]", key, current->name);
                    (void)fclose(file);
                    return EAD_ERR_CONFIG;
                }
            }
            if ((snprintf(current->pairs[current->pair_count].key,
                          sizeof(current->pairs[current->pair_count].key), "%s", key) >=
                 (int)sizeof(current->pairs[current->pair_count].key)) ||
                (snprintf(current->pairs[current->pair_count].value,
                          sizeof(current->pairs[current->pair_count].value), "%s", value) >=
                 (int)sizeof(current->pairs[current->pair_count].value)))
            {
                (void)snprintf(error, error_size, "key or value too long at line %zu", line_number);
                (void)fclose(file);
                return EAD_ERR_CONFIG;
            }
            ++current->pair_count;
        }
    }

    if (ferror(file) != 0)
    {
        fn_SetError(error, error_size, "error while reading configuration");
        (void)fclose(file);
        return EAD_ERR_IO;
    }
    if (in_block_comment)
    {
        fn_SetError(error, error_size,
                    "unterminated block comment at end of configuration");
        (void)fclose(file);
        return EAD_ERR_CONFIG;
    }
    (void)fclose(file);
    return EAD_OK;
}

static ead_result_t fn_ParseTerminalKind(const char *text, ead_terminal_kind_t *kind)
{
    if ((text == NULL) || (kind == NULL)) return EAD_ERR_ARGUMENT;
    if (strcasecmp(text, "EL1409") == 0) *kind = EAD_TERMINAL_EL1409;
    else if (strcasecmp(text, "EL2409") == 0) *kind = EAD_TERMINAL_EL2409;
    else if (strcasecmp(text, "EL3074") == 0) *kind = EAD_TERMINAL_EL3074;
    else if (strcasecmp(text, "EL4074") == 0) *kind = EAD_TERMINAL_EL4074;
    else if (strcasecmp(text, "RAW") == 0) *kind = EAD_TERMINAL_RAW;
    else return EAD_ERR_CONFIG;
    return EAD_OK;
}

static ead_result_t fn_ParsePdo(const char *text, ead_pdo_layout_t *layout)
{
    if ((text == NULL) || (layout == NULL)) return EAD_ERR_ARGUMENT;
    if (strcasecmp(text, "bit_packed") == 0) *layout = EAD_PDO_BIT_PACKED;
    else if (strcasecmp(text, "compact_int16") == 0) *layout = EAD_PDO_COMPACT_INT16;
    else if (strcasecmp(text, "status_value_int16") == 0) *layout = EAD_PDO_STATUS_VALUE_INT16;
    else if (strcasecmp(text, "control_value_int16") == 0) *layout = EAD_PDO_CONTROL_VALUE_INT16;
    else if (strcasecmp(text, "real32") == 0) *layout = EAD_PDO_REAL32;
    else if (strcasecmp(text, "raw") == 0) *layout = EAD_PDO_RAW;
    else return EAD_ERR_CONFIG;
    return EAD_OK;
}

static ead_result_t fn_ParseAnalogMode(const char *text, ead_analog_mode_t *mode)
{
    if ((text == NULL) || (mode == NULL)) return EAD_ERR_ARGUMENT;
    if (strcasecmp(text, "bipolar_10v") == 0) *mode = EAD_ANALOG_BIPOLAR_10V;
    else if (strcasecmp(text, "unipolar_10v") == 0) *mode = EAD_ANALOG_UNIPOLAR_10V;
    else if (strcasecmp(text, "bipolar_20ma") == 0) *mode = EAD_ANALOG_BIPOLAR_20MA;
    else if (strcasecmp(text, "unipolar_20ma") == 0) *mode = EAD_ANALOG_UNIPOLAR_20MA;
    else if (strcasecmp(text, "current_4_20ma") == 0) *mode = EAD_ANALOG_4_TO_20MA;
    else return EAD_ERR_CONFIG;
    return EAD_OK;
}

static void fn_SetTerminalDefaults(ead_terminal_config_t *terminal)
{
    (void)memset(terminal, 0, sizeof(*terminal));
    terminal->kind = EAD_TERMINAL_EL1409;
    terminal->enabled = true;
    terminal->input_offset_mode = EAD_OFFSET_AUTO;
    terminal->output_offset_mode = EAD_OFFSET_AUTO;
    terminal->logical_base = 1U;
    terminal->refresh_ms = 100U;
    terminal->pdo_layout = EAD_PDO_BIT_PACKED;
    terminal->analog_mode = EAD_ANALOG_BIPOLAR_10V;
    terminal->engineering_min = -10.0;
    terminal->engineering_max = 10.0;
    terminal->full_scale_raw = 30518;
}

static ead_result_t fn_AssignScanner(ead_scanner_config_t *scanner,
                                     const ead_config_section_t *section,
                                     char *error,
                                     size_t error_size)
{
    static const char *const allowed[] = {
        "profile_name", "backend", "adapter_ip", "local_ip", "tcp_port", "target_udp_port",
        "local_udp_port", "config_assembly", "input_assembly", "output_assembly",
        "input_header_bytes", "output_header_bytes", "input_tail_padding_bytes",
        "output_tail_padding_bytes", "input_assembly_size", "output_assembly_size",
        "o2t_rpi_us", "t2o_rpi_us", "service_cycle_ms", "input_stale_timeout_ms",
        "o2t_run_idle_header", "t2o_run_idle_header", "reconnect_enabled",
        "reconnect_delay_ms", "max_reconnect_attempts", "timeout_multiplier",
        "originator_vendor_id", "originator_serial_number", "socket_path",
        "output_state_file", "preserve_device_outputs", "persist_outputs",
        "strict_assembly_size", "console_color", "periodic_status_ms",
        "released_output_poll_ms", "verify_explicit_output_writes"
    };
    size_t index;
    ead_result_t result = fn_ValidateKeys(section, allowed, sizeof(allowed) / sizeof(allowed[0]), error, error_size);
    if (result != EAD_OK) return result;

    for (index = 0U; index < section->pair_count; ++index)
    {
        const char *key = section->pairs[index].key;
        const char *value = section->pairs[index].value;
        unsigned long long number = 0U;
        bool boolean_value = false;
#define EAD_SET_STRING(field) do { if (snprintf(scanner->field, sizeof(scanner->field), "%s", value) >= (int)sizeof(scanner->field)) goto invalid; } while (0)
#define EAD_SET_U16(field) do { if (!fn_ParseUnsigned(value, &number) || (number > UINT16_MAX)) goto invalid; scanner->field = (uint16_t)number; } while (0)
#define EAD_SET_U32(field) do { if (!fn_ParseUnsigned(value, &number) || (number > UINT32_MAX)) goto invalid; scanner->field = (uint32_t)number; } while (0)
#define EAD_SET_SIZE(field) do { if (!fn_ParseUnsigned(value, &number) || (number > SIZE_MAX)) goto invalid; scanner->field = (size_t)number; } while (0)
#define EAD_SET_BOOL(field) do { if (!fn_ParseBool(value, &boolean_value)) goto invalid; scanner->field = boolean_value; } while (0)
        if (strcmp(key, "profile_name") == 0) EAD_SET_STRING(profile_name);
        else if (strcmp(key, "backend") == 0) EAD_SET_STRING(backend);
        else if (strcmp(key, "adapter_ip") == 0) EAD_SET_STRING(adapter_ip);
        else if (strcmp(key, "local_ip") == 0) EAD_SET_STRING(local_ip);
        else if (strcmp(key, "tcp_port") == 0) EAD_SET_U16(tcp_port);
        else if (strcmp(key, "target_udp_port") == 0) EAD_SET_U16(target_udp_port);
        else if (strcmp(key, "local_udp_port") == 0) EAD_SET_U16(local_udp_port);
        else if (strcmp(key, "config_assembly") == 0) EAD_SET_U16(config_assembly);
        else if (strcmp(key, "input_assembly") == 0) EAD_SET_U16(input_assembly);
        else if (strcmp(key, "output_assembly") == 0) EAD_SET_U16(output_assembly);
        else if (strcmp(key, "input_header_bytes") == 0) EAD_SET_SIZE(input_header_bytes);
        else if (strcmp(key, "output_header_bytes") == 0) EAD_SET_SIZE(output_header_bytes);
        else if (strcmp(key, "input_tail_padding_bytes") == 0) EAD_SET_SIZE(input_tail_padding_bytes);
        else if (strcmp(key, "output_tail_padding_bytes") == 0) EAD_SET_SIZE(output_tail_padding_bytes);
        else if (strcmp(key, "input_assembly_size") == 0) EAD_SET_SIZE(input_assembly_size);
        else if (strcmp(key, "output_assembly_size") == 0) EAD_SET_SIZE(output_assembly_size);
        else if (strcmp(key, "o2t_rpi_us") == 0) EAD_SET_U32(o2t_rpi_us);
        else if (strcmp(key, "t2o_rpi_us") == 0) EAD_SET_U32(t2o_rpi_us);
        else if (strcmp(key, "service_cycle_ms") == 0) EAD_SET_U32(service_cycle_ms);
        else if (strcmp(key, "input_stale_timeout_ms") == 0) EAD_SET_U32(input_stale_timeout_ms);
        else if (strcmp(key, "o2t_run_idle_header") == 0) EAD_SET_BOOL(o2t_run_idle_header);
        else if (strcmp(key, "t2o_run_idle_header") == 0) EAD_SET_BOOL(t2o_run_idle_header);
        else if (strcmp(key, "reconnect_enabled") == 0) EAD_SET_BOOL(reconnect_enabled);
        else if (strcmp(key, "reconnect_delay_ms") == 0) EAD_SET_U32(reconnect_delay_ms);
        else if (strcmp(key, "max_reconnect_attempts") == 0) EAD_SET_U32(max_reconnect_attempts);
        else if (strcmp(key, "timeout_multiplier") == 0) { if (!fn_ParseUnsigned(value, &number) || (number > 7U)) goto invalid; scanner->timeout_multiplier = (uint8_t)number; }
        else if (strcmp(key, "originator_vendor_id") == 0) EAD_SET_U16(originator_vendor_id);
        else if (strcmp(key, "originator_serial_number") == 0) EAD_SET_U32(originator_serial_number);
        else if (strcmp(key, "socket_path") == 0) EAD_SET_STRING(socket_path);
        else if (strcmp(key, "output_state_file") == 0) EAD_SET_STRING(output_state_file);
        else if (strcmp(key, "preserve_device_outputs") == 0) EAD_SET_BOOL(preserve_device_outputs);
        else if (strcmp(key, "persist_outputs") == 0) EAD_SET_BOOL(persist_outputs);
        else if (strcmp(key, "strict_assembly_size") == 0) EAD_SET_BOOL(strict_assembly_size);
        else if (strcmp(key, "console_color") == 0) EAD_SET_BOOL(console_color);
        else if (strcmp(key, "periodic_status_ms") == 0) EAD_SET_U32(periodic_status_ms);
        else if (strcmp(key, "released_output_poll_ms") == 0) EAD_SET_U32(released_output_poll_ms);
        else if (strcmp(key, "verify_explicit_output_writes") == 0) EAD_SET_BOOL(verify_explicit_output_writes);
#undef EAD_SET_STRING
#undef EAD_SET_U16
#undef EAD_SET_U32
#undef EAD_SET_SIZE
#undef EAD_SET_BOOL
        continue;
invalid:
        if ((error != NULL) && (error_size > 0U))
        {
            (void)snprintf(error, error_size, "[scanner]: invalid value '%s' for %s", value, key);
        }
        return EAD_ERR_CONFIG;
    }

    if (strcasecmp(scanner->backend, "native_c") != 0)
    {
        fn_SetError(error, error_size, "[scanner]: backend must be native_c in the C-only build");
        return EAD_ERR_CONFIG;
    }
    if ((scanner->profile_name[0] == '\0') ||
        (scanner->adapter_ip[0] == '\0') || (scanner->tcp_port == 0U) ||
        (scanner->o2t_rpi_us == 0U) || (scanner->t2o_rpi_us == 0U) ||
        (scanner->service_cycle_ms == 0U) || (scanner->input_stale_timeout_ms == 0U) || (scanner->socket_path[0] == '\0') ||
        (scanner->released_output_poll_ms == 0U))
    {
        fn_SetError(error, error_size, "[scanner]: required values must be non-zero/non-empty");
        return EAD_ERR_CONFIG;
    }
    if ((scanner->input_assembly_size > EAD_MAX_ASSEMBLY_SIZE) ||
        (scanner->output_assembly_size > EAD_MAX_ASSEMBLY_SIZE))
    {
        fn_SetError(error, error_size, "[scanner]: assembly size exceeds EAD_MAX_ASSEMBLY_SIZE");
        return EAD_ERR_CONFIG;
    }
    return EAD_OK;
}

static ead_result_t fn_AssignTerminal(ead_terminal_config_t *terminal,
                                      const ead_config_section_t *section,
                                      char *error,
                                      size_t error_size)
{
    static const char *const allowed[] = {
        "type", "position", "enabled", "input_offset", "output_offset",
        "input_bytes", "output_bytes", "logical_base", "refresh_ms",
        "pdo_layout", "analog_mode", "engineering_min", "engineering_max",
        "full_scale_raw", "allow_extended_range", "channel_stride",
        "status_stride", "status_offset", "value_offset"
    };
    size_t index;
    ead_result_t result;

    result = fn_ValidateKeys(section, allowed, sizeof(allowed) / sizeof(allowed[0]), error, error_size);
    if (result != EAD_OK) return result;
    fn_SetTerminalDefaults(terminal);
    if (snprintf(terminal->name, sizeof(terminal->name), "%s", section->name + 9) >=
        (int)sizeof(terminal->name))
    {
        fn_SetError(error, error_size, "terminal name too long");
        return EAD_ERR_CONFIG;
    }

    for (index = 0U; index < section->pair_count; ++index)
    {
        const char *key = section->pairs[index].key;
        const char *value = section->pairs[index].value;
        unsigned long long number = 0U;
        long long signed_number = 0;
        bool boolean_value = false;
        double real_value = 0.0;
        if (strcmp(key, "type") == 0)
        {
            if (fn_ParseTerminalKind(value, &terminal->kind) != EAD_OK) goto invalid;
        }
        else if (strcmp(key, "position") == 0)
        {
            if (!fn_ParseUnsigned(value, &number) || (number == 0U) || (number > SIZE_MAX)) goto invalid;
            terminal->position = (size_t)number;
        }
        else if (strcmp(key, "enabled") == 0)
        {
            if (!fn_ParseBool(value, &boolean_value)) goto invalid;
            terminal->enabled = boolean_value;
        }
        else if ((strcmp(key, "input_offset") == 0) || (strcmp(key, "output_offset") == 0))
        {
            ead_offset_mode_t *mode = (strcmp(key, "input_offset") == 0) ?
                                      &terminal->input_offset_mode : &terminal->output_offset_mode;
            size_t *offset = (strcmp(key, "input_offset") == 0) ?
                             &terminal->input_offset : &terminal->output_offset;
            if (strcasecmp(value, "auto") == 0)
            {
                *mode = EAD_OFFSET_AUTO;
                *offset = 0U;
            }
            else
            {
                if (!fn_ParseUnsigned(value, &number) || (number > SIZE_MAX)) goto invalid;
                *mode = EAD_OFFSET_EXPLICIT;
                *offset = (size_t)number;
            }
        }
        else if (strcmp(key, "input_bytes") == 0)
        {
            if (!fn_ParseUnsigned(value, &number) || (number > SIZE_MAX)) goto invalid;
            terminal->input_bytes_override = (size_t)number;
        }
        else if (strcmp(key, "output_bytes") == 0)
        {
            if (!fn_ParseUnsigned(value, &number) || (number > SIZE_MAX)) goto invalid;
            terminal->output_bytes_override = (size_t)number;
        }
        else if (strcmp(key, "logical_base") == 0)
        {
            if (!fn_ParseUnsigned(value, &number) || (number == 0U) || (number > SIZE_MAX)) goto invalid;
            terminal->logical_base = (size_t)number;
        }
        else if (strcmp(key, "refresh_ms") == 0)
        {
            if (!fn_ParseUnsigned(value, &number) || (number == 0U) || (number > UINT32_MAX)) goto invalid;
            terminal->refresh_ms = (uint32_t)number;
        }
        else if (strcmp(key, "pdo_layout") == 0)
        {
            if (fn_ParsePdo(value, &terminal->pdo_layout) != EAD_OK) goto invalid;
        }
        else if (strcmp(key, "analog_mode") == 0)
        {
            if (fn_ParseAnalogMode(value, &terminal->analog_mode) != EAD_OK) goto invalid;
        }
        else if (strcmp(key, "engineering_min") == 0)
        {
            if (!fn_ParseDouble(value, &real_value)) goto invalid;
            terminal->engineering_min = real_value;
        }
        else if (strcmp(key, "engineering_max") == 0)
        {
            if (!fn_ParseDouble(value, &real_value)) goto invalid;
            terminal->engineering_max = real_value;
        }
        else if (strcmp(key, "full_scale_raw") == 0)
        {
            if (!fn_ParseSigned(value, &signed_number) || (signed_number <= 0) || (signed_number > INT32_MAX)) goto invalid;
            terminal->full_scale_raw = (int32_t)signed_number;
        }
        else if (strcmp(key, "allow_extended_range") == 0)
        {
            if (!fn_ParseBool(value, &boolean_value)) goto invalid;
            terminal->allow_extended_range = boolean_value;
        }
        else if (strcmp(key, "channel_stride") == 0)
        {
            if (!fn_ParseUnsigned(value, &number) || (number > SIZE_MAX)) goto invalid;
            terminal->channel_stride = (size_t)number;
        }
        else if (strcmp(key, "status_stride") == 0)
        {
            if (!fn_ParseUnsigned(value, &number) || (number > SIZE_MAX)) goto invalid;
            terminal->status_stride = (size_t)number;
        }
        else if (strcmp(key, "status_offset") == 0)
        {
            if (!fn_ParseUnsigned(value, &number) || (number > SIZE_MAX)) goto invalid;
            terminal->status_offset = (size_t)number;
        }
        else if (strcmp(key, "value_offset") == 0)
        {
            if (!fn_ParseUnsigned(value, &number) || (number > SIZE_MAX)) goto invalid;
            terminal->value_offset = (size_t)number;
        }
        continue;
invalid:
        if ((error != NULL) && (error_size > 0U))
        {
            (void)snprintf(error, error_size, "[%s]: invalid value '%s' for %s",
                           section->name, value, key);
        }
        return EAD_ERR_CONFIG;
    }

    if ((terminal->position == 0U) || (terminal->logical_base == 0U) ||
        (terminal->refresh_ms == 0U))
    {
        if ((error != NULL) && (error_size > 0U))
        {
            (void)snprintf(error, error_size, "[%s]: position, logical_base and refresh_ms must be non-zero", section->name);
        }
        return EAD_ERR_CONFIG;
    }
    if (((terminal->kind == EAD_TERMINAL_EL3074) ||
         (terminal->kind == EAD_TERMINAL_EL4074)) &&
        ((terminal->engineering_max <= terminal->engineering_min) ||
         (terminal->full_scale_raw <= 0)))
    {
        if ((error != NULL) && (error_size > 0U))
        {
            (void)snprintf(error, error_size, "[%s]: invalid analog scaling", section->name);
        }
        return EAD_ERR_CONFIG;
    }
    return EAD_OK;
}

ead_result_t fn_EadConfigLoad(const char *path,
                              ead_system_config_t *config,
                              char *error,
                              size_t error_size)
{
    ead_config_section_t sections[EAD_CONFIG_MAX_SECTIONS];
    size_t section_count = 0U;
    size_t index;
    bool scanner_seen = false;
    ead_result_t result;

    if ((path == NULL) || (config == NULL)) return EAD_ERR_ARGUMENT;
    fn_EadConfigSetDefaults(config);
    (void)memset(sections, 0, sizeof(sections));
    if ((error != NULL) && (error_size > 0U)) error[0] = '\0';

    result = fn_ReadIni(path, sections, &section_count, error, error_size);
    if (result != EAD_OK) return result;

    for (index = 0U; index < section_count; ++index)
    {
        if (strcmp(sections[index].name, "scanner") == 0)
        {
            result = fn_AssignScanner(&config->scanner, &sections[index], error, error_size);
            if (result != EAD_OK) return result;
            scanner_seen = true;
        }
        else if (strncmp(sections[index].name, "terminal.", 9U) == 0)
        {
            if ((sections[index].name[9] == '\0') ||
                (config->terminal_count >= EAD_MAX_TERMINALS))
            {
                fn_SetError(error, error_size, "invalid or excessive terminal section");
                return EAD_ERR_CONFIG;
            }
            result = fn_AssignTerminal(&config->terminals[config->terminal_count],
                                       &sections[index], error, error_size);
            if (result != EAD_OK) return result;
            ++config->terminal_count;
        }
        else
        {
            if ((error != NULL) && (error_size > 0U))
            {
                (void)snprintf(error, error_size, "unknown section [%s]", sections[index].name);
            }
            return EAD_ERR_CONFIG;
        }
    }

    if (!scanner_seen)
    {
        fn_SetError(error, error_size, "missing [scanner] section");
        return EAD_ERR_CONFIG;
    }
    if (config->terminal_count == 0U)
    {
        fn_SetError(error, error_size, "no terminal sections are configured");
        return EAD_ERR_CONFIG;
    }
    return EAD_OK;
}
