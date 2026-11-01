#ifndef NGNIX_HTTP_REQUEST_H
#define NGNIX_HTTP_REQUEST_H

#include "../core/pool.h"
#include <stddef.h>

typedef struct {
    char *method;
    char *target;
    char *version;
    size_t header_bytes;
} ng_http_request_t;

typedef enum {
    NG_HTTP_INCOMPLETE = 0,
    NG_HTTP_COMPLETE = 1,
    NG_HTTP_BAD_REQUEST = -1
} ng_http_parse_result_t;

ng_http_parse_result_t ng_http_parse_request(const char *buffer, size_t length,
                                             ng_pool_t *pool, ng_http_request_t *request);

#endif
