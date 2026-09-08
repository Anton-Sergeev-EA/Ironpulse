# ADR 0001: Standalone Asio for async networking

## Status
Accepted

## Context
The protocol layer needs asynchronous, non-blocking I/O to poll dozens of
Modbus TCP devices concurrently without spawning a thread per device.
Options considered: raw POSIX sockets + epoll, Boost.Asio, standalone Asio,
libuv.

## Decision
Use **standalone Asio** (the header-only, non-Boost distribution).

- Battle-tested async model (proactor pattern) that maps directly onto our
  connection-per-device requirement.
- No dependency on the full Boost distribution — standalone Asio is
  header-only and pulled in via FetchContent, keeping build times reasonable.
- Idiomatic C++ (callbacks, and can migrate to `asio::awaitable` coroutines
  later without changing the transport layer).

## Consequences
- All protocol-layer code must run on (or coordinate with) an
  `asio::io_context`; see `main.cpp` for the threading model
  (fixed-size pool of threads calling `io_context::run()`).
- Future coroutine-based rewrite of `ModbusClient` is possible without
  touching the wire-format code in `frame_codec.cpp`.
