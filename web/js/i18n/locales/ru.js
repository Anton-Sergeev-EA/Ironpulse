I18n.register("ru", {
    "page.title": "Ironpulse from Sergeev Anton — промышленная телеметрия",
    "language.label": "Язык интерфейса",

    "connection.connecting": "Подключение...",
    "connection.online": "Подключено",
    "connection.reconnecting": "Переподключение...",

    "about.description":
        "<strong>Ironpulse</strong> — движок промышленной телеметрии и обнаружения аномалий " +
        "в реальном времени. Опрашивает датчики по протоколу Modbus TCP, сохраняет показания " +
        "как временные ряды (в памяти и на диске через write-ahead log), обнаруживает " +
        "аномалии с помощью нескольких статистических стратегий (z-score, EWMA, CUSUM) с " +
        "голосованием по кворуму, следит за заданными пределами, рассылает уведомления в Telegram, Slack и по webhook, и отдаёт результат через REST + WebSocket API на этот " +
        "дашборд в реальном времени.",
    "about.stack":
        "<strong>Технологии:</strong> C++20 (ядро, Modbus-клиент, хранилище, аналитика, REST " +
        "и WebSocket API), JavaScript (дашборд), Python (симулятор Modbus-устройств для демо), " +
        "Docker/Docker Compose и nginx (развёртывание).",

    "panel.devices": "Устройства",
    "panel.readings": "Показания в реальном времени",
    "panel.alerts": "Активные алерты",
    "alerts.empty": "Аномалий не обнаружено",

    "device.online": "в сети",
    "device.offline": "не в сети",

    "alert.confirmed": "аномалия подтверждена детекторами: {votes} из {total}",
    "alert.confidence": "уверенность {percent}%",

    "auth.title": "Нужен токен доступа",
    "auth.description": "Этот сервер защищён. Введите API-токен, который выдал администратор.",
    "auth.placeholder": "API-токен",
    "auth.submit": "Войти",
    "auth.invalid": "Неверный токен",
    "auth.unreachable": "Сервер недоступен, попробуйте ещё раз",
    "alert.limit_high": "значение {value} выше предела {limit}",
    "alert.limit_low": "значение {value} ниже предела {limit}",
    "severity.critical": "критично",
    "severity.warning": "предупреждение",
    "chart.download": "Скачать CSV",
    "chart.limit_high": "макс.",
    "chart.limit_low": "мин.",
    "devices.empty": "Устройства не настроены",
    "device.sensors": "датчиков: {count}",
});
