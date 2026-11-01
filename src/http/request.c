#include "request.h"

#include <ctype.h>
#include <string.h>

static char *copy_token(ng_pool_t *pool, const char *start, size_t length) {
    char *token = ng_pool_alloc(pool, length + 1);
    if (!token) return NULL;
    memcpy(token, start, length);
    token[length] = '\0';
    return token;
}

ng_http_parse_result_t ng_http_parse_request(const char *buffer, size_t length,
                                             ng_pool_t *pool, ng_http_request_t *request) {
    const char *end = NULL;
    for (size_t i = 3; i < length; i++) {
        if (buffer[i - 3] == '\r' && buffer[i - 2] == '\n' && buffer[i - 1] == '\r' && buffer[i] == '\n') {
            end = buffer + i + 1;
            break;
        }
    }
    if (!end) return length > 8192 ? NG_HTTP_BAD_REQUEST : NG_HTTP_INCOMPLETE;

    const char *line_end = NULL;
    for (const char *p = buffer; p + 1 < end; p++) {
        if (p[0] == '\r' && p[1] == '\n') { line_end = p; break; }
    }
    if (!line_end) return NG_HTTP_BAD_REQUEST;

    const char *first_space = memchr(buffer, ' ', (size_t)(line_end - buffer));
    if (!first_space || first_space == buffer) return NG_HTTP_BAD_REQUEST;
    const char *second_space = memchr(first_space + 1, ' ', (size_t)(line_end - first_space - 1));
    if (!second_space || second_space == first_space + 1 || second_space + 1 == line_end) return NG_HTTP_BAD_REQUEST;
    for (const char *p = buffer; p < line_end; p++) {
        if ((unsigned char)*p < 0x20 && *p != ' ') return NG_HTTP_BAD_REQUEST;
    }
    request->method = copy_token(pool, buffer, (size_t)(first_space - buffer));
    request->target = copy_token(pool, first_space + 1, (size_t)(second_space - first_space - 1));
    request->version = copy_token(pool, second_space + 1, (size_t)(line_end - second_space - 1));
    if (!request->method || !request->target || !request->version) return NG_HTTP_BAD_REQUEST;
    if (strlen(request->version) != 8 || strncmp(request->version, "HTTP/1.", 7) != 0 ||
        request->version[7] < '0' || request->version[7] > '1') {
        return NG_HTTP_BAD_REQUEST;
    }
    for (const char *p = first_space + 1; p < second_space; p++) {
        if (isspace((unsigned char)*p) || *p == '#') return NG_HTTP_BAD_REQUEST;
    }
    request->header_bytes = (size_t)(end - buffer);
    return NG_HTTP_COMPLETE;
}
