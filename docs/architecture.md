# Architecture

## Overview

Ironpulse is a pipeline: **acquire → store → analyze → alert → serve**, with an
optional **export** branch that streams every reading to files.

```
┌──────────────┐   ┌──────────────┐   ┌──────────────────┐   ┌──────────────┐
│   protocol   │──▶│   storage    │   │    analytics     │──▶│    notify    │
│ DevicePoller │   │ RingBuffer / │   │ RuleEngine:      │   │ Telegram,    │
│ ModbusClient │   │ SeriesStore  │   │ limits + z-score/│   │ Slack,       │
│ decoder      │   │ + WAL        │   │ EWMA/CUSUM quorum│   │ webhook      │
└──────┬───────┘   └──────▲───────┘   └────────▲─────────┘   └──────────────┘
       │                  │                    │
       ├──────────────────┴──── EventBus ──────┴──────────▶ api (REST, WebSocket,
       │                                                    /metrics) → dashboard
       │   ┌──────────────────────────────────────────┐
       └──▶│ ingest (optional export)                 │
           │ SPSC queue → Ingestor thread → segments  │──▶ export/*.ipseg
           └──────────────────────────────────────────┘
```

Each layer is an independent CMake target (`ironpulse::core`, `::protocol`,
`::storage`, `::ingest`, `::analytics`, `::api`, `::notify`) with its own tests, so it can
be developed, tested, and benchmarked in isolation. Layers never call each
other directly: they publish and subscribe to events on `core::EventBus`
(`SensorReadingEvent`, `AnomalyEvent`, `DeviceStatusEvent`, `PollErrorEvent`).
`main.cpp` only wires subscriptions together.

## Data flow

1. **`protocol::DevicePoller`** — one per device — runs a fixed-rate cycle. At
   startup `plan_reads()` groups the device's sensors into as few Modbus
   requests as possible (≤ 125 registers each, bounded gaps, holding and input
   tables kept apart). Each cycle sends those requests one after another (Modbus
   TCP allows one outstanding request per connection), decodes every sensor's
   registers (`decode_raw`: 16/32-bit integers and float32 in either word order,
   then scale/offset), and publishes all readings with one shared timestamp.
2. **Storage** appends each reading to its in-memory ring buffer and, with
   persistence enabled, to the sensor's write-ahead log.
3. **`analytics::RuleEngine`** checks the reading against the sensor's hard
   limits (edge-triggered critical alerts) and feeds it to the sensor's
   detectors (quorum-voted warning alerts), honouring a per-sensor cooldown.
4. **Alerts** go to the bounded `AlertLog` (for the REST API), the WebSocket
   broadcast, the metrics registry, and the **`notify::Notifier`** queue.
5. **The API layer** serves the dashboard and integrations: REST for state and
   history, a WebSocket for live push, `/metrics` for Prometheus.
6. **Export** (when `export.enabled`): `ingest::ReadingExporter` maps the sensor
   id to a numeric tag and pushes a 24-byte sample into a lock-free SPSC queue.
   The `Ingestor` thread drains it into batches (by size or `flush_interval_ms`),
   encodes each with a CRC-32C and `SegmentWriter` appends it to the current
   segment file. Format: [`export-format.md`](export-format.md); design:
   [ADR 0004](adr/0004-export-pipeline.md).

## Concurrency model

- A single `asio::io_context`, driven by a small fixed-size pool of threads
  (`config.worker_threads`), runs all network I/O: Modbus polling, the WebSocket
  server and housekeeping timers.
- Every `ModbusClient`, `DevicePoller` and `WsConnection` owns a **strand**, so
  each object's handlers never run concurrently even though the pool has many
  threads. Different devices and connections still proceed in parallel.
- WebSocket broadcasts arrive from several polling strands at once; each
  connection queues outgoing frames and keeps a single write in flight, since
  overlapping `async_write` calls on one socket would interleave frames.
- `SeriesStore`, `RingBuffer`, `WalWriter`, `RuleEngine`, `AlertLog`,
  `DeviceRegistry` and `Metrics` are mutex-protected and safe to call from any
  thread. `RuleEngine` publishes alerts *after* releasing its lock, so a
  subscriber can never deadlock it.
- The REST server (cpp-httplib) runs its own accept loop on a dedicated thread.
- The notifier delivers on its own worker thread with a bounded queue, so a slow
  or unreachable chat service never delays polling or alerting.
- The export path follows the same rule for disk I/O: polling strands only push
  into the SPSC queue (taking turns through a mutex held for a few nanoseconds,
  never contended by the writer); encoding and file writes happen on the
  `Ingestor` thread. A stalled disk drops export samples (counted), never
  polling.

## Failure handling

- Every Modbus connect and request is bounded by `timeout_ms`. On timeout the
  connection is closed and reopened on the next cycle; a timer that expires at
  the same moment a response arrives is ignored via a generation counter.
- A response with an unexpected transaction id or too few registers is treated
  as a broken stream: the connection is reset rather than trusted.
- A device that answers with a Modbus exception stays *online* (it is reachable;
  its configuration is wrong) and the error is logged and counted.
- Polling falls back to the next tick rather than bursting to catch up after a
  slow cycle or reconnect.
- Notification delivery retries server errors and timeouts with exponential
  backoff; client errors (bad token, unknown chat) are not retried.

## Why not lock-free everywhere?

Sensor update rates in this domain are sub-kHz per device. A mutex around a
`RingBuffer::push` is not the bottleneck at that rate. Reaching for lock-free
structures before profiling proves a need would trade readability for
speculative performance — see `docs/adr/0002-storage-format.md`.

The one lock-free structure, `ingest::SpscRingBuffer`, sits where it earns its
place: at the hand-off from the polling strands to a thread doing file I/O, so
that the writer never holds a lock the strands need. Measured on a 2-core VM
(`tests/benchmarks/bench_ingest.cpp`): ~9 ns for an SPSC push+pop against ~32 ns
for a mutex-guarded storage push, and ~240 M items/s between two threads.

## Extending

- **A detection strategy:** implement `analytics::AnomalyDetector` (`observe`
  and `name`), then add its type to `make_detector()` and to the config parser's
  `parse_detector()`.
- **A notification channel:** add a case to `notify::build_request()` (pure,
  unit-testable) and accept the new `type` in the config parser.
- **An export consumer:** read `*.ipseg` files as described in
  [`export-format.md`](export-format.md); `ironpulse-export dump` converts them
  to CSV, `tools/export_reader/read_segment.py` is a dependency-free reader.
- **A dashboard language:** add `web/js/i18n/locales/<code>.js`, list it in
  `web/js/i18n/i18n.js` and `index.html`, and add the notification texts to
  `src/notify/messages.cpp`.

## Known limitations

- Modbus TCP only; serial Modbus RTU devices need a TCP gateway.
- A single shared API token rather than per-user accounts and roles.
- Alerts are not acknowledged by operators; the alert log keeps the last 500
  in memory (history of the readings themselves is persisted).
- Config loader only supports JSON, not YAML — `nlohmann::json` was already a
  dependency, so a YAML parser was not worth adding.
