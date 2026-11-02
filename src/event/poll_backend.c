#define _POSIX_C_SOURCE 200809L
#include "loop.h"

#include "../core/module.h"
#include "../core/pool.h"
#include "../http/request.h"
#include "../os/unix/socket.h"
 
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define NG_HTTP_BUFFER_SIZE 8192

typedef struct {
    int fd;
    ng_pool_t *pool;
    char request_buffer[NG_HTTP_BUFFER_SIZE + 1];
    size_t request_size;
    const char *response;
    size_t response_size;
    size_t response_offset;
    int responding;
} ng_connection_t;

static void connection_close(ng_connection_t *connection, ng_log_t *log) {
    if (connection->fd != -1) {
        close(connection->fd);
        ng_log_write(log, NG_LOG_DEBUG, "closed client connection");
        connection->fd = -1;
    }
    ng_pool_destroy(connection->pool);
    connection->pool = NULL;
}

static void response_prepare(ng_connection_t *connection, int status, const char *type,
                             const char *body, size_t body_length, ng_log_t *log) {
    const char *reason = status == 200 ? "OK" : status == 400 ? "Bad Request" :
                         status == 404 ? "Not Found" : status == 405 ? "Method Not Allowed" :
                         "Internal Server Error";
    char header[512];
    int header_size = snprintf(header, sizeof(header),
                               "HTTP/1.1 %d %s\r\nContent-Type: %s\r\n"
                               "Content-Length: %zu\r\nConnection: close\r\n\r\n",
                               status, reason, type, body_length);
    if (header_size < 0 || (size_t)header_size >= sizeof(header)) {
        connection_close(connection, log);
        return;
    }
    size_t total = (size_t)header_size + body_length;
    char *output = ng_pool_alloc(connection->pool, total);
    if (!output) {
        connection_close(connection, log);
        return;
    }
    memcpy(output, header, (size_t)header_size);
    memcpy(output + header_size, body, body_length);
    connection->response = output;
    connection->response_size = total;
    connection->response_offset = 0;
    connection->responding = 1;
}

static void process_request(ng_connection_t *connection, ng_log_t *log) {
    ng_http_request_t request = {0};
    ng_http_parse_result_t parsed = ng_http_parse_request(connection->request_buffer,
                                                           connection->request_size,
                                                           connection->pool, &request);
    if (parsed == NG_HTTP_BAD_REQUEST) {
        static const char body[] = "bad request\n";
        response_prepare(connection, 400, "text/plain; charset=utf-8", body, sizeof(body) - 1, log);
        return;
    }
    if (parsed != NG_HTTP_COMPLETE) return;

    ng_log_write(log, NG_LOG_DEBUG, "request %s %s %s", request.method, request.target, request.version);
    ng_http_response_t response = {0};
    int handled = ng_modules_handle_http(&request, &response, connection->pool, log);
    if (handled == NG_MODULE_DECLINED) {
        response.status = 404;
        response.content_type = "text/plain; charset=utf-8";
        response.body = "not found\n";
        response.body_length = strlen(response.body);
    } else if (handled == NG_MODULE_ERROR) {
        response.status = 500;
        response.content_type = "text/plain; charset=utf-8";
        response.body = "internal server error\n";
        response.body_length = strlen(response.body);
    }
    response_prepare(connection, response.status, response.content_type, response.body,
                     response.body_length, log);
}

static void accept_ready(int listener, ng_connection_t *clients, size_t count,
                         size_t block_size, ng_log_t *log) {
    for (;;) {
        int fd = accept(listener, NULL, NULL);
        if (fd == -1) {
            if (errno == EINTR) continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK) ng_log_write(log, NG_LOG_ERROR, "accept: %s", strerror(errno));
            return;
        }
        if (ng_socket_nonblocking(fd) == -1) {
            ng_log_write(log, NG_LOG_ERROR, "cannot make client socket non-blocking: %s", strerror(errno));
            close(fd);
            continue;
        }
        size_t slot;
        for (slot = 0; slot < count; slot++) if (clients[slot].fd == -1) break;
        if (slot == count) {
            close(fd);
            ng_log_write(log, NG_LOG_WARN, "connection limit reached; rejected client");
            continue;
        }
        clients[slot] = (ng_connection_t){.fd = fd, .pool = ng_pool_create(block_size)};
        if (!clients[slot].pool) {
            close(fd);
            clients[slot].fd = -1;
            ng_log_write(log, NG_LOG_ERROR, "could not allocate connection pool");
            continue;
        }
        ng_log_write(log, NG_LOG_DEBUG, "accepted client connection fd=%d", fd);
    }
}

static void read_ready(ng_connection_t *connection, ng_log_t *log) {
    for (;;) {
        if (connection->request_size == NG_HTTP_BUFFER_SIZE) {
            static const char body[] = "request headers too large\n";
            response_prepare(connection, 400, "text/plain; charset=utf-8", body, sizeof(body) - 1, log);
            return;
        }
        ssize_t count = recv(connection->fd, connection->request_buffer + connection->request_size,
                             NG_HTTP_BUFFER_SIZE - connection->request_size, 0);
        if (count > 0) {
            connection->request_size += (size_t)count;
            connection->request_buffer[connection->request_size] = '\0';
            process_request(connection, log);
            if (connection->responding || connection->fd == -1) return;
            continue;
        }
        if (count == 0) { connection_close(connection, log); return; }
        if (errno == EINTR) continue;
        if (errno != EAGAIN && errno != EWOULDBLOCK) connection_close(connection, log);
        return;
    }
}

static void write_ready(ng_connection_t *connection, ng_log_t *log) {
    while (connection->response_offset < connection->response_size) {
        ssize_t count = send(connection->fd, connection->response + connection->response_offset,
                             connection->response_size - connection->response_offset, 0);
        if (count > 0) { connection->response_offset += (size_t)count; continue; }
        if (count == -1 && errno == EINTR) continue;
        if (count == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
        connection_close(connection, log);
        return;
    }
    connection_close(connection, log);
}

int ng_event_loop_run(const ng_config_t *config, ng_log_t *log) {
    signal(SIGPIPE, SIG_IGN);
    int listener = ng_socket_listen(config->listen_port);
    if (listener == -1) {
        ng_log_write(log, NG_LOG_ERROR, "listen on port %u failed: %s", config->listen_port, strerror(errno));
        return -1;
    }
    size_t capacity = config->worker_connections;
    ng_connection_t *clients = calloc(capacity, sizeof(*clients));
    struct pollfd *events = calloc(capacity + 1, sizeof(*events));
    if (!clients || !events) {
        ng_log_write(log, NG_LOG_ERROR, "could not allocate event loop state");
        free(clients); free(events); close(listener); return -1;
    }
    for (size_t i = 0; i < capacity; i++) clients[i].fd = -1;
    ng_log_write(log, NG_LOG_INFO, "listening on 0.0.0.0:%u as %s", config->listen_port, config->server_name);

    int result = 0;
    for (;;) {
        events[0] = (struct pollfd){.fd = listener, .events = POLLIN};
        for (size_t i = 0; i < capacity; i++) {
            events[i + 1] = (struct pollfd){.fd = clients[i].fd,
                                            .events = clients[i].fd == -1 ? 0 : clients[i].responding ? POLLOUT : POLLIN};
        }
        int ready = poll(events, capacity + 1, -1);
        if (ready == -1) {
            if (errno == EINTR) continue;
            ng_log_write(log, NG_LOG_ERROR, "poll failed: %s", strerror(errno));
            result = -1;
            break;
        }
        if (events[0].revents & POLLIN) accept_ready(listener, clients, capacity, config->pool_block_size, log);
        for (size_t i = 0; i < capacity; i++) {
            if (clients[i].fd == -1) continue;
            short revents = events[i + 1].revents;
            if (revents & (POLLERR | POLLHUP | POLLNVAL)) connection_close(&clients[i], log);
            else if (revents & POLLIN) read_ready(&clients[i], log);
            else if (revents & POLLOUT) write_ready(&clients[i], log);
        }
    }
    for (size_t i = 0; i < capacity; i++) connection_close(&clients[i], log);
    free(events);
    free(clients);
    close(listener);
    return result;
}
