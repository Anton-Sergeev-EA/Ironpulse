I18n.register("es", {
    "page.title": "Ironpulse from Sergeev Anton — Telemetría industrial",
    "language.label": "Idioma de la interfaz",

    "connection.connecting": "Conectando...",
    "connection.online": "Conectado",
    "connection.reconnecting": "Reconectando...",

    "about.description":
        "<strong>Ironpulse</strong> es un motor de telemetría industrial y detección de anomalías " +
        "en tiempo real. Consulta los sensores mediante Modbus TCP, almacena las lecturas como " +
        "series temporales (en memoria y en disco mediante un write-ahead log), detecta anomalías " +
        "con varias estrategias estadísticas (z-score, EWMA, CUSUM) combinadas por votación de " +
        "quórum, vigila los límites configurados, envía notificaciones a Telegram, Slack y webhooks y transmite los resultados a este panel en tiempo real a través de una API REST + WebSocket.",
    "about.stack":
        "<strong>Tecnologías:</strong> C++20 (núcleo, cliente Modbus, almacenamiento, analítica, " +
        "API REST y WebSocket), JavaScript (panel), Python (simulador de dispositivos Modbus para " +
        "demostraciones), Docker/Docker Compose y nginx (despliegue).",

    "panel.devices": "Dispositivos",
    "panel.readings": "Lecturas en tiempo real",
    "panel.alerts": "Alertas activas",
    "alerts.empty": "No se han detectado anomalías",

    "device.online": "en línea",
    "device.offline": "desconectado",

    "alert.confirmed": "anomalía confirmada por {votes} de {total} detectores",
    "alert.confidence": "confianza {percent} %",

    "auth.title": "Se requiere un token de acceso",
    "auth.description": "Este servidor está protegido. Introduce el token de API que te proporcionó el administrador.",
    "auth.placeholder": "Token de API",
    "auth.submit": "Entrar",
    "auth.invalid": "Token no válido",
    "auth.unreachable": "Servidor no disponible, inténtalo de nuevo",
    "alert.limit_high": "el valor {value} supera el límite {limit}",
    "alert.limit_low": "el valor {value} está por debajo del límite {limit}",
    "severity.critical": "crítico",
    "severity.warning": "advertencia",
    "chart.download": "Descargar CSV",
    "chart.limit_high": "máx.",
    "chart.limit_low": "mín.",
    "devices.empty": "No hay dispositivos configurados",
    "device.sensors": "sensores: {count}",
});
