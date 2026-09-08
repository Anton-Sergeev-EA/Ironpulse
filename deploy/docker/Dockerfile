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
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake --preset release \
    && cmake --build --preset release -j"$(nproc)"

# ---------------------------------------------------------------------------
# Stage 2: runtime — minimal image, no compiler/toolchain
# ---------------------------------------------------------------------------
FROM debian:bookworm-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        wget \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --create-home --shell /usr/sbin/nologin ironpulse

WORKDIR /app
COPY --from=build /src/build/release/ironpulse /app/ironpulse
COPY web/ /app/web/
COPY deploy/config/config.example.json /app/config.json

RUN chown -R ironpulse:ironpulse /app
USER ironpulse

EXPOSE 8080 8081

# Works standalone (`docker run`) as well as under Compose, which adds its
# own equivalent healthcheck in docker-compose.yml with the configured
# HTTP_PORT — this one covers the image's built-in default of 8080 for
# anyone running the container directly.
HEALTHCHECK --interval=10s --timeout=3s --start-period=10s --retries=5 \
    CMD wget --quiet --tries=1 --spider http://127.0.0.1:${IRONPULSE_HTTP_PORT:-8080}/healthz || exit 1

ENTRYPOINT ["/app/ironpulse"]
CMD ["/app/config.json"]
