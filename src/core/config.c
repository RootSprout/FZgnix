#define _POSIX_C_SOURCE 200809L
#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ng_config_defaults(ng_config_t *config) {
    *config = (ng_config_t){
        .listen_port = 8080,
        .worker_connections = 256,
        .pool_block_size = 16384,
        .error_log_level = NG_LOG_INFO,
        .root = "www",
        .server_name = "localhost"
    };
}

static char *trim(char *text) {
    while (isspace((unsigned char)*text)) text++;
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) *--end = '\0';
    return text;
}

static int set_error(char *error, size_t size, unsigned line, const char *message) {
    snprintf(error, size, "line %u: %s", line, message);
    return -1;
}

int ng_config_load(const char *path, ng_config_t *config, char *error, size_t error_size) {
    FILE *file = fopen(path, "r");
    if (!file) {
        snprintf(error, error_size, "cannot open %s: %s", path, strerror(errno));
        return -1;
    }
    char *line = NULL;
    size_t capacity = 0;
    unsigned number = 0;
    int result = 0;
    while (getline(&line, &capacity, file) != -1) {
        number++;
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';
        char *text = trim(line);
        if (*text == '\0' || strcmp(text, "http {") == 0 || strcmp(text, "server {") == 0 ||
            strcmp(text, "events {") == 0 || strcmp(text, "}") == 0) continue;
        char *semicolon = strchr(text, ';');
        if (!semicolon || *trim(semicolon + 1) != '\0') {
            result = set_error(error, error_size, number, "expected one directive ending in ';'");
            break;
        }
        *semicolon = '\0';
        char *directive = trim(text);
        char *value = directive;
        while (*value && !isspace((unsigned char)*value)) value++;
        if (*value == '\0') {
            result = set_error(error, error_size, number, "directive requires a value");
            break;
        }
        *value++ = '\0';
        value = trim(value);
        if (strchr(value, ' ') || strchr(value, '\t')) {
            result = set_error(error, error_size, number, "directive accepts one value");
            break;
        }
        char *end = NULL;
        errno = 0;
        unsigned long parsed = strtoul(value, &end, 10);
        if (strcmp(directive, "listen") == 0) {
            if (errno || end == value || *end || parsed == 0 || parsed > UINT16_MAX) {
                result = set_error(error, error_size, number, "listen port must be 1..65535"); break;
            }
            config->listen_port = (uint16_t)parsed;
        } else if (strcmp(directive, "worker_connections") == 0) {
            if (errno || end == value || *end || parsed == 0 || parsed > 65535) {
                result = set_error(error, error_size, number, "worker_connections must be 1..65535"); break;
            }
            config->worker_connections = (size_t)parsed;
        } else if (strcmp(directive, "pool_block_size") == 0) {
            if (errno || end == value || *end || parsed < 1024 || parsed > 1048576) {
                result = set_error(error, error_size, number, "pool_block_size must be 1024..1048576"); break;
            }
            config->pool_block_size = (size_t)parsed;
        } else if (strcmp(directive, "error_log_level") == 0) {
            ng_log_level_t level = ng_log_level_parse(value);
            if ((int)level < 0) { result = set_error(error, error_size, number, "unknown log level"); break; }
            config->error_log_level = level;
        } else if (strcmp(directive, "root") == 0 || strcmp(directive, "server_name") == 0) {
            char *destination = strcmp(directive, "root") == 0 ? config->root : config->server_name;
            size_t destination_size = strcmp(directive, "root") == 0 ? sizeof(config->root) : sizeof(config->server_name);
            if (strlen(value) >= destination_size) { result = set_error(error, error_size, number, "value is too long"); break; }
            strcpy(destination, value);
        } else {
            result = set_error(error, error_size, number, "unknown directive");
            break;
        }
    }
    if (ferror(file) && result == 0) {
        snprintf(error, error_size, "error reading %s", path);
        result = -1;
    }
    free(line);
    fclose(file);
    return result;
}
