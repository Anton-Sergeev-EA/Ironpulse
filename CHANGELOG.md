# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- **Export of every reading to files** (`export` config section, off by default;
  `EXPORT_ENABLED` in Docker). Readings are batched into compact, self-describing
  segment files (`*.ipseg`, 24 bytes per reading, CRC-32C per batch, the sensor
  list inside every file) for a data lake, historian or offline analysis. Disk
  I/O runs on its own thread behind a lock-free queue, so a slow disk can never
  delay polling or alerts. Format: `docs/export-format.md`; design: ADR 0004.
- `ironpulse-export` command-line tool: `verify`, `tags`, `dump` (CSV), also
  included in the Docker image; a dependency-free Python reader in
  `tools/export_reader/`.
- **Disk budget for the export:** `export.max_total_mb` (`EXPORT_MAX_MB` in
  Docker, default 1024 MB) deletes the oldest finished segments automatically;
  `export.segment_max_mb` is also settable as `EXPORT_SEGMENT_MB`.
- `ironpulse_export_*` metrics (readings, drops, batches, I/O errors, bytes,
  segments, deleted segments, queue depth) and a log warning when export
  readings are dropped.
- The `ingest` module: the lock-free SPSC queue, batch serializer and batching
  consumer of the former apollonian_core_ingestor project, merged with its
  history.
- Benchmarks for the export path; 45 new tests (137 in total).

### Changed
- Docker Compose rotates container logs (10 MB × 3 per service) instead of
  keeping them forever.
- The Modbus simulator's port follows `IRONPULSE_BIND_HOST`, so a production
  deployment no longer exposes it to the internet.

### Fixed (in the imported ingestor code)
- The hardware (SSE4.2) and software CRC paths used different polynomials, so a
  batch written on one machine could fail verification on another.
- Samples followed a 12-byte header and were read through a misaligned pointer;
  the header is now 16 bytes and samples are 8-byte aligned.
- The queue ignored its configured capacity and always allocated 24 MB; an idle
  consumer woke 10 000 times a second; a throwing sink terminated the process.

## [1.0.0] - 2026-10-02

### Added
- **Dashboard in 8 languages** — Russian (primary), English, Chinese, Hindi,
  Spanish, French, German, Italian — with a language switcher, `?lang=` links,
  browser-language detection and locale-aware time and number formatting.
- **Sensor model.** A device has a list of sensors, each with its register table
  (holding/input), address, data type (`uint16`, `int16`, `uint32`, `int32`,
  `float32`), word order, scale/offset, name and unit. Sensors of one device are
  read in as few requests as possible.
- **Hard limits** per sensor; crossing one raises a *critical* alert (statistical
  anomalies are *warnings*). Alerts carry kind, severity, value and limit.
- **Per-sensor detection settings** in the config: z-score, EWMA and CUSUM with
  their parameters, quorum size and a cooldown between alerts.
- **Notifications** to Telegram, Slack and generic webhooks, localized in the same
  8 languages, including device outage and recovery messages; background
  delivery with retries.
- **Token authentication** for the REST API, the WebSocket and `/metrics`, with a
  sign-in dialog in the dashboard.
- **Prometheus metrics** at `/metrics`.
- **REST:** `GET /api/v1/sensors`; history from the write-ahead log for the whole
  retention period; CSV export (`?format=csv`); version in `/healthz` and
  `/api/v1/config`. OpenAPI description in `docs/openapi.yaml`.
- **Dashboard:** current value with units per sensor, limit lines on charts,
  severity badges, CSV download.
- Automatic hourly WAL retention (and once at startup).
- `ironpulse --version`.
- End-to-end tests with a fake Modbus device over TCP; HTTP API and WebSocket
  tests on real sockets (92 tests in total).
- Release workflow publishing amd64 and arm64 images to GitHub Container Registry.
- English README.

### Changed
- Modbus requests and connects are bounded by `timeout_ms`; a device that accepts
  the connection but never answers is now reported offline instead of stalling
  polling forever. Responses with a wrong transaction id or too few registers are
  rejected.
- Polling runs at a fixed rate and publishes all of a device's readings with one
  timestamp.
- Z-score and EWMA detectors wait for a warm-up period before voting, which
  removes false alarms after every restart.
- Configuration is validated with precise error messages; out-of-range numbers
  are rejected instead of being silently truncated; `${VAR}` placeholders are
  expanded from the environment; `log_level` is honoured.
- The demo simulator emulates a transformer and a pump with realistic register
  layouts; configs use the sensor model.
- The Docker image requires OpenSSL (for HTTPS notifications); the systemd unit
  reads secrets from an `EnvironmentFile` and is further sandboxed.

### Fixed
- WebSocket broadcasts from several polling threads could interleave frames on one
  connection; writes are now queued per connection.
- A WebSocket client could make the server allocate an arbitrarily large buffer;
  incoming frames are capped at 64 KiB and handshake headers at 8 KiB. Slow
  clients are disconnected instead of buffering without bound.
- WebSocket handshake headers are matched case-insensitively.
- Malformed query parameters (`?limit=abc`) returned 500; they now return 400.
- Sensor ids are validated, so they cannot escape the data directory.
- WAL files are rewritten atomically (temp file + rename) during retention.

## [0.1.0] - 2026-09-09

### Added
- Asynchronous Modbus TCP client and frame codec.
- Time-series storage: in-memory ring buffers with write-ahead-log persistence.
- Anomaly detection: z-score, EWMA and CUSUM detectors with quorum voting.
- REST API and a hand-written RFC 6455 WebSocket server; live dashboard.
- Modbus device simulator, Docker Compose stack, nginx and systemd deployment.
- CI with GCC/Clang, sanitizers, clang-format and a Docker smoke test.
