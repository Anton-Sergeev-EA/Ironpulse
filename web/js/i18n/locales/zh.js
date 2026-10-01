I18n.register("zh", {
    "page.title": "Ironpulse from Sergeev Anton — 工业遥测",
    "language.label": "界面语言",

    "connection.connecting": "正在连接…",
    "connection.online": "已连接",
    "connection.reconnecting": "正在重新连接…",

    "about.description":
        "<strong>Ironpulse</strong> 是一款实时工业遥测与异常检测引擎。它通过 Modbus TCP " +
        "协议轮询传感器，将读数存储为时间序列（保存在内存中，并通过预写日志持久化到磁盘），" +
        "使用多种统计策略（z-score、EWMA、CUSUM）并以法定票数投票的方式检测异常，监控设定的限值，并通过 Telegram、Slack 和 webhook 发送通知，" +
        "再通过 REST + WebSocket API 将结果实时推送到本仪表板。",
    "about.stack":
        "<strong>技术栈：</strong>C++20（核心、Modbus 客户端、存储、分析、REST 与 WebSocket API）、" +
        "JavaScript（仪表板）、Python（用于演示的 Modbus 设备模拟器）、" +
        "Docker/Docker Compose 和 nginx（部署）。",

    "panel.devices": "设备",
    "panel.readings": "实时读数",
    "panel.alerts": "当前告警",
    "alerts.empty": "未检测到异常",

    "device.online": "在线",
    "device.offline": "离线",

    "alert.confirmed": "异常已由 {total} 个检测器中的 {votes} 个确认",
    "alert.confidence": "置信度 {percent}%",

    "auth.title": "需要访问令牌",
    "auth.description": "此服务器受保护。请输入管理员提供的 API 令牌。",
    "auth.placeholder": "API 令牌",
    "auth.submit": "登录",
    "auth.invalid": "令牌无效",
    "auth.unreachable": "服务器无法访问，请重试",
    "alert.limit_high": "数值 {value} 高于上限 {limit}",
    "alert.limit_low": "数值 {value} 低于下限 {limit}",
    "severity.critical": "严重",
    "severity.warning": "警告",
    "chart.download": "下载 CSV",
    "chart.limit_high": "上限",
    "chart.limit_low": "下限",
    "devices.empty": "未配置设备",
    "device.sensors": "传感器：{count}",
});
