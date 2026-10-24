#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <stdbool.h>

#include "can_core.h"
#include "error_codes.h"

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} log_level_t;

static log_level_t g_current_level = LOG_LEVEL_INFO;
static FILE *g_log_file = NULL;

void logger_set_level(log_level_t level) {
    g_current_level = level;
}

log_level_t logger_get_level(void) {
    return g_current_level;
}

int logger_init(const char *filename) {
    if (filename != NULL) {
        g_log_file = fopen(filename, "a");
        if (g_log_file == NULL) {
            return CAN_ERR_IO;
        }
    }
    return CAN_OK;
}

void logger_close(void) {
    if (g_log_file != NULL && g_log_file != stdout && g_log_file != stderr) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

static const char *level_to_string(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO ";
        case LOG_LEVEL_WARN:  return "WARN ";
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_FATAL: return "FATAL";
        default:              return "UNKWN";
    }
}

static const char *level_to_color(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "\033[36m"; /* Cyan */
        case LOG_LEVEL_INFO:  return "\033[32m"; /* Green */
        case LOG_LEVEL_WARN:  return "\033[33m"; /* Yellow */
        case LOG_LEVEL_ERROR: return "\033[31m"; /* Red */
        case LOG_LEVEL_FATAL: return "\033[35m"; /* Magenta */
        default:              return "\033[0m";
    }
}

void logger_log(log_level_t level, const char *module, const char *fmt, ...) {
    if (level < g_current_level) return;

    time_t raw_time = time(NULL);
    struct tm tm_buf;
#if defined(_WIN32)
    struct tm *tmp = gmtime(&raw_time);
    if (tmp) tm_buf = *tmp;
    else memset(&tm_buf, 0, sizeof(tm_buf));
#else
    gmtime_r(&raw_time, &tm_buf);
#endif

    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);

    va_list args;

    /* Print to console */
    printf("[%s] %s[%s]\033[0m [%s] ", time_str, level_to_color(level), level_to_string(level), module ? module : "APP");
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    fflush(stdout);

    /* Print to file if configured */
    if (g_log_file != NULL) {
        fprintf(g_log_file, "[%s] [%s] [%s] ", time_str, level_to_string(level), module ? module : "APP");
        va_start(args, fmt);
        vfprintf(g_log_file, fmt, args);
        va_end(args);
        fprintf(g_log_file, "\n");
        fflush(g_log_file);
    }
}

void logger_log_frame_csv(FILE *csv_file, const can_msg_t *msg) {
    if (csv_file == NULL || msg == NULL) return;

    fprintf(csv_file, "%s,0x%03X,%u,", msg->timestamp_iso, msg->can_id, msg->dlc);
    for (uint8_t i = 0; i < msg->dlc; i++) {
        fprintf(csv_file, "%02X%s", msg->data[i], (i + 1 < msg->dlc) ? " " : "");
    }
    fprintf(csv_file, ",%s,%s\n", msg->is_extended ? "EFF" : "SFF", msg->checksum_valid ? "VALID" : "N/A");
    fflush(csv_file);
}
