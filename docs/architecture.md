# Architecture

## Overview

Ironpulse is a pipeline: **acquire → store → analyze → alert → serve**.

```
┌─────────────┐   ┌──────────────┐   ┌────────────────┐   ┌─────────────┐
│  protocol   │──▶│   storage    │──▶│   analytics     │──▶│     api      │
│ Modbus TCP  │   │ RingBuffer/  │   │ ZScoreDetector  │   │ REST + WS    │
│ client(s)   │   │ SeriesStore  │   │ (Strategy)      │   │ (+ web/ SPA) │
└─────────────┘   └──────────────┘   └────────────────┘   └─────────────┘
```

Each layer is an independent CMake target with its own unit tests, so it
can be developed, tested, and benchmarked in isolation.

## Concurrency model

- A single `asio::io_context`, driven by a small fixed-size pool of threads
  (`config.worker_threads`), runs all network I/O — one `ModbusClient` per
  configured device, each polling on its own `asio::steady_timer`.
- `SeriesStore` and its per-sensor `RingBuffer`s are mutex-protected and
  safe to call from any of the io threads.
- `AnomalyDetector` instances are *not* shared across threads — each
  `DevicePoller` owns its own detector, since detection state (rolling
  window) is inherently per-sensor and per-poller, avoiding the need for
  synchronization there entirely.

## Why not lock-free everywhere?

Sensor update rates in this domain are sub-kHz per device. A mutex around
a `RingBuffer::push` is not the bottleneck at that rate. Reaching for
lock-free structures before profiling proves a need would trade
readability for speculative performance — see `docs/adr/0002-storage-format.md`.

## Extending the analytics layer

`AnomalyDetector` is a small interface (see
`include/ironpulse/analytics/detector.hpp`); adding EWMA or CUSUM means
implementing the interface, no changes needed elsewhere. A `rule_engine`
that combines multiple detectors' outputs per sensor before raising an
alert is the next planned addition.

## Known limitations (current milestone)

- No automatic WAL compaction on startup — `RetentionPolicy::prune` must be
  invoked periodically by the operator (or wired into a timer) rather than
  running itself; the maintenance timer in `main.cpp` currently only
  flushes the WAL, not prunes it.
- Config loader only supports JSON, not YAML (despite YAML examples
  referenced in early planning docs) — kept simple deliberately;
  `nlohmann::json` was already a dependency, so a YAML parser was not
  worth adding for the current milestone.
