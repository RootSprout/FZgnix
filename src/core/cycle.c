#include "cycle.h"

#include "config.h"
#include "log.h"
#include "module.h"
#include "../event/loop.h"

#include <stdio.h>
#include <unistd.h>

int ng_cycle_run(const char *config_path) {
    ng_config_t config;
    ng_config_defaults(&config);
    char error[512];
    if (ng_config_load(config_path, &config, error, sizeof(error)) != 0) {
        fprintf(stderr, "ngnix: %s\n", error);
        return 1;
    }
    ng_log_t log;
    ng_log_init(&log, STDERR_FILENO, config.error_log_level);
    ng_log_write(&log, NG_LOG_INFO, "starting core cycle from %s", config_path);
    if (ng_modules_init(&log) != 0) return 1;
    if (ng_event_loop_run(&config, &log) != 0) return 1;
    return 0;
}
