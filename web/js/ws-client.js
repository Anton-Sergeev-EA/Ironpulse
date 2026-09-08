// Connects to ironpulse's live telemetry WebSocket and routes incoming
// events to the charts, devices, and alerts panels.
//
// The REST API and the WebSocket server listen on different ports (see
// main.cpp — cpp-httplib and the custom WS server are separate listeners).
// This client tries a same-origin "/ws/live" connection FIRST: in any
// production deployment behind a reverse proxy (the Docker-bundled nginx,
// or a host nginx per deploy/nginx/anton-tests.ru.conf), that's the only
// path that's actually reachable — the app's raw ports are intentionally
// not exposed externally (see docker-compose.prod.yml). Trying the direct
// port first would mean a guaranteed failed connection attempt (and a
// multi-second delay) on every single page load in that — the most
// common real-world — deployment shape.
//
// If the same-origin attempt fails (no reverse proxy in front, e.g. local
// development with `docker compose up` talking directly to ironpulse's
// ports), this client falls back to asking the REST API which port the
// WS server is actually on (GET /api/v1/config) and connecting directly.
//
// Expected message shapes:
//   { "type": "reading", "sensor_id": "...", "value": 65.3, "timestamp": ... }
//   { "type": "anomaly",  "sensor_id": "...", "message": "...", "confidence": 0.87, "timestamp": ... }
//   { "type": "device_status", "device_id": "...", "online": true }
const WsClient = {
    socket: null,
    reconnectDelayMs: 2000,
    sameOriginAttemptTimeoutMs: 1500,
    resolvedUrl: null, // cached once a connection strategy succeeds

    connect() {
        if (this.resolvedUrl) {
            this._connectTo(this.resolvedUrl);
            return;
        }
        this._trySameOriginThenFallback();
    },

    _wsProtocol() {
        return window.location.protocol === "https:" ? "wss:" : "ws:";
    },

    _trySameOriginThenFallback() {
        const sameOriginUrl = `${this._wsProtocol()}//${window.location.host}/ws/live`;
        const probe = new WebSocket(sameOriginUrl);
        let settled = false;

        const fallbackTimer = setTimeout(() => {
            if (settled) return;
            settled = true;
            probe.close();
            this._connectDirectViaConfig();
        }, this.sameOriginAttemptTimeoutMs);

        probe.addEventListener("open", () => {
            if (settled) return;
            settled = true;
            clearTimeout(fallbackTimer);
            this.resolvedUrl = sameOriginUrl;
            this.socket = probe;
            this._wireSocketEvents(probe);
            this._setConnectionStatus(true);
        });

        probe.addEventListener("error", () => {
            if (settled) return;
            settled = true;
            clearTimeout(fallbackTimer);
            this._connectDirectViaConfig();
        });
    },

    async _connectDirectViaConfig() {
        try {
            const response = await fetch("/api/v1/config");
            const config = await response.json();
            const host = config.ws_host || window.location.hostname;
            const url = `${this._wsProtocol()}//${host}:${config.ws_port}/live`;
            this.resolvedUrl = url;
            this._connectTo(url);
        } catch (err) {
            console.error("Failed to discover WebSocket port from /api/v1/config:", err);
            this._setConnectionStatus(false);
            setTimeout(() => this.connect(), this.reconnectDelayMs);
        }
    },

    _connectTo(url) {
        this.socket = new WebSocket(url);
        this._wireSocketEvents(this.socket);
    },

    _wireSocketEvents(socket) {
        socket.addEventListener("open", () => this._setConnectionStatus(true));
        socket.addEventListener("close", () => {
            this._setConnectionStatus(false);
            setTimeout(() => this.connect(), this.reconnectDelayMs);
        });
        socket.addEventListener("error", () => socket.close());
        socket.addEventListener("message", (event) => this._handleMessage(event));
    },

    _handleMessage(event) {
        let payload;
        try {
            payload = JSON.parse(event.data);
        } catch {
            return;
        }

        switch (payload.type) {
            case "reading":
                ChartsPanel.pushPoint(payload.sensor_id, payload.timestamp, payload.value);
                break;
            case "anomaly":
                AlertsPanel.addAlert(payload);
                break;
            case "device_status":
                DevicesPanel.setStatus(payload.device_id, payload.online);
                break;
        }
    },

    _setConnectionStatus(online) {
        const dot = document.getElementById("connection-dot");
        const label = document.getElementById("connection-label");
        dot.className = `dot ${online ? "online" : "offline"}`;
        label.textContent = online ? "Подключено" : "Переподключение...";
    },
};

document.addEventListener("DOMContentLoaded", () => {
    WsClient.connect();
});
