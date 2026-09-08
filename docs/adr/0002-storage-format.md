# ADR 0002: In-memory ring buffer + WAL for time-series storage

## Status
Accepted and implemented.

## Context
Sensor readings arrive at sub-second intervals per device. We need:
1. Fast, recent-data access for the analytics layer (anomaly detection
   needs a rolling window) and for API queries ("last N minutes").
2. Durability across restarts, without pulling in a full time-series
   database (InfluxDB, TimescaleDB) as a hard dependency for a project
   whose core value is the C++ engine itself.

## Decision
- **Hot path**: a fixed-capacity, mutex-protected `RingBuffer<T>` per
  sensor (see `include/ironpulse/storage/ring_buffer.hpp`), holding the
  most recent N samples in memory. A plain mutex was chosen over a
  lock-free structure because, at expected per-sensor update rates
  (sub-kHz), contention is not the bottleneck — see `docs/architecture.md`
  for the reasoning; this can be revisited if benchmarks
  (`tests/benchmarks/bench_ring_buffer.cpp`, planned) show otherwise.
- **Durability**: a simple append-only write-ahead log per sensor
  (`WalWriter`/`WalReader`, `include/ironpulse/storage/wal_writer.hpp`),
  buffered in memory and flushed in batches. On startup, `SeriesStore`
  replays a sensor's WAL file into its ring buffer the first time that
  sensor is touched, optionally discarding entries older than a configured
  max age. `RetentionPolicy` (`retention_policy.hpp`) rewrites a WAL file
  in place to drop records past the retention window — this avoids running
  a separate storage service while still surviving process restarts.

## Consequences
- Enabling persistence (`persistence_enabled: true` in config) is opt-in;
  without it, a restart still loses all in-memory history, same as before.
- WAL writes are batched (not synced per-sample), so an unclean shutdown
  can lose the last partial batch — acceptable for telemetry, where losing
  a fraction-of-a-second of samples is not catastrophic. `flush_all()` is
  called on graceful shutdown and periodically via a timer in `main.cpp`.
- `RetentionPolicy::prune` is implemented and unit-tested but not yet
  wired into a recurring timer in `main.cpp` — currently an operator (or a
  future maintenance task) must invoke it, e.g. via a cron job calling a
  small CLI wrapper, or by wiring it into the existing maintenance timer
  alongside the WAL flush. Tracked in the README roadmap.
- The `SeriesStore` API (`include/ironpulse/storage/series_store.hpp`) is
  written so persistence is fully optional and transparent to callers in
  the protocol/analytics/API layers — they call `record()`/`recent()`
  exactly the same whether or not a `WalWriter` is attached.
