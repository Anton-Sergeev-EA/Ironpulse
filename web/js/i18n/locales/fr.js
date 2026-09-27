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
        "(z-score, EWMA, CUSUM) combinées par vote à quorum, et diffuse les résultats en temps " +
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
});
