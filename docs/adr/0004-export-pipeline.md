# ADR 0004: Export pipeline built on the apollonian ingestor

## Status
Accepted and implemented.

## Context
Ironpulse keeps readings for its own purposes: a per-sensor ring buffer for the
detectors and the dashboard, and a per-sensor WAL so that history survives a
restart (ADR 0002). Neither is meant for getting data *out*: the WAL is one file
per sensor, carries no sensor metadata and is rewritten by retention pruning.

Users asked for the raw stream — to load into a data lake or historian, to train
models offline, to keep longer than `retention_hours`. Separately, the
[apollonian_core_ingestor](https://github.com/Anton-Sergeev-EA/apollonian_core_ingestor)
project already contained the pieces of such a pipeline: a lock-free SPSC queue,
a batch serializer with hardware CRC and a batching consumer thread. It had no
data source and no file output, and it had two latent format defects (below).

## Decision
Merge apollonian into Ironpulse as the `ironpulse::ingest` module, with its
history, and build the export on it:

```
polling strands ──submit()──▶ [mutex] ──▶ SPSC queue ──▶ Ingestor thread
                                                         │ batch (size or time)
                                                         │ encode + CRC-32C
                                                         ▼
                                                   SegmentWriter ──▶ *.ipseg
```

- **Separate from the WAL.** The WAL serves the dashboard and is shaped for
  that. Export is an optional, append-only stream for consumers outside
  Ironpulse, with its own format (`docs/export-format.md`).
- **Disk I/O never on a polling strand.** The only work done on the strand is a
  hash lookup and a queue push. If the disk stalls, the queue fills and export
  readings are dropped and counted (`ironpulse_export_dropped_total`, plus a log
  warning) — polling, alerting and the dashboard are unaffected. Losing export
  data under a disk failure is preferable to losing monitoring.
- **SPSC queue with a producer-side mutex.** The queue has one consumer by
  construction but several producers (one strand per device). The producers take
  a short mutex around the push. It is held for a few nanoseconds and only ever
  contended by other producers — never by the consumer, which is the thread that
  can be slow. A lock-free MPSC queue would remove that mutex at the cost of a
  more complex and harder-to-verify structure, for no measurable gain at
  sub-kHz-per-device rates (`BM_ExporterSubmit`: ~110 ns per reading, most of it
  the sensor-id hash lookup).
- **Self-describing segments.** Each file carries the tag table it was written
  with, so a configuration change cannot silently re-label old data.
- **`*.part` until finished.** Shippers can pick up finished files with a glob;
  crash leftovers are finalised at the next start instead of being forgotten.

### Format fixes made while merging
1. The SSE4.2 path computed CRC-32C, the software fallback IEEE CRC-32. A batch
   written on one class of CPU failed verification on the other. Both paths now
   compute CRC-32C, and a test checks them against each other for every length
   and alignment.
2. The 12-byte batch header left the `double`s misaligned, so the "zero-copy"
   decoder read them through a misaligned pointer (undefined behaviour; a fault on
   strict-alignment targets). The header is now 16 bytes and the in-place decoder
   refuses misaligned input.

The batch version was bumped to 2; version 1 was never used outside apollonian.

## Alternatives considered
- **A Kafka / MQTT producer in the engine.** Ties the engine to a broker and a
  client library; files can be forwarded to any of these by existing tools.
- **Parquet or Arrow IPC.** Better for analytics, but heavyweight dependencies
  for the engine; converting segments to Parquet offline is straightforward (see
  the NumPy dtype in the format document).
- **Reusing the WAL.** Wrong shape (per sensor, no metadata) and owned by
  retention.

## Consequences
- One more thread and one more bounded buffer (`queue_capacity` × 24 bytes,
  1.5 MB by default) when export is enabled; nothing when it is disabled.
- Ironpulse does not delete segments; retention of exported data is the
  consumer's responsibility.
- The "Why not lock-free everywhere?" position in `architecture.md` stands: the
  lock-free queue is used where a thread boundary to slow I/O exists, not in
  storage.
