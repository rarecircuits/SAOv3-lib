#ifndef SAOH_UTIL_LOG_H
#define SAOH_UTIL_LOG_H

#include "saoh_config.h"

#include "saoh/consts/err.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOGL_NONE 0
#define LOGL_ERROR 1
#define LOGL_WARN 2
#define LOGL_INFO 3
#define LOGL_DEBUG 4
#define LOGL_TRACE 5

void log_internal_hexdump_trace(const char *LOG_UNIT, const void *buf, size_t len);

// clang-format off
#if SAOH_CFG_LOG_MIN_LEVEL >= LOGL_ERROR
#define log_error(fmt, ...) printf("\033[1;91m[ERROR] %-8s\033[0;91m" fmt "\033[0m\n", LOG_UNIT, ##__VA_ARGS__)
#define log_ret_error(reason, err) log_error("Error during %s: %s (%d)", reason, saoh_err_str(err), err)
#else
#define log_error(fmt, ...) do {} while(0)
#define log_ret_error(reason, err) do {} while(0)
#endif

#if SAOH_CFG_LOG_MIN_LEVEL >= LOGL_WARN
#define log_warn(fmt, ...) printf("\033[1;93m[WARN]  %-8s\033[0;93m" fmt "\033[0m\n", LOG_UNIT, ##__VA_ARGS__)
#define log_ret_warn(reason, err) log_warn("Error during %s: %s (%d)", reason, saoh_err_str(err), err)
#else
#define log_warn(fmt, ...) do {} while(0)
#define log_ret_warn(reason, err) do {} while(0)
#endif

#if SAOH_CFG_LOG_MIN_LEVEL >= LOGL_INFO
#define log_info(fmt, ...) printf("\033[1;32m[INFO]  %-8s\033[0;32m" fmt "\033[0m\n", LOG_UNIT, ##__VA_ARGS__)
#else
#define log_info(fmt, ...) do {} while(0)
#endif

#if SAOH_CFG_LOG_MIN_LEVEL >= LOGL_DEBUG
#define log_debug(fmt, ...) printf("\033[1;37m[DEBUG] %-8s\033[0;90m" fmt "\033[0m\n", LOG_UNIT, ##__VA_ARGS__)
#define log_ret_debug(reason, err) log_debug("Error during %s: %s (%d)", reason, saoh_err_str(err), err)
#else
#define log_debug(fmt, ...) do {} while(0)
#define log_ret_debug(reason, err) do {} while(0)
#endif

#if SAOH_CFG_LOG_MIN_LEVEL >= LOGL_TRACE
#define log_trace(fmt, ...) printf("\033[1;90m[TRACE] %-8s\033[0;90m" fmt "\033[0m\n", LOG_UNIT, ##__VA_ARGS__)
#define log_trace_hexdump(buf, len) log_internal_hexdump_trace(LOG_UNIT, buf, len)
#else
#define log_trace(fmt, ...) do {} while(0)
#define log_trace_hexdump(buf, len) do {} while(0)
#endif
// clang-format on

#ifdef __cplusplus
}
#endif

#endif
