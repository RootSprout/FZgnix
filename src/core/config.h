#ifndef NGNIX_CORE_CONFIG_H
#define NGNIX_CORE_CONFIG_H

#include "log.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t listen_port;
    size_t worker_connections;
    size_t pool_block_size;
    ng_log_level_t error_log_level;
    char root[1024];
    char server_name[256];
} ng_config_t;

void ng_config_defaults(ng_config_t *config);
int ng_config_load(const char *path, ng_config_t *config, char *error, size_t error_size);

#endif
