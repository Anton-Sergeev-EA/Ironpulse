# syntax=docker/dockerfile:1

# ---------------------------------------------------------------------------
# Stage 1: build
# ---------------------------------------------------------------------------
FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        git \
        ca-certificates \
        libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# HTTPLIB_REQUIRE_OPENSSL: fail the build rather than silently producing an
# image that cannot deliver notifications to https:// endpoints (Telegram,
# Slack and most webhooks).
RUN cmake --preset release -DHTTPLIB_REQUIRE_OPENSSL=ON \
    && cmake --build --preset release -j"$(nproc)" \
    && strip build/release/ironpulse

# ---------------------------------------------------------------------------
# Stage 2: runtime — minimal image, no compiler/toolchain
# ---------------------------------------------------------------------------
FROM debian:bookworm-slim AS runtime

LABEL org.opencontainers.image.title="Ironpulse" \
      org.opencontainers.image.description="Industrial telemetry and anomaly detection engine (Modbus TCP)" \
      org.opencontainers.image.source="https://github.com/Anton-Sergeev-EA/Ironpulse" \
      org.opencontainers.image.licenses="MIT"

# ca-certificates + libssl3: TLS for outgoing notifications.
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        libssl3 \
        wget \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --create-home --shell /usr/sbin/nologin ironpulse

WORKDIR /app
COPY --from=build /src/build/release/ironpulse /app/ironpulse
COPY web/ /app/web/
COPY deploy/config/config.example.json /app/config.json

RUN mkdir -p /app/data && chown -R ironpulse:ironpulse /app
USER ironpulse
VOLUME ["/app/data"]

EXPOSE 8080 8081

# Works standalone (`docker run`) as well as under Compose, which adds its
# own equivalent healthcheck in docker-compose.yml with the configured
# HTTP_PORT — this one covers the image's built-in default of 8080 for
# anyone running the container directly.
HEALTHCHECK --interval=10s --timeout=3s --start-period=10s --retries=5 \
    CMD wget --quiet --tries=1 --spider http://127.0.0.1:${IRONPULSE_HTTP_PORT:-8080}/healthz || exit 1

ENTRYPOINT ["/app/ironpulse"]
CMD ["/app/config.json"]
