/**
 * SDK 日志模块
 */

#include "tempmail_internal.h"
#include <stdarg.h>

static tm_log_level_t g_log_level = TM_LOG_SILENT;
static tm_log_handler_t g_log_handler = NULL;

void tm_set_log_level(tm_log_level_t level) {
    g_log_level = level;
}

void tm_set_log_handler(tm_log_handler_t handler) {
    g_log_handler = handler;
}

static const char* level_str(tm_log_level_t level) {
    switch (level) {
        case TM_LOG_ERROR: return "ERROR";
        case TM_LOG_WARN:  return "WARN";
        case TM_LOG_INFO:  return "INFO";
        case TM_LOG_DEBUG: return "DEBUG";
        default:           return "";
    }
}

void tm_log(tm_log_level_t level, const char *fmt, ...) {
    if (level > g_log_level) return;

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (g_log_handler) {
        g_log_handler(level, buf);
    } else {
        fprintf(stderr, "%s %s\n", level_str(level), buf);
    }
}
