#include "../core/module.h"

#include <string.h>

static int static_http_handle(const ng_http_request_t *request, ng_http_response_t *response,
                              ng_pool_t *pool, ng_log_t *log) {
    (void)pool;
    (void)log;
    if (strcmp(request->method, "GET") != 0 && strcmp(request->method, "HEAD") != 0) {
        response->status = 405;
        response->content_type = "text/plain; charset=utf-8";
        response->body = "method not allowed\n";
        response->body_length = strlen(response->body);
        return NG_MODULE_HANDLED;
    }
    if (strcmp(request->target, "/") != 0 && strcmp(request->target, "/health") != 0) {
        return NG_MODULE_DECLINED;
    }
    response->status = 200;
    response->content_type = "text/plain; charset=utf-8";
    response->body = "hello from the ngnix core\n";
    response->body_length = strlen(response->body);
    if (strcmp(request->method, "HEAD") == 0) response->body_length = 0;
    return NG_MODULE_HANDLED;
}

const ng_module_t ng_http_static_module = {
    .name = "http_static",
    .init = NULL,
    .handle_http = static_http_handle
};
