I18n.register("en", {
    "page.title": "Ironpulse from Sergeev Anton — Industrial Telemetry",
    "language.label": "Interface language",

    "connection.connecting": "Connecting...",
    "connection.online": "Connected",
    "connection.reconnecting": "Reconnecting...",

    "about.description":
        "<strong>Ironpulse</strong> is a real-time industrial telemetry and anomaly detection " +
        "engine. It polls sensors over Modbus TCP, stores readings as time series (in memory " +
        "and on disk via a write-ahead log), detects anomalies with several statistical " +
        "strategies (z-score, EWMA, CUSUM) combined by quorum voting, and streams the results " +
        "to this dashboard in real time through a REST + WebSocket API.",
    "about.stack":
        "<strong>Tech stack:</strong> C++20 (core, Modbus client, storage, analytics, REST and " +
        "WebSocket API), JavaScript (dashboard), Python (Modbus device simulator for demos), " +
        "Docker/Docker Compose and nginx (deployment).",

    "panel.devices": "Devices",
    "panel.readings": "Live readings",
    "panel.alerts": "Active alerts",
    "alerts.empty": "No anomalies detected",

    "device.online": "online",
    "device.offline": "offline",

    "alert.confirmed": "anomaly confirmed by {votes} of {total} detectors",
    "alert.confidence": "confidence {percent}%",
});
