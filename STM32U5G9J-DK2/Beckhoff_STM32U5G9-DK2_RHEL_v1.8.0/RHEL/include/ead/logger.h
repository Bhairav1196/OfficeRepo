/**
 * @file logger.h
 * @brief Project logging levels, color control and hexadecimal diagnostic output.
 * @author Prathamesh Chipkar
 */
#ifndef EAD_LOGGER_H
#define EAD_LOGGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ead/compiler.h"

/** @brief Logger severity or output-format enumeration. */
typedef enum
{
    EAD_LOG_ERROR = 0,
    EAD_LOG_WARN,
    EAD_LOG_INFO,
    EAD_LOG_DEBUG,
    EAD_LOG_TRACE
} ead_log_level_t;

/** @brief Logger severity or output-format enumeration. */
typedef enum
{
    EAD_COLOR_DEFAULT = 0,
    EAD_COLOR_RED,
    EAD_COLOR_GREEN,
    EAD_COLOR_YELLOW,
    EAD_COLOR_CYAN,
    EAD_COLOR_MAGENTA
} ead_log_color_t;

/** @brief Set the minimum severity emitted by the project logger. */
void fn_EadLogSetLevel(ead_log_level_t level);
/** @brief Enable or disable ANSI color in diagnostic logs. */
void fn_EadLogEnableColor(bool enabled);
/** @brief Emit a formatted project log message. */
void fn_EadLog(ead_log_level_t level, const char *format, ...)
    EAD_PRINTF_FORMAT(2, 3);
/** @brief Emit a formatted log message using an explicit terminal color. */
void fn_EadLogColor(ead_log_level_t level, ead_log_color_t color, const char *format, ...)
    EAD_PRINTF_FORMAT(3, 4);
/** @brief Emit a bounded hexadecimal dump for protocol/process-image diagnostics. */
void fn_EadLogHex(ead_log_level_t level, const char *label, const uint8_t *data, size_t length);

#define EAD_LOGE(...) fn_EadLog(EAD_LOG_ERROR, __VA_ARGS__)
#define EAD_LOGW(...) fn_EadLog(EAD_LOG_WARN, __VA_ARGS__)
#define EAD_LOGI(...) fn_EadLog(EAD_LOG_INFO, __VA_ARGS__)
#define EAD_LOGD(...) fn_EadLog(EAD_LOG_DEBUG, __VA_ARGS__)
#define EAD_LOGT(...) fn_EadLog(EAD_LOG_TRACE, __VA_ARGS__)

#endif
