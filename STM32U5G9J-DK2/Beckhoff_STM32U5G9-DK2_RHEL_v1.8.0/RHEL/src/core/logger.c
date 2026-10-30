/**
 * @file logger.c
 * @brief Thread-safe project logging implementation.
 * @author Prathamesh Chipkar
 */
#define _POSIX_C_SOURCE 200809L
#include "ead/logger.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

static ead_log_level_t st_LogLevel = EAD_LOG_INFO;
static bool st_ColorEnabled = false;
static pthread_mutex_t st_LogMutex = PTHREAD_MUTEX_INITIALIZER;

void fn_EadLogSetLevel(ead_log_level_t level) { st_LogLevel = level; }
void fn_EadLogEnableColor(bool enabled) { st_ColorEnabled = enabled; }

static const char *fn_LevelText(ead_log_level_t level)
{
    switch (level)
    {
        case EAD_LOG_ERROR: return "ERROR";
        case EAD_LOG_WARN: return "WARN";
        case EAD_LOG_INFO: return "INFO";
        case EAD_LOG_DEBUG: return "DEBUG";
        case EAD_LOG_TRACE: return "TRACE";
        default: return "LOG";
    }
}

static const char *fn_ColorCode(ead_log_color_t color)
{
    switch (color)
    {
        case EAD_COLOR_RED: return "\033[1;31m";
        case EAD_COLOR_GREEN: return "\033[1;32m";
        case EAD_COLOR_YELLOW: return "\033[1;33m";
        case EAD_COLOR_CYAN: return "\033[1;36m";
        case EAD_COLOR_MAGENTA: return "\033[1;35m";
        default: return "";
    }
}

static bool fn_ColorActive(void)
{
    const char *no_color = getenv("NO_COLOR");
    if ((no_color != NULL) && (no_color[0] != '\0')) return false;
    return st_ColorEnabled && (isatty(STDERR_FILENO) != 0);
}

static void fn_LogV(ead_log_level_t level,
                    ead_log_color_t color,
                    const char *format,
                    va_list arguments) EAD_PRINTF_FORMAT(3, 0);

static void fn_LogV(ead_log_level_t level,
                    ead_log_color_t color,
                    const char *format,
                    va_list arguments)
{
    struct timespec now;
    struct tm local_time;
    char timestamp[32];
    const bool use_color = fn_ColorActive() && (color != EAD_COLOR_DEFAULT);

    if ((format == NULL) || (level > st_LogLevel)) return;
    (void)clock_gettime(CLOCK_REALTIME, &now);
    (void)localtime_r(&now.tv_sec, &local_time);
    (void)strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &local_time);

    (void)pthread_mutex_lock(&st_LogMutex);
    if (use_color) (void)fputs(fn_ColorCode(color), stderr);
    (void)fprintf(stderr, "%s.%03ld %-5s ", timestamp,
                  now.tv_nsec / 1000000L, fn_LevelText(level));
    (void)vfprintf(stderr, format, arguments);
    if (use_color) (void)fputs("\033[0m", stderr);
    (void)fputc('\n', stderr);
    (void)fflush(stderr);
    (void)pthread_mutex_unlock(&st_LogMutex);
}

void fn_EadLog(ead_log_level_t level, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    fn_LogV(level, EAD_COLOR_DEFAULT, format, arguments);
    va_end(arguments);
}

void fn_EadLogColor(ead_log_level_t level, ead_log_color_t color, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    fn_LogV(level, color, format, arguments);
    va_end(arguments);
}

void fn_EadLogHex(ead_log_level_t level, const char *label, const uint8_t *data, size_t length)
{
    size_t index;
    if ((data == NULL) || (level > st_LogLevel)) return;
    fn_EadLog(level, "%s (%zu bytes)", (label != NULL) ? label : "data", length);
    (void)pthread_mutex_lock(&st_LogMutex);
    for (index = 0U; index < length; index += 16U)
    {
        size_t column;
        (void)fprintf(stderr, "  %04zx :", index);
        for (column = 0U; (column < 16U) && ((index + column) < length); ++column)
        {
            (void)fprintf(stderr, " %02X", data[index + column]);
        }
        (void)fputc('\n', stderr);
    }
    (void)pthread_mutex_unlock(&st_LogMutex);
}
