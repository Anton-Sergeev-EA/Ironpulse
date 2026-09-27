I18n.register("de", {
    "page.title": "Ironpulse from Sergeev Anton — Industrielle Telemetrie",
    "language.label": "Sprache der Oberfläche",

    "connection.connecting": "Verbindung wird hergestellt...",
    "connection.online": "Verbunden",
    "connection.reconnecting": "Verbindung wird wiederhergestellt...",

    "about.description":
        "<strong>Ironpulse</strong> ist eine Engine für industrielle Telemetrie und " +
        "Anomalieerkennung in Echtzeit. Sie fragt Sensoren über Modbus TCP ab, speichert " +
        "Messwerte als Zeitreihen (im Arbeitsspeicher und per Write-Ahead-Log auf der " +
        "Festplatte), erkennt Anomalien mit mehreren statistischen Verfahren (z-Score, EWMA, " +
        "CUSUM) und Quorum-Abstimmung und überträgt die Ergebnisse über eine REST- und " +
        "WebSocket-API in Echtzeit an dieses Dashboard.",
    "about.stack":
        "<strong>Technologien:</strong> C++20 (Kern, Modbus-Client, Speicherung, Analytik, REST- " +
        "und WebSocket-API), JavaScript (Dashboard), Python (Modbus-Gerätesimulator für Demos), " +
        "Docker/Docker Compose und nginx (Bereitstellung).",

    "panel.devices": "Geräte",
    "panel.readings": "Messwerte in Echtzeit",
    "panel.alerts": "Aktive Alarme",
    "alerts.empty": "Keine Anomalien erkannt",

    "device.online": "online",
    "device.offline": "offline",

    "alert.confirmed": "Anomalie von {votes} von {total} Detektoren bestätigt",
    "alert.confidence": "Konfidenz {percent} %",
});
