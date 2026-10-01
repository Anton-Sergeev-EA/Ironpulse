I18n.register("fr", {
    "page.title": "Ironpulse from Sergeev Anton — Télémétrie industrielle",
    "language.label": "Langue de l’interface",

    "connection.connecting": "Connexion…",
    "connection.online": "Connecté",
    "connection.reconnecting": "Reconnexion…",

    "about.description":
        "<strong>Ironpulse</strong> est un moteur de télémétrie industrielle et de détection " +
        "d’anomalies en temps réel. Il interroge les capteurs via Modbus TCP, enregistre les " +
        "mesures sous forme de séries temporelles (en mémoire et sur disque grâce à un " +
        "write-ahead log), détecte les anomalies à l’aide de plusieurs stratégies statistiques " +
        "(z-score, EWMA, CUSUM) combinées par vote à quorum, surveille les limites configurées, envoie des notifications vers Telegram, Slack et des webhooks, et diffuse les résultats en temps " +
        "réel vers ce tableau de bord via une API REST + WebSocket.",
    "about.stack":
        "<strong>Technologies :</strong> C++20 (cœur, client Modbus, stockage, analytique, API " +
        "REST et WebSocket), JavaScript (tableau de bord), Python (simulateur d’équipements " +
        "Modbus pour les démos), Docker/Docker Compose et nginx (déploiement).",

    "panel.devices": "Équipements",
    "panel.readings": "Mesures en temps réel",
    "panel.alerts": "Alertes actives",
    "alerts.empty": "Aucune anomalie détectée",

    "device.online": "en ligne",
    "device.offline": "hors ligne",

    "alert.confirmed": "anomalie confirmée par {votes} détecteurs sur {total}",
    "alert.confidence": "confiance {percent} %",

    "auth.title": "Jeton d’accès requis",
    "auth.description": "Ce serveur est protégé. Saisissez le jeton d’API fourni par votre administrateur.",
    "auth.placeholder": "Jeton d’API",
    "auth.submit": "Se connecter",
    "auth.invalid": "Jeton invalide",
    "auth.unreachable": "Serveur injoignable, veuillez réessayer",
    "alert.limit_high": "la valeur {value} dépasse la limite {limit}",
    "alert.limit_low": "la valeur {value} est inférieure à la limite {limit}",
    "severity.critical": "critique",
    "severity.warning": "avertissement",
    "chart.download": "Télécharger le CSV",
    "chart.limit_high": "max.",
    "chart.limit_low": "min.",
    "devices.empty": "Aucun équipement configuré",
    "device.sensors": "capteurs : {count}",
});
