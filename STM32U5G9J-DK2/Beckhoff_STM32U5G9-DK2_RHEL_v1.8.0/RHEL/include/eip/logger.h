/**
 * @file logger.h
 * @brief Compatibility logging facade for the embedded EtherNet/IP transport.
 * @author Prathamesh Chipkar
 */
#ifndef EIP_LOGGER_COMPAT_H
#define EIP_LOGGER_COMPAT_H

#include "ead/logger.h"

typedef ead_log_level_t log_level_t;
#define LOG_LEVEL_ERROR EAD_LOG_ERROR
#define LOG_LEVEL_WARN  EAD_LOG_WARN
#define LOG_LEVEL_INFO  EAD_LOG_INFO
#define LOG_LEVEL_DEBUG EAD_LOG_DEBUG
#define LOG_LEVEL_TRACE EAD_LOG_TRACE
#define LOG_ERROR(...) EAD_LOGE(__VA_ARGS__)
#define LOG_WARN(...)  EAD_LOGW(__VA_ARGS__)
#define LOG_INFO(...)  EAD_LOGI(__VA_ARGS__)
#define LOG_DEBUG(...) EAD_LOGD(__VA_ARGS__)
#define LOG_TRACE(...) EAD_LOGT(__VA_ARGS__)
#define fn_LogHex fn_EadLogHex

#endif
