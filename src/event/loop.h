#ifndef NGNIX_EVENT_LOOP_H
#define NGNIX_EVENT_LOOP_H

#include "../core/config.h"
#include "../core/log.h"

int ng_event_loop_run(const ng_config_t *config, ng_log_t *log);

#endif
