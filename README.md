# ngnix

A from-scratch learning project that rebuilds NGINX's core ideas feature by feature, following *Inside NGINX: High Performance Web Server Through Source Code*.

The current milestone is a modular, single-process HTTP server with a non-blocking `poll(2)` event loop. Startup flows through the core cycle and config parser; each client gets a request-scoped memory pool; logging, HTTP parsing, and request handling are separate modules. It is intentionally a teaching implementation, not a production web server.

## Build and run

```sh
make
./ngnix -c ngnix.conf
curl -i http://127.0.0.1:8080/
```

Press Ctrl-C to stop the server. The default port is `8080`.

## Current scope

- Core startup cycle and config-driven listener
- Aligned arena allocator with separately tracked large allocations
- Leveled timestamped logger
- Module registry and HTTP handler dispatch
- Incremental HTTP/1.x request parsing, bounded headers, 400/404/405 responses
- Non-blocking IPv4 socket and `poll(2)` event backend
- Per-connection memory pools and partial response writes

The `root` setting is parsed for the next static-file milestone; this version currently serves `/` and `/health` from the built-in HTTP module.

## Module layout

- `src/core`: startup cycle, config subset, memory pool, logger and module registry
- `src/event`: `poll(2)` event backend and connection state machine
- `src/http`: HTTP/1.x request parsing and the first built-in HTTP module
- `src/os/unix`: Unix socket setup and non-blocking descriptor helpers
- `src/stream`: reserved for the stream subsystem described later in the book

The connected request path is:

```text
main -> core cycle -> config + module initialization -> event loop
                                               client -> HTTP parser -> module dispatch -> response
```

See [ROADMAP.md](ROADMAP.md) for the planned sequence mapped to the book.
See [ARCHITECTURE.md](ARCHITECTURE.md) for the component walkthrough and request-flow diagram.
