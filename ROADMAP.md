# Build roadmap

The order follows the book's progression from server architecture into NGINX's internal systems. Each step should leave behind a runnable, understandable implementation.

| Step | Book topics | Feature to build |
| --- | --- | --- |
| 1 | Chapters 1–4: web servers, process/thread/event architectures, C10K, request flow | Single-process non-blocking event-loop foundation (done) |
| 2 | Chapters 5–8: master/worker processes, lifecycle, source tree, core cycle and connections | Core cycle, connection lifecycle and module registry (initial slice done); master/worker lifecycle and graceful reload remain |
| 3 | Chapters 9–10: memory pools, logging and debugging | Aligned per-connection pool, large allocation tracking and leveled logging (initial slice done); pool reset/debug handlers remain |
| 4 | Chapters 11–12: configuration parsing and directive system | Validated config subset and module dispatch (initial slice done); nested configuration contexts and extensible directives remain |
| 5 | Chapters 13–14: event handling and OS interfaces | `poll(2)` backend (initial slice done); timers, event queues and `kqueue`/`epoll` backends remain |
| 6 | Chapters 15–17: HTTP engine, lifecycle and phases | Bounded HTTP/1.x request parser and handler dispatch (initial slice done); full headers, keep-alive, phases and static files remain |
| 7 | Chapters 18–19: reverse proxy and load balancing | Upstream proxying, response relay, round-robin and then other balancing policies |

The source tree uses `src/core`, `src/event`, `src/http`, `src/stream` and `src/os/unix` to keep the major subsystems distinct. The book's table of contents ends with reverse proxying and load balancing. Features such as TLS, HTTP/2, caching and exact configuration compatibility can be considered after that core sequence.

## Principles

- Implement the mechanism being studied rather than delegating the whole server to a framework.
- Keep OS-specific event notification behind an interface so the event model can evolve from portable `poll(2)` to `kqueue`/`epoll`.
- Make each milestone build on the previous one and document which NGINX behavior it models and which it omits.
- Treat this as an educational reimplementation; compatibility and production hardening are separate goals.
