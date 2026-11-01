# Architecture and code walkthrough

This document describes the implementation that is in the repository today. `ngnix` is a small educational HTTP server written in C. It borrows the separation of concerns used by NGINX, but it is not source-compatible with NGINX and does not implement the complete feature set.

## Architecture diagram

```mermaid
flowchart TD
    Args[CLI arguments] --> Main[src/main.c]
    Main --> Cycle[Core cycle\nsrc/core/cycle.c]
    Cycle --> Defaults[Config defaults]
    Defaults --> ConfigParser[Config loader\nsrc/core/config.c]
    ConfigParser --> RuntimeConfig[ng_config_t]
    Cycle --> Logger[Logger\nsrc/core/log.c]
    Cycle --> Registry[Module registry\nsrc/core/module.c]
    Cycle --> Loop[Event loop\nsrc/event/poll_backend.c]
    RuntimeConfig --> Loop
    Logger --> Loop

    Loop --> Socket[Unix socket helpers\nsrc/os/unix/socket.c]
    Socket --> Listener[Non-blocking listening socket]
    Client[HTTP client] --> Listener
    Listener --> Accept[Accept ready clients]
    Accept --> Conn[Connection state\nfd + bounded request buffer]
    Conn --> Pool[Per-connection pool\nsrc/core/pool.c]
    Conn --> Poll[poll(2) readiness loop]
    Poll -->|readable| Parser[HTTP parser\nsrc/http/request.c]
    Parser -->|incomplete| Conn
    Parser -->|complete request| Dispatch[Module dispatch]
    Registry --> Dispatch
    Dispatch --> Static[Built-in HTTP module\nsrc/http/static_module.c]
    Static -->|handled or declined| Response[Select 200, 404, 405, or 400]
    Response --> Serialize[Build response headers + body]
    Pool --> Serialize
    Serialize -->|writable| Poll
    Poll -->|partial send until complete| Client
    Poll -->|done or error| Close[Close fd + destroy pool]
    Close --> Pool
```

## Startup and runtime configuration

### `src/main.c`

`main` is deliberately small. It chooses `ngnix.conf` by default, accepts `-c <file>` to select another configuration file, rejects unsupported arguments, and hands control to `ng_cycle_run`. Keeping argument parsing here means the core cycle can be started independently of command-line details.

### `src/core/cycle.c` and `cycle.h`

The cycle is the startup coordinator:

1. Fill an `ng_config_t` with defaults.
2. Load the selected config file and stop with a readable error if parsing fails.
3. Initialize the logger using the configured log threshold.
4. Initialize registered modules.
5. Start the event loop. The current event loop runs until an unrecoverable `poll` error; graceful shutdown/reload is future work.

This gives the program one place to construct and connect the subsystems. The cycle owns the configuration and logger during the server lifetime.

### `src/core/config.c` and `config.h`

`ng_config_t` is the runtime settings structure. It currently carries the listen port, maximum simultaneous client slots, pool block size, log threshold, root path, and server name. `ng_config_defaults` initializes these fields before the file is read, so an omitted supported directive keeps its default.

`ng_config_load` reads one line at a time, removes `#` comments, trims whitespace, and checks the semicolon-terminated directive. It validates numeric ranges and fixed-size string lengths, and reports errors with a line number.

The parser is intentionally a subset. It accepts `listen`, `worker_connections`, `pool_block_size`, `error_log_level`, `root`, and `server_name`. It skips the literal block-marker lines `http {`, `server {`, `events {`, and `}` so the sample has a familiar shape, but it does not build a configuration tree or assign directives to scopes. In this version, `root` is stored but not used to serve files.

## Core services

### `src/core/pool.c` and `pool.h`

The request-scoped allocator uses two allocation paths:

- Small allocations are served by linked arena blocks. Each block tracks `used` and `capacity`; allocations advance a bump pointer after aligning for `max_align_t`.
- Allocations larger than the configured block size use `malloc` and are recorded on a separate linked list so they can still be released with the pool.

`ng_pool_create` creates the first block. `ng_pool_alloc` returns uninitialized memory, `ng_pool_calloc` checks multiplication overflow and zeroes the returned bytes, and `ng_pool_destroy` frees all large allocations, all arena blocks, and the pool object.

The event loop creates one pool per accepted connection. Parsed request strings and the assembled response live in that pool, so closing the connection releases the whole group at once. The current pool has no individual-free or reset operation, and callers must not use pointers after destroying the pool.

### `src/core/log.c` and `log.h`

The logger has `debug`, `info`, `warn`, `error`, and `off` levels. The configured level is a threshold: messages below it are ignored. Each entry gets a local timestamp and level label, is formatted into a bounded stack buffer, and is written to the configured file descriptor. The cycle currently directs logs to standard error.

This is a small synchronous logger. It does not yet support NGINX-style log chains, file rotation, debug categories, or a separate configurable log destination.

### `src/core/module.c` and `module.h`

`ng_module_t` describes a module by name and optional lifecycle and HTTP-handler callbacks. The registry is currently a compile-time array containing `ng_http_static_module`.

At startup, `ng_modules_init` invokes each non-null init callback. During a request, `ng_modules_handle_http` calls handlers in registry order. A module can return:

- `NG_MODULE_HANDLED` when it produced the response;
- `NG_MODULE_DECLINED` to let the next module try;
- `NG_MODULE_ERROR` to ask the event layer to produce a 500 response.

This is the connection point for adding more HTTP modules. Registration is static at build time today; dynamic loading and per-module configuration are not implemented.

## Event and socket layers

### `src/os/unix/socket.c` and `socket.h`

`ng_socket_listen` creates an IPv4 TCP socket, enables `SO_REUSEADDR`, marks it non-blocking, binds it to `0.0.0.0:<port>`, and starts listening. `ng_socket_nonblocking` uses `fcntl` to preserve the existing flags and add `O_NONBLOCK`.

This isolates basic Unix socket setup from HTTP and event logic. The implementation currently binds IPv4 on all interfaces; IPv6, TLS, Unix-domain sockets, and platform-specific tuning are not included.

### `src/event/loop.h` and `poll_backend.c`

`ng_event_loop_run` creates the listening socket, a client-state array sized from `worker_connections`, and a `pollfd` array. Each client slot contains the fd, a bounded request buffer, a per-connection pool, and the response bytes plus a send offset.

The loop rebuilds readiness interests on each pass:

- the listener watches for `POLLIN`;
- a client still reading watches for `POLLIN`;
- a client with a prepared response watches for `POLLOUT`.

When the listener is readable, `accept_ready` accepts until the non-blocking socket reports `EAGAIN`/`EWOULDBLOCK`, marks each client non-blocking, finds a free slot, and creates its pool. If the configured slots are full, the accepted fd is closed.

`read_ready` receives data incrementally. After each read it asks the HTTP parser whether the header terminator has arrived. Once a response is ready, `write_ready` sends from the saved offset. If the socket can only accept part of the response, the offset is retained and the next writable event resumes the send. After the response is sent, or the connection errors, `connection_close` closes the fd and destroys its pool.

This is a level-triggered `poll(2)` implementation and scans the configured slot array on each loop. It handles multiple open connections without a thread per connection, but it is not an epoll/kqueue implementation and does not yet include timers, keep-alive, graceful worker lifecycle, or connection reuse.

## HTTP layer

### `src/http/request.c` and `request.h`

The parser searches for `\r\n\r\n`, which marks the end of the headers. Before that delimiter arrives, it returns `NG_HTTP_INCOMPLETE`. It then parses the first line into method, target, and HTTP version, validates the version as HTTP/1.0 or HTTP/1.1, and copies those three strings into the connection pool.

The event loop caps buffered request data at 8192 bytes. The parser does not yet parse individual headers, validate `Content-Length`, consume request bodies, handle chunked encoding, decode paths, or support HTTP/2. Its purpose is to connect incremental socket reads to a module-facing request structure.

### `src/http/static_module.c`

The first built-in module handles `GET` and `HEAD` for `/` and `/health`, returning a small text response. Unsupported methods produce `405`; unrecognized paths are declined so the dispatcher can try another handler. If no module handles a path, the event layer returns `404`.

Despite the module name and parsed `root` setting, this first module does not read files from disk yet. Real static-file lookup is a later milestone.

### Response construction in `poll_backend.c`

Once a module returns a response, the event layer maps the status to a reason phrase, formats the status line and headers, adds `Content-Length` and `Connection: close`, then copies the header and body into the connection pool. The `response_offset` field makes writes safe when `send` writes only part of the response.

Each connection serves one request and closes. Persistent connections and multiple requests on one socket are not supported yet.

## Build and sample configuration

The `Makefile` discovers every `.c` file below `src`, compiles with C11 warnings enabled, and links the `ngnix` executable. `ngnix.conf` demonstrates the supported directives; the current parser treats the block braces as formatting markers rather than scope boundaries.

`src/stream/README.md` marks the planned stream subsystem. It does not contain executable stream code yet.

## End-to-end request flow

1. `main` selects the config file and starts the core cycle.
2. The cycle applies defaults, loads settings, initializes logging and modules.
3. The event loop binds the configured port and waits in `poll`.
4. A readable listener accepts a client and creates connection state plus a pool.
5. Readable client events fill the bounded request buffer until a complete header arrives.
6. The HTTP parser creates a request object in the pool.
7. The module registry tries handlers; the first handler that claims the request provides a response.
8. The event layer serializes the response, sends it incrementally on writable events, then closes the connection and frees its pool.

## Next architecture steps

The current boundaries are intended to make the next additions local: add parser and context objects under `core` for nested configuration, add timer and backend interfaces under `event`, add file-serving and request-phase handlers under `http`, and add worker lifecycle management around the core cycle. Proxying and load balancing can then be added as upstream modules, with the `stream` subsystem built on the same event and connection foundations.
