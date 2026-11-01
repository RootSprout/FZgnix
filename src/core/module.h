#ifndef NGNIX_CORE_MODULE_H
#define NGNIX_CORE_MODULE_H

#include "log.h"
#include "pool.h"
#include "../http/request.h"

typedef struct {
    int status;
    const char *content_type;
    const char *body;
    size_t body_length;
} ng_http_response_t;

typedef struct ng_module {
    const char *name;
    int (*init)(ng_log_t *log);
    int (*handle_http)(const ng_http_request_t *request, ng_http_response_t *response,
                       ng_pool_t *pool, ng_log_t *log);
} ng_module_t;

enum { NG_MODULE_DECLINED = 0, NG_MODULE_HANDLED = 1, NG_MODULE_ERROR = -1 };

const ng_module_t *const *ng_modules(size_t *count);
int ng_modules_init(ng_log_t *log);
int ng_modules_handle_http(const ng_http_request_t *request, ng_http_response_t *response,
                           ng_pool_t *pool, ng_log_t *log);

#endif
