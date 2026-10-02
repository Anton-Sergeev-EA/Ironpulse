# Ironpulse.

**Real-time industrial monitoring that watches your equipment's sensors and tells you when something is going wrong — before it breaks.**

[![CI](https://img.shields.io/badge/CI-GitHub_Actions-2088FF?logo=githubactions&logoColor=white)](.github/workflows/ci.yml)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](CMakeLists.txt)
[![Docker](https://img.shields.io/badge/Docker-amd64%20%7C%20arm64-2496ED?logo=docker&logoColor=white)](deploy/docker)
[![Languages](https://img.shields.io/badge/UI-8%20languages-8A2BE2)](#interface-languages)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-131%20passing-brightgreen)](tests)

[Русский](README.md) · **English** · [中文](README.zh.md) · [हिन्दी](README.hi.md) · [Español](README.es.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md)

## What is it?
Picture a plant with dozens of sensors: transformer winding temperature, motor bearing vibration, pump pressure. Today someone has to notice it on a screen — or worse, hear a loud noise — before realising a part is overheating or about to fail.

**Ironpulse watches those sensors continuously and tells you immediately when readings leave their allowed range or start behaving unusually** — with the same statistical methods predictive-maintenance engineers use. Hard limits catch what is already dangerous; statistics catch both sudden spikes and slow, creeping degradation long before a value reaches its limit.

You get:
- **A live dashboard** in 8 languages — each sensor's current value in its own units, a chart with limit lines, and an alert feed with severities.
- **Notifications** to Telegram, Slack or any webhook — in your team's language, including device outages and recoveries.
- **History on disk** — a restart loses nothing, and any sensor can be exported to CSV for the whole retention period.
- **APIs and metrics** for other systems: REST, WebSocket and `/metrics` for Prometheus/Grafana.
- **Token-protected access** to the API, the live stream and the metrics.

Ironpulse talks to real industrial equipment over **Modbus TCP**, the protocol spoken by a huge share of sensors, PLCs and meters worldwide. It reads holding and input registers, understands 16- and 32-bit integers and floats in either word order, and applies scale and offset — so it works with real devices' register maps, not just the demo.

### Who is it for?
- **Engineers and technicians** who need a lightweight, self-hosted monitoring tool without paying for (or waiting for IT to approve) a commercial SCADA/IIoT platform.
- **Developers** looking at a production-grade C++20 codebase: async networking, a hand-written WebSocket server, a REST API, containerised deployment and a real test suite — without heavy frameworks.
- **Students and enthusiasts** who want to see how an industrial monitoring pipeline works end to end.

You **don't** need to know C++ to run it — see [Quick start](#quick-start). Docker builds everything.

## What it looks like.
On the left, devices and their connection status. In the middle, a card per sensor: name, a large current value in the sensor's units (red when outside its limits), a live chart with dashed limit lines and a CSV export button. On the right, the alert feed: red "critical" for limit breaches, amber "warning" for statistical anomalies. Everything updates in real time without reloading, and works as well on a control-room screen as on a phone.

## Interface languages.
The dashboard and the notifications are available in 8 languages: **Russian** (primary), English, Chinese, Hindi, Spanish, French, German and Italian — the switcher is in the top-right corner. This document is available in the same 8 languages (links at the top). The dashboard language is chosen in this order:
1. the `?lang=` URL parameter, e.g. `http://localhost:8080/?lang=en` (handy for sharing a link);
2. the language previously chosen in this browser;
3. the browser's language, if supported;
4. otherwise Russian.

The notification language is set in the configuration (`notifications.language`) — see [Notifications](#notifications).

Dashboard translations live in [`web/js/i18n/locales/`](web/js/i18n/locales): to add a language, copy `ru.js`, translate the values, and add the file to `web/index.html` and to `supported` in [`web/js/i18n/i18n.js`](web/js/i18n/i18n.js). CI checks that every language defines the same set of strings.

## Quick start.
All you need is [Docker](https://docs.docker.com/get-docker/). It builds everything else (the C++ compiler, all libraries) inside a container, so your machine stays clean.
```bash
git clone https://github.com/Anton-Sergeev-EA/Ironpulse.git
cd Ironpulse/deploy/docker
docker compose up --build
```
Wait for it to finish (a couple of minutes the first time — C++ is compiled from source), then open:
- **http://localhost:8080** — the live dashboard
- **http://localhost:8080/api/v1/sensors** — the raw API, if you're curious

That's it. You are now watching two simulated devices — a transformer (winding and oil temperature) and a pump (bearing vibration and pressure). The simulator occasionally produces spikes and sustained excursions — watch warnings and critical events appear in the "Active alerts" panel.

Prebuilt images for amd64 and arm64 (Raspberry Pi, ARM gateways) are published with every release:
```bash
docker pull ghcr.io/anton-sergeev-ea/ironpulse:latest
```

### Something not working?
- **Port already in use?** Very common if something already runs on 8080 (Jenkins, another dashboard, …). Copy `.env.example` to `.env` in `deploy/docker/`, change `HTTP_PORT`/`WS_PORT`/`NGINX_PORT` to free ports and run `docker compose up --build` again. One file, one change.
- **Still stuck?** See [Troubleshooting](#troubleshooting).

## What's on the screen, and what the terms mean.

| Term | Plain explanation |
|---|---|
| **Modbus** | A decades-old but still ubiquitous "language" industrial sensors and controllers use to talk to software. Ironpulse speaks it directly. |
| **Register** | A memory cell on the device holding a reading. Every Modbus device's documentation has a register map saying which address means what. |
| **Limit** | A boundary a value must not cross, e.g. winding temperature above 90 °C. Crossing it raises a **critical** alert. |
| **Anomaly** | A reading that doesn't fit the sensor's recent normal pattern — a sudden spike, or a slow drift that has lasted too long. It is a **warning**: the value is still within limits but behaving unusually. |
| **Confidence** | How sure the system is that a flagged reading is a real anomaly rather than noise. Higher is more certain. |
| **Z-score, EWMA, CUSUM** | Three statistical detection methods, each good at a different *kind* of problem (see [How anomaly detection works](#how-anomaly-detection-works)). Ironpulse runs several at once and only raises an alarm when enough of them agree — fewer false positives. |
| **WAL (write-ahead log)** | A safety net: every reading is written to disk as it arrives, so a restart doesn't lose recent history. |
| **REST API / WebSocket** | Two ways other software gets data out of Ironpulse: REST — "give me the current state", WebSocket — "keep me posted in real time". |

## How anomaly detection works.
Two independent mechanisms run on every reading.

**Hard limits** (`limits`). A value below `low` or above `high` raises a critical alert. It fires once when the value leaves the allowed range — not on every poll while it stays out — and again only after it has come back.

**Statistics.** A single fixed threshold either misses slow problems (bearing vibration creeping up over days) or raises false alarms on harmless noise. So each sensor can run several independent strategies, and an alarm is raised only when enough of them (`votes_required`) agree:
- **Z-score** — flags a reading that deviates from the sensor's recent mean by an unusual number of standard deviations. Good at sudden spikes.
- **EWMA** (exponentially weighted moving average) — tracks a self-adjusting "normal" that reacts to genuine shifts faster than a fixed window.
- **CUSUM** (cumulative sum) — accumulates evidence of a *sustained* drift, catching slow degradation a single-point check would miss until it became serious. It needs the sensor's normal `mean` and `stddev`.

Three more details keep the alert feed meaningful rather than noisy:
- **warm-up** — right after startup detectors stay silent until they have enough data (z-score: half its window, EWMA: ⌈3/α⌉ samples); otherwise every restart would cause false alarms;
- **cooldown** (`cooldown_seconds`) — a sustained excursion produces one notification, not one per second;
- **quorum** — a lone blip from one detector on a noisy sensor doesn't wake up the on-call engineer.

## Configuring your devices.
Devices and their sensors are described in the config file (`deploy/config/config.example.json` without Docker, `config.docker.json` with Docker). A real device with three sensors:
```json
{
  "devices": [
    {
      "id": "transformer_01",
      "host": "192.168.1.50",
      "port": 502,
      "unit_id": 1,
      "poll_interval_ms": 1000,
      "timeout_ms": 3000,
      "sensors": [
        {
          "id": "winding_temp",
          "name": "Winding temperature",
          "unit": "°C",
          "register_type": "holding",
          "address": 0,
          "data_type": "float32",
          "word_order": "big",
          "limits": { "high": 90 }
        },
        {
          "id": "oil_temp",
          "name": "Oil temperature",
          "unit": "°C",
          "register_type": "input",
          "address": 10,
          "data_type": "int16",
          "scale": 0.1,
          "limits": { "high": 75 },
          "detection": {
            "detectors": [{ "type": "cusum", "mean": 55, "stddev": 0.5 }],
            "cooldown_seconds": 120
          }
        },
        {
          "id": "load_current",
          "name": "Load current",
          "unit": "A",
          "address": 20,
          "data_type": "uint32",
          "word_order": "little",
          "scale": 0.01,
          "limits": { "low": 0, "high": 400 }
        }
      ]
    }
  ]
}
```

**Device:**

| Field | Default | Meaning |
|---|---|---|
| `id` | — | Unique device name: letters, digits, `_`, `-`, `.` |
| `host`, `port` | —, `502` | Where the device (or Modbus TCP gateway) is reachable |
| `unit_id` | `1` | Modbus slave/unit address (matters behind RS-485 gateways) |
| `poll_interval_ms` | `1000` | Polling period. Polling runs at a fixed rate and does not drift even if the device answers slowly |
| `timeout_ms` | `3000` | How long to wait for an answer. A device that accepts the connection but stays silent is reported as unreachable |
| `sensors` | one `uint16` in register 0 | The device's sensors |

**Sensor:**

| Field | Default | Meaning |
|---|---|---|
| `id` | — | Unique (across all devices) sensor name; also the name of its history file |
| `name`, `unit` | `id`, empty | Label and units for the dashboard and notifications |
| `register_type` | `holding` | `holding` (function 0x03) or `input` (0x04) |
| `address` | `0` | Address of the first register (zero-based, as on the wire) |
| `data_type` | `uint16` | `uint16`, `int16`, `uint32`, `int32`, `float32` (32-bit types use two registers) |
| `word_order` | `big` | For 32-bit values: `big` — high word first (ABCD), `little` — low word first (CDAB) |
| `scale`, `offset` | `1`, `0` | value = raw × `scale` + `offset` |
| `limits.low`, `limits.high` | none | Hard limits; crossing one is a critical alert |
| `detection.detectors` | z-score + EWMA | Strategies: `{"type": "zscore", "window": 60, "threshold": 3}`, `{"type": "ewma", "alpha": 0.2, "threshold": 3}`, `{"type": "cusum", "mean": …, "stddev": …, "slack": 0.5, "threshold": 5}`. An empty list disables statistics |
| `detection.votes_required` | `1` | How many strategies must agree |
| `detection.cooldown_seconds` | `30` | Minimum time between two alerts of the same kind for this sensor |

Ironpulse merges neighbouring registers of a device into as few requests as possible (up to 125 registers each) — ten sensors are usually read in one or two requests, not ten.

The config is validated at startup and errors point at the exact field, e.g. `config: devices[0].sensors[2].data_type: unknown value 'float64' (expected one of: uint16, int16, uint32, int32, float32)`. Devices from older configs (without a `sensors` list) keep working as before.

Any string value can reference an environment variable: `"host": "${PLC_HOST}"`. The main settings can also be overridden with `IRONPULSE_*` variables without editing the file — see [`docs/adr/0003-env-driven-config.md`](docs/adr/0003-env-driven-config.md).

## Notifications.
Ironpulse sends alerts and device outages to Telegram, Slack and any HTTP endpoint (webhook). Texts are in the chosen language and name the sensor, value, limit and units:
```
🔴 CRITICAL
Winding temperature: value 142 °C is above the limit 90 °C
Device: transformer_01
Time: 2026-10-02 06:54:09 UTC
Dashboard: https://monitoring.example.com
```

The easiest way to enable them is `.env` (Docker) or environment variables:

| Variable | Meaning |
|---|---|
| `TELEGRAM_BOT_TOKEN`, `TELEGRAM_CHAT_ID` | A bot from [@BotFather](https://t.me/BotFather), added to your chat or group |
| `SLACK_WEBHOOK_URL` | An Incoming Webhook from Slack's settings |
| `WEBHOOK_URL` | Any endpoint accepting a JSON POST |
| `NOTIFY_LANGUAGE` | `ru`, `en`, `zh`, `hi`, `es`, `fr`, `de` or `it` |
| `IRONPULSE_PUBLIC_URL` | The dashboard's address, linked from notifications |

(Without Docker: `IRONPULSE_TELEGRAM_BOT_TOKEN`, `IRONPULSE_TELEGRAM_CHAT_ID`, `IRONPULSE_SLACK_WEBHOOK_URL`, `IRONPULSE_WEBHOOK_URL`, `IRONPULSE_NOTIFY_LANGUAGE`.)

The same in the config file, with extra options:
```json
"notifications": {
  "language": "en",
  "min_severity": "critical",
  "dashboard_url": "https://monitoring.example.com",
  "channels": [
    { "type": "telegram", "bot_token": "${TG_TOKEN}", "chat_id": "-1001234567890" },
    { "type": "webhook", "url": "https://hooks.example.com/ironpulse",
      "headers": { "Authorization": "Bearer ${HOOK_SECRET}" } }
  ]
}
```
- `min_severity: "critical"` — only limit breaches and outages, no statistical warnings.
- A Telegram channel accepts a `url`: your own Bot API server or a proxy, if `api.telegram.org` is not reachable from the plant network.
- Webhooks receive structured JSON: `type` (`alert`, `device_offline`, `device_online`), `severity`, `title`, `text`, `timestamp`, `device_id`, `sensor_id` and an `alert` object with `kind`, `value`, `limit`, `direction`, `votes` etc. — convenient for ticketing and automation.

Delivery runs in the background and never slows polling down; server errors are retried up to three times with growing pauses. A channel missing its URL or token is disabled with a warning in the log instead of stopping the engine.

## Security.
By default the API is open — convenient for a first look, not for a networked server. Set a token:
```bash
# deploy/docker/.env
IRONPULSE_API_TOKEN=$(openssl rand -hex 32)
```
The REST API, the live WebSocket stream and `/metrics` then require `Authorization: Bearer <token>`. The dashboard asks for the token once and remembers it in the browser. `/healthz` stays open for health checks.

Also built in: request parameter validation (`400` instead of a crash), bounded WebSocket frames and headers, disconnection of stalled clients, constant-time token comparison, filesystem-safe sensor ids, an unprivileged container user and a sandboxed systemd unit.

## Monitoring Ironpulse itself.
`GET /metrics` serves Prometheus metrics — scrape it with Prometheus/Grafana or VictoriaMetrics:

| Metric | Shows |
|---|---|
| `ironpulse_sensor_value{sensor,unit}` | Latest value of each sensor |
| `ironpulse_device_up{device}` | 1 — device answers, 0 — it does not |
| `ironpulse_readings_total{sensor}` | Readings received |
| `ironpulse_alerts_total{sensor,kind,severity}` | Alerts raised |
| `ironpulse_poll_errors_total{device}` | Failed requests to a device |
| `ironpulse_notifications_total{channel,result}` | Notification deliveries |
| `ironpulse_websocket_clients`, `ironpulse_uptime_seconds`, `ironpulse_build_info{version}` | The service's own state |

```yaml
# prometheus.yml
scrape_configs:
  - job_name: ironpulse
    authorization: { credentials: "<token>" }   # if IRONPULSE_API_TOKEN is set
    static_configs: [{ targets: ["ironpulse-host:8080"] }]
```

## Exporting the data stream.
Besides the dashboard, Ironpulse can write every reading to compact binary files — for a data lake, a historian, offline analysis or model training. Disk writes happen on a separate thread: a slow or full disk never delays polling or alerts — at worst some readings miss the export, and the metrics show it.

```json
"export": { "enabled": true, "directory": "export", "segment_max_mb": 64 }
```

In Docker, set `EXPORT_ENABLED=true` in `.env` — files appear in the data volume under `/app/data/export`.

Files are named `telemetry-<time>-NNNNNN.ipseg`: 24 bytes per reading, a CRC-32C checksum per batch and the sensor list inside every file, so a file can be read on its own, even after the configuration has changed. A file still being written ends in `.part` — pick up only finished `.ipseg` files. Ironpulse does not delete them: retention of exported data is up to you.

```bash
ironpulse-export verify export/                       # check every batch's CRC
ironpulse-export dump export/ > readings.csv          # all readings as CSV
ironpulse-export dump --sensor winding_temp export/   # one sensor only
```

Format description: [`docs/export-format.md`](docs/export-format.md); a dependency-free Python reader: [`tools/export_reader/read_segment.py`](tools/export_reader/read_segment.py); metrics: `ironpulse_export_*` in `/metrics`.

## Deployment options.

| Scenario | Instructions |
|---|---|
| I just want to see it working locally | [Quick start](#quick-start) |
| My own server/VPS, nothing else runs there | [Docker Compose on a dedicated VPS](#docker-compose-on-a-dedicated-vps) |
| The server already hosts other sites with their own nginx + HTTPS | [Behind an existing nginx](#behind-an-existing-nginx-and-domain) |
| No Docker, building C++ directly | [Building from source](#building-from-source-without-docker) |

### Docker Compose on a dedicated VPS.
```bash
cd deploy/docker
cp .env.example .env          # set IRONPULSE_API_TOKEN and, optionally, notifications
docker compose up -d --build
```
The dashboard is at `http://<server-ip>:8080` (and through the bundled nginx on port 80). For real public use put a TLS certificate in front — see the next section.

### Behind an existing nginx (and domain).
If your VPS already hosts other sites with their own nginx and TLS certificates (e.g. via [certbot](https://certbot.eff.org/)), Ironpulse can join as one more site instead of fighting over ports 80/443:
```bash
cd deploy/docker
cp .env.example .env
# Set IRONPULSE_BIND_HOST=127.0.0.1 in .env so ironpulse is reachable
# only from this machine, not directly from the internet.
docker compose -f docker-compose.yml -f docker-compose.prod.yml up -d --build
```
This binds ironpulse's ports to `127.0.0.1` only and skips the bundled nginx (`docker-compose.prod.yml`), so your existing nginx is the single TLS termination point. Then add a vhost — [`deploy/nginx/anton-tests.ru.conf`](deploy/nginx/anton-tests.ru.conf) is a ready example; copy it and substitute your domain and certificate paths:
```bash
sudo cp deploy/nginx/anton-tests.ru.conf /etc/nginx/sites-available/your-domain.tld
sudo ln -s /etc/nginx/sites-available/your-domain.tld /etc/nginx/sites-enabled/
sudo nginx -t && sudo systemctl reload nginx
```

### Production checklist.
- [ ] `IRONPULSE_API_TOKEN` is set (long and random: `openssl rand -hex 32`)
- [ ] `IRONPULSE_BIND_HOST=127.0.0.1` if a reverse proxy sits in front (never expose the raw app port)
- [ ] The TLS certificate is valid and auto-renews (`certbot renew --dry-run`)
- [ ] `persistence_enabled: true` if history must survive restarts
- [ ] `retention_hours` fits your disk — about 16 bytes per reading; old records are removed automatically every hour
- [ ] `devices` point at your **real** Modbus devices, not the bundled simulator, and every sensor has `limits`
- [ ] Notifications tested: temporarily set a limit below the current value and check the message arrives
- [ ] Enough disk space for the build — Docker temporarily needs ~3-4 GB although the final image is under 250 MB (`docker builder prune` afterwards)
- [ ] All health checks are green: `docker compose ps` shows every service as `healthy`

### Building from source (without Docker).
You need a C++20 compiler (GCC ≥ 12 or Clang ≥ 15), CMake ≥ 3.20, Ninja and OpenSSL ≥ 3 (`libssl-dev`, for HTTPS notifications). Everything else (Asio, spdlog, nlohmann/json, cpp-httplib, Catch2) is fetched automatically.
```bash
cmake --preset debug
cmake --build --preset debug -j$(nproc)
ctest --preset debug --output-on-failure
```
Other presets: `release`, `asan` (AddressSanitizer + UBSan), `tsan` (ThreadSanitizer).

To actually run it you need Modbus devices to poll — use the bundled simulator for a self-contained demo:
```bash
pip install -r tools/modbus_simulator/requirements.txt
python3 tools/modbus_simulator/simulator.py --port 5020 &

./build/debug/ironpulse deploy/config/config.example.json
```
`ironpulse --version` prints the version. See [`deploy/systemd/ironpulse.service`](deploy/systemd/ironpulse.service) for a hardened systemd unit; it reads secrets from `/etc/ironpulse/ironpulse.env`.

## Troubleshooting.
**"Port is already allocated" when starting Docker.** Something else uses that port. Stop it or change the port in `.env`.

**Ironpulse exits with `Failed to load config ... config: devices[0]...`.** The config has an error; the message names the exact field and the problem.

**The dashboard asks for a token I never set.** A token is set in the environment (`IRONPULSE_API_TOKEN` in `.env`, or `api_token` in the config). Enter it, or remove it if access should be open.

**The device shows "offline" and the log says `device unreachable: cannot connect`.** Ironpulse can't reach `host`/`port`. With Docker Compose, `host` must be the Docker service name (e.g. `simulator`), not `127.0.0.1`.

**The log says `poll failed: device exception code 2`.** The device answers but refuses the requested registers: code 2 means "illegal address". Check `address`, `register_type` and `data_type` against the device's register map (zero- vs one-based numbering and holding vs input are the usual mix-ups).

**Values look like garbage (huge numbers, negative instead of positive).** Almost always `data_type` or `word_order`: try `word_order: "little"` for 32-bit values, `int16` instead of `uint16` for quantities that can be negative, and check `scale`.

**Notifications don't arrive.** Check the log: `Notification via telegram failed (HTTP 401)` — wrong bot token; `HTTP 400` — wrong `chat_id` or the bot isn't in the chat; `network error` — the server has no internet access (Telegram channels accept a proxy or your own Bot API server in `url`). The `ironpulse_notifications_total{result="error"}` metric counts failed deliveries.

**I changed a file but Docker still serves the old version.** `docker compose build --no-cache`, and if that's not enough, `docker builder prune -af` first.

## Architecture (for developers).
```
Modbus TCP devices → protocol (polling, register decoding) → EventBus
                                                                │
           ┌──────────────────────┬──────────────────────┬──────┴────────────┐
           ▼                      ▼                      ▼                   ▼
storage (ring buffer + WAL)  analytics (limits,     api (REST, WebSocket,  metrics
                             detectors, quorum)     /metrics) → dashboard
                                     │
                                     ▼
                             notify (Telegram, Slack, webhook)
```
Each layer (`core`, `protocol`, `storage`, `ingest`, `analytics`, `api`, `notify`) is an independent CMake target with its own tests. Layers talk through the `EventBus`, not directly. See [`docs/architecture.md`](docs/architecture.md) for the concurrency model and [`docs/adr/`](docs/adr/) for individual decisions.

### API reference.
Full contract: [`docs/openapi.yaml`](docs/openapi.yaml) (OpenAPI 3 — open it in Swagger UI or import it into Postman).

| Endpoint | Description |
|---|---|
| `GET /api/v1/devices` | Devices: `id`, `online`, `poll_interval_ms`, sensor list |
| `GET /api/v1/sensors` | Sensors: `id`, `name`, `unit`, `device_id`, `limits`, latest value |
| `GET /api/v1/series/{sensor_id}?since=<seconds>` | History for the period (default 300 s; with WAL, the whole retention period). Long ranges are thinned to 10,000 points |
| `GET /api/v1/series/{sensor_id}?since=<seconds>&format=csv` | The same as CSV, never thinned |
| `GET /api/v1/alerts?limit=<n>` | Recent alerts, newest first (1–1000, default 50): `kind`, `severity`, `value`, `limit`, `direction`, `votes`, `detectors_total`, `confidence` |
| `GET /api/v1/config` | `ws_port`, `ws_host`, `auth_required`, `version` — what the dashboard needs to start (no token required) |
| `GET /metrics` | Prometheus metrics |
| `GET /healthz` | Liveness and version (no token required) |
| `WS /live` (`/ws/live` behind a reverse proxy) | Live push: `reading`, `anomaly`, `device_status` events. With a token: `?token=<token>` |

### Engineering practices.
- Strict compiler warnings (`-Wall -Wextra -Wpedantic -Wconversion ...`), a warning-free build; optionally as errors (`IRONPULSE_WARNINGS_AS_ERRORS`)
- 131 tests: unit tests for every component, HTTP API and WebSocket handshake tests on real sockets, end-to-end tests with a fake Modbus device over TCP (decoding, limits, timeouts, outage and recovery). All green under AddressSanitizer/UBSan and ThreadSanitizer
- CI: GCC and Clang, sanitizers, `clang-format`, `ruff` for the simulator, translation completeness, Docker build and a Compose smoke test that checks live data — [`.github/workflows/ci.yml`](.github/workflows/ci.yml)
- Tag-driven releases: native amd64 and arm64 image builds published to GitHub Container Registry — [`.github/workflows/release.yml`](.github/workflows/release.yml)
- Multi-stage Docker build, unprivileged container user, health checks on every service
- The dashboard has zero external runtime dependencies (Chart.js is vendored), so it works in restricted or offline networks

### Roadmap.
- [x] Async Modbus TCP client + frame codec
- [x] Time-series storage with WAL persistence and retention
- [x] Z-score, EWMA and CUSUM detection combined by a quorum-voting `RuleEngine`
- [x] REST API + hand-written WebSocket (RFC 6455) live push
- [x] Environment-driven configuration for containers
- [x] Google Benchmark suite for storage/protocol hot paths
- [x] Integration test: full pipeline from a mock device to a published alert
- [x] Automatic periodic WAL retention
- [x] Multi-architecture Docker images (linux/amd64 + linux/arm64)
- [x] Several sensors per device, data types, scaling, input registers
- [x] Hard limits, alert severity, cooldown
- [x] Notifications: Telegram, Slack, webhook — in 8 languages
- [x] Token access, Prometheus metrics, CSV export
- [x] Export of every reading to CRC-checked files (`ingest` module, formerly apollonian_core_ingestor)
- [ ] Modbus RTU (RS-485) directly, without a gateway
- [ ] OPC UA and MQTT as data sources
- [ ] Alert acknowledgement by operators and an audit log
- [ ] Users and roles instead of a single token

## Contributing.
Issues and pull requests are welcome. Before opening a PR:
```bash
cmake --preset debug && cmake --build --preset debug -j$(nproc)
ctest --preset debug --output-on-failure
find include src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format --dry-run --Werror
ruff check tools/ && ruff format --check tools/
```
CI runs the same checks (plus sanitizers and the Docker build) on every pull request. Changes per version are listed in [CHANGELOG.md](CHANGELOG.md).

## License.
MIT — see [LICENSE](LICENSE). Free to use, modify and deploy, including commercially.
