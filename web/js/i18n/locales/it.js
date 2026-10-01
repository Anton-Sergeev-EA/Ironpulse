I18n.register("it", {
    "page.title": "Ironpulse from Sergeev Anton — Telemetria industriale",
    "language.label": "Lingua dell’interfaccia",

    "connection.connecting": "Connessione in corso...",
    "connection.online": "Connesso",
    "connection.reconnecting": "Riconnessione in corso...",

    "about.description":
        "<strong>Ironpulse</strong> è un motore di telemetria industriale e rilevamento delle " +
        "anomalie in tempo reale. Interroga i sensori tramite Modbus TCP, memorizza le letture " +
        "come serie temporali (in memoria e su disco tramite un write-ahead log), rileva le " +
        "anomalie con diverse strategie statistiche (z-score, EWMA, CUSUM) combinate tramite " +
        "votazione a quorum, sorveglia i limiti configurati, invia notifiche a Telegram, Slack e webhook e trasmette i risultati a questa dashboard in tempo reale tramite " +
        "un’API REST + WebSocket.",
    "about.stack":
        "<strong>Tecnologie:</strong> C++20 (core, client Modbus, archiviazione, analisi, API REST " +
        "e WebSocket), JavaScript (dashboard), Python (simulatore di dispositivi Modbus per le " +
        "demo), Docker/Docker Compose e nginx (distribuzione).",

    "panel.devices": "Dispositivi",
    "panel.readings": "Letture in tempo reale",
    "panel.alerts": "Allarmi attivi",
    "alerts.empty": "Nessuna anomalia rilevata",

    "device.online": "online",
    "device.offline": "offline",

    "alert.confirmed": "anomalia confermata da {votes} rilevatori su {total}",
    "alert.confidence": "affidabilità {percent}%",

    "auth.title": "Token di accesso richiesto",
    "auth.description": "Questo server è protetto. Inserisci il token API fornito dall’amministratore.",
    "auth.placeholder": "Token API",
    "auth.submit": "Accedi",
    "auth.invalid": "Token non valido",
    "auth.unreachable": "Server non raggiungibile, riprova",
    "alert.limit_high": "il valore {value} supera il limite {limit}",
    "alert.limit_low": "il valore {value} è sotto il limite {limit}",
    "severity.critical": "critico",
    "severity.warning": "avviso",
    "chart.download": "Scarica CSV",
    "chart.limit_high": "max",
    "chart.limit_low": "min",
    "devices.empty": "Nessun dispositivo configurato",
    "device.sensors": "sensori: {count}",
});
