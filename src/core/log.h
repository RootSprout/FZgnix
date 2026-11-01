#ifndef NGNIX_CORE_LOG_H
#define NGNIX_CORE_LOG_H

#include <stdarg.h>

typedef enum {
    NG_LOG_DEBUG = 0,
    NG_LOG_INFO,
    NG_LOG_WARN,
    NG_LOG_ERROR,
    NG_LOG_OFF
} ng_log_level_t;

typedef struct {
    int fd;
    ng_log_level_t level;
} ng_log_t;

void ng_log_init(ng_log_t *log, int fd, ng_log_level_t level);
void ng_log_write(ng_log_t *log, ng_log_level_t level, const char *format, ...);
ng_log_level_t ng_log_level_parse(const char *text);

#endif
