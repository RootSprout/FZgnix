#define _POSIX_C_SOURCE 200809L
#include "log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const char *level_name[] = {"debug", "info", "warn", "error", "off"};

void ng_log_init(ng_log_t *log, int fd, ng_log_level_t level) {
    log->fd = fd;
    log->level = level;
}

ng_log_level_t ng_log_level_parse(const char *text) {
    for (int i = NG_LOG_DEBUG; i <= NG_LOG_OFF; i++) {
        if (strcmp(text, level_name[i]) == 0) return (ng_log_level_t)i;
    }
    return (ng_log_level_t)-1;
}

void ng_log_write(ng_log_t *log, ng_log_level_t level, const char *format, ...) {
    if (!log || level < log->level || level == NG_LOG_OFF || log->fd < 0) return;
    char message[2048];
    struct timespec now;
    struct tm local;
    clock_gettime(CLOCK_REALTIME, &now);
    localtime_r(&now.tv_sec, &local);
    int prefix = snprintf(message, sizeof(message), "%04d-%02d-%02dT%02d:%02d:%02d.%03ld [%s] ",
                          local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                          local.tm_hour, local.tm_min, local.tm_sec, now.tv_nsec / 1000000,
                          level_name[level]);
    if (prefix < 0 || (size_t)prefix >= sizeof(message)) return;
    va_list args;
    va_start(args, format);
    int body = vsnprintf(message + prefix, sizeof(message) - (size_t)prefix, format, args);
    va_end(args);
    if (body < 0) return;
    size_t length = (size_t)prefix + (size_t)body;
    if (length >= sizeof(message) - 1) length = sizeof(message) - 2;
    message[length++] = '\n';
    (void)write(log->fd, message, length);
}
