# ADR 0003: Environment-variable config overrides for container deployment

## Status
Accepted and implemented.

## Context
Early versions configured ironpulse purely from a JSON file baked into
(or mounted into) the container. That created a real footgun in Docker
Compose: the port the container's REST server binds to *inside* the
container (from the JSON file) and the port Compose maps it to *on the
host* (from `docker-compose.yml`'s `ports:` list) were two independent
numbers that had to be kept in sync by hand. Change one without the
other — a completely natural mistake when working around a host port
conflict — and `GET /api/v1/config` (which the dashboard uses to find the
WebSocket server, see ADR notes in `web/js/ws-client.js`) reports a port
that isn't actually reachable from outside the container.

## Decision
`AppConfig::load_from_file` now applies `IRONPULSE_*` environment
variable overrides on top of the JSON file (see `src/core/config.cpp`).
`docker-compose.yml` reads port numbers from a single `.env` file and
uses the *same* value both to set `IRONPULSE_HTTP_PORT` /
`IRONPULSE_WS_PORT` inside the container's environment and to map
`${HTTP_PORT}:${HTTP_PORT}` / `${WS_PORT}:${WS_PORT}` on the `ports:`
list. One number, one place to change it, structurally unable to drift.

This follows standard twelve-factor-app practice: values that vary
between environments (dev machine vs. VDS vs. CI) belong in the
environment, not hardcoded in a file that's the same across all of them.
The JSON file remains the source of truth for values that *don't* vary
by environment — the list of Modbus devices, detector thresholds — since
expressing a list of objects as environment variables is awkward and not
worth it here.

## Consequences
- Running `docker compose up` after only editing `.env` just works — no
  need to also touch `deploy/config/config.docker.json`.
- Native (non-Docker) runs are unaffected unless the operator explicitly
  sets `IRONPULSE_*` variables; the JSON file's values remain the
  defaults.
- Precedence is: built-in default < JSON file < environment variable.
  This order (env wins) matches every mainstream twelve-factor framework
  and is what operators expect.
