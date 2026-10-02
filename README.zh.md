# Ironpulse

**实时工业监控系统：持续监视设备传感器的读数，在故障发生之前提醒您出了问题。**

[![CI](https://img.shields.io/badge/CI-GitHub_Actions-2088FF?logo=githubactions&logoColor=white)](.github/workflows/ci.yml)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](CMakeLists.txt)
[![Docker](https://img.shields.io/badge/Docker-amd64%20%7C%20arm64-2496ED?logo=docker&logoColor=white)](deploy/docker)
[![Languages](https://img.shields.io/badge/UI-8%20languages-8A2BE2)](#interface-languages)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-131%20passing-brightgreen)](tests)

[Русский](README.md) · [English](README.en.md) · **中文** · [हिन्दी](README.hi.md) · [Español](README.es.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md)

## 这是什么？
设想一座有几十个传感器的工厂：变压器绕组温度、电机轴承振动、水泵压力。如今往往要有人在屏幕上注意到——更糟的是听到一声巨响——才意识到某个部件正在过热或即将损坏。

**Ironpulse 持续监视这些传感器，一旦读数超出允许范围或开始出现异常，立即通知您**——使用的是预测性维护工程师所用的统计方法。硬限值捕捉已经危险的情况；统计方法则能在数值触及限值之前很久，就同时捕捉到突发尖峰和缓慢的、逐渐恶化的趋势。

您将获得：
- **8 种语言的实时仪表板**——每个传感器以自身单位显示的当前值、带限值线的图表，以及带严重级别的告警列表。
- **通知**：发送到 Telegram、Slack 或任意 webhook——使用您团队的语言，包括设备失联和恢复的消息。
- **磁盘上的历史数据**——重启不会丢失任何数据，任何传感器都可导出整个保留期内的 CSV。
- **供其他系统使用的 API 和指标**：REST、WebSocket，以及供 Prometheus/Grafana 使用的 `/metrics`。
- **基于令牌的访问保护**，覆盖 API、实时数据流和指标。

Ironpulse 通过 **Modbus TCP** 与真实的工业设备通信——全球大量的传感器、PLC 和仪表都使用这一协议。它读取保持寄存器和输入寄存器，支持任意字序的 16 位、32 位整数和浮点数，并应用比例和偏移——因此能处理真实设备的寄存器映射，而不仅仅是演示数据。

### 适合谁使用？
- **工程师和技术人员**：需要一个轻量级、可自行托管的监控工具，而无需购买（或等待 IT 部门批准）商业 SCADA/IIoT 平台。
- **开发者**：希望参考一个生产级的 C++20 代码库——异步网络、手写的 WebSocket 服务器、REST API、容器化部署和完整的测试套件——且不依赖重型框架。
- **学生和爱好者**：想了解工业监控流水线从头到尾究竟如何运作。

运行它**不需要**懂 C++——参见下面的[快速开始](#quick-start)。Docker 会构建一切。

## 界面效果
左侧是设备及其连接状态。中间是每个传感器一张卡片：名称、以传感器单位显示的大号当前值（超出限值时显示为红色）、带虚线限值线的实时图表，以及 CSV 导出按钮。右侧是告警列表：红色“严重”表示超出限值，黄色“警告”表示统计异常。所有内容实时更新、无需刷新页面，在控制室大屏和手机上同样好用。

<a id="interface-languages"></a>
## 界面语言
仪表板和通知提供 8 种语言：**俄语**（主要语言）、英语、中文、印地语、西班牙语、法语、德语和意大利语——切换器位于右上角。本说明文档同样提供这 8 种语言。仪表板语言按以下顺序确定：
1. 地址中的 `?lang=` 参数，例如 `http://localhost:8080/?lang=zh`（便于分享链接）；
2. 此前在该浏览器中选择的语言；
3. 浏览器语言（如果受支持）；
4. 否则使用俄语。

通知语言在配置中设置（`notifications.language`）——参见[通知](#notifications)。

仪表板的翻译位于 [`web/js/i18n/locales/`](web/js/i18n/locales)：要添加一种语言，复制 `ru.js`、翻译其中的值，并将文件加入 `web/index.html` 以及 [`web/js/i18n/i18n.js`](web/js/i18n/i18n.js) 中的 `supported` 列表。CI 会检查所有语言是否包含完全相同的字符串集合。

<a id="quick-start"></a>
## 快速开始
您只需要安装 [Docker](https://docs.docker.com/get-docker/)。它会在容器内构建其余一切（C++ 编译器和所有库），您的电脑保持干净。
```bash
git clone https://github.com/Anton-Sergeev-EA/Ironpulse.git
cd Ironpulse/deploy/docker
docker compose up --build
```
等待完成（首次运行需要几分钟——C++ 从源代码编译），然后打开：
- **http://localhost:8080** —— 实时仪表板
- **http://localhost:8080/api/v1/sensors** —— 原始 API（如果您感兴趣）

就这么简单。现在您正在监视两台模拟设备——一台变压器（绕组和油温）和一台水泵（轴承振动和压力）。模拟器会不时产生尖峰和持续偏移——在“当前告警”面板中观察警告和严重事件的出现。

每次发布都会提供适用于 amd64 和 arm64（树莓派、ARM 网关）的预构建镜像：
```bash
docker pull ghcr.io/anton-sergeev-ea/ironpulse:latest
```

### 遇到问题？
- **端口已被占用？** 如果 8080 上已经运行着其他程序（Jenkins、其他仪表板等），这很常见。在 `deploy/docker/` 中把 `.env.example` 复制为 `.env`，将 `HTTP_PORT`/`WS_PORT`/`NGINX_PORT` 改为空闲端口，然后重新运行 `docker compose up --build`。一个文件，一处修改。
- **仍然不行？** 参见[故障排除](#troubleshooting)。

## 屏幕上的内容和术语含义

| 术语 | 通俗解释 |
|---|---|
| **Modbus** | 一种历史悠久但仍被工业传感器和控制器广泛用于与软件通信的“语言”。Ironpulse 直接使用它。 |
| **寄存器** | 设备上存放读数的存储单元。每台 Modbus 设备的文档中都有寄存器映射表，说明每个地址的含义。 |
| **限值** | 数值不得越过的边界，例如绕组温度高于 90 °C。越过限值会产生**严重**告警。 |
| **异常** | 不符合传感器近期正常模式的读数——可能是突然的尖峰，也可能是持续过久的缓慢漂移。它是**警告**：数值仍在限值之内，但表现异常。 |
| **置信度** | 系统对被标记的读数确实是异常而非噪声的把握程度。越高越确定。 |
| **Z-score、EWMA、CUSUM** | 三种统计检测方法，各自擅长不同*类型*的问题（参见[异常检测的工作原理](#how-anomaly-detection-works)）。Ironpulse 同时运行多种方法，只有当足够多的方法达成一致时才报警——减少误报。 |
| **WAL（预写日志）** | 安全保障：每个读数到达时都会立即写入磁盘，因此重启不会丢失近期历史。 |
| **REST API / WebSocket** | 其他软件从 Ironpulse 获取数据的两种方式：REST——“给我当前状态”，WebSocket——“实时通知我”。 |

<a id="how-anomaly-detection-works"></a>
## 异常检测的工作原理
每个读数都会经过两个相互独立的机制。

**硬限值**（`limits`）。数值低于 `low` 或高于 `high` 会产生严重告警。它只在数值离开允许范围时触发一次——而不是在其停留于范围外期间每次轮询都触发——只有在数值回到正常范围后才会再次触发。

**统计方法。** 单一固定阈值要么漏掉缓慢发展的问题（轴承振动在几天内逐渐升高），要么对无害的噪声误报。因此每个传感器可以运行多种独立策略，只有当足够多的策略（`votes_required`）达成一致时才报警：
- **Z-score** —— 标记偏离传感器近期均值的标准差个数异常多的读数。擅长捕捉突发尖峰。
- **EWMA**（指数加权移动平均）—— 跟踪自适应的“正常值”，比固定窗口更快地响应真实的变化。
- **CUSUM**（累积和）—— 累积*持续*漂移的证据，捕捉单点检查在问题变得严重之前会漏掉的缓慢劣化。它需要传感器的正常均值（`mean`）和标准差（`stddev`）。

另外三个细节让告警列表保持有意义而不嘈杂：
- **预热** —— 启动后检测器会保持沉默，直到积累了足够的数据（z-score：半个窗口，EWMA：⌈3/α⌉ 个样本）；否则每次重启都会引起误报；
- **冷却时间**（`cooldown_seconds`）—— 一次持续偏移只产生一条通知，而不是每秒一条；
- **法定票数** —— 噪声较大的传感器上单个检测器的偶发波动不会吵醒值班工程师。

## 配置您的设备
设备及其传感器在配置文件中描述（不使用 Docker 时为 `deploy/config/config.example.json`，使用 Docker 时为 `config.docker.json`）。一台带三个传感器的真实设备：
```json
{
  "devices": [
    {
      "id": "transformer_01",
      "host": "192.168.1.50",
      "port": 502,
      "unit_id": 1,
      "poll_interval_ms": 1000,
      "timeout_ms": 3000,
      "sensors": [
        {
          "id": "winding_temp",
          "name": "绕组温度",
          "unit": "°C",
          "register_type": "holding",
          "address": 0,
          "data_type": "float32",
          "word_order": "big",
          "limits": { "high": 90 }
        },
        {
          "id": "oil_temp",
          "name": "油温",
          "unit": "°C",
          "register_type": "input",
          "address": 10,
          "data_type": "int16",
          "scale": 0.1,
          "limits": { "high": 75 },
          "detection": {
            "detectors": [{ "type": "cusum", "mean": 55, "stddev": 0.5 }],
            "cooldown_seconds": 120
          }
        },
        {
          "id": "load_current",
          "name": "负载电流",
          "unit": "A",
          "address": 20,
          "data_type": "uint32",
          "word_order": "little",
          "scale": 0.01,
          "limits": { "low": 0, "high": 400 }
        }
      ]
    }
  ]
}
```

**设备：**

| 字段 | 默认值 | 含义 |
|---|---|---|
| `id` | — | 唯一的设备名称：拉丁字母、数字、`_`、`-`、`.` |
| `host`、`port` | —、`502` | 设备（或 Modbus TCP 网关）的访问地址 |
| `unit_id` | `1` | Modbus 从站/单元地址（在 RS-485 网关后面时很重要） |
| `poll_interval_ms` | `1000` | 轮询周期。轮询按固定频率进行，即使设备响应缓慢也不会漂移 |
| `timeout_ms` | `3000` | 等待响应的时间。接受连接但保持沉默的设备会被报告为不可达 |
| `sensors` | 寄存器 0 中的一个 `uint16` | 设备的传感器列表 |

**传感器：**

| 字段 | 默认值 | 含义 |
|---|---|---|
| `id` | — | （在所有设备中）唯一的传感器名称；同时也是其历史文件的名称 |
| `name`、`unit` | `id`、空 | 仪表板和通知中显示的名称和单位 |
| `register_type` | `holding` | `holding`（功能码 0x03）或 `input`（0x04） |
| `address` | `0` | 第一个寄存器的地址（从零开始，与协议一致） |
| `data_type` | `uint16` | `uint16`、`int16`、`uint32`、`int32`、`float32`（32 位类型占用两个寄存器） |
| `word_order` | `big` | 对于 32 位值：`big` —— 高位字在前（ABCD），`little` —— 低位字在前（CDAB） |
| `scale`、`offset` | `1`、`0` | 值 = 原始值 × `scale` + `offset` |
| `limits.low`、`limits.high` | 无 | 硬限值；越过即为严重告警 |
| `detection.detectors` | z-score + EWMA | 策略：`{"type": "zscore", "window": 60, "threshold": 3}`、`{"type": "ewma", "alpha": 0.2, "threshold": 3}`、`{"type": "cusum", "mean": …, "stddev": …, "slack": 0.5, "threshold": 5}`。空列表表示关闭统计检测 |
| `detection.votes_required` | `1` | 需要多少个策略达成一致 |
| `detection.cooldown_seconds` | `30` | 同一传感器两次同类告警之间的最短间隔 |

Ironpulse 会把同一设备的相邻寄存器合并为尽可能少的请求（每个请求最多 125 个寄存器）——十个传感器通常只需一到两个请求，而不是十个。

配置会在启动时校验，错误信息会指向具体字段，例如 `config: devices[0].sensors[2].data_type: unknown value 'float64' (expected one of: uint16, int16, uint32, int32, float32)`。旧版本配置中的设备（没有 `sensors` 列表）仍按原方式工作。

任何字符串值都可以引用环境变量：`"host": "${PLC_HOST}"`。主要设置也可以通过 `IRONPULSE_*` 变量覆盖而无需修改文件——参见 [`docs/adr/0003-env-driven-config.md`](docs/adr/0003-env-driven-config.md)。

<a id="notifications"></a>
## 通知
Ironpulse 会把告警和设备失联消息发送到 Telegram、Slack 和任意 HTTP 端点（webhook）。文本使用所选语言，包含传感器名称、数值、限值和单位：
```
🔴 严重
绕组温度：数值 142 °C 高于上限 90 °C
设备: transformer_01
时间: 2026-10-02 06:54:09 UTC
仪表板: https://monitoring.example.com
```

最简单的启用方式是 `.env`（Docker）或环境变量：

| 变量 | 含义 |
|---|---|
| `TELEGRAM_BOT_TOKEN`、`TELEGRAM_CHAT_ID` | 通过 [@BotFather](https://t.me/BotFather) 创建的机器人，并已加入您的聊天或群组 |
| `SLACK_WEBHOOK_URL` | Slack 设置中的 Incoming Webhook |
| `WEBHOOK_URL` | 任何接受 JSON POST 的端点 |
| `NOTIFY_LANGUAGE` | `ru`、`en`、`zh`、`hi`、`es`、`fr`、`de` 或 `it` |
| `IRONPULSE_PUBLIC_URL` | 仪表板地址，会在通知中附带链接 |

（不使用 Docker 时：`IRONPULSE_TELEGRAM_BOT_TOKEN`、`IRONPULSE_TELEGRAM_CHAT_ID`、`IRONPULSE_SLACK_WEBHOOK_URL`、`IRONPULSE_WEBHOOK_URL`、`IRONPULSE_NOTIFY_LANGUAGE`。）

在配置文件中设置效果相同，并提供更多选项：
```json
"notifications": {
  "language": "zh",
  "min_severity": "critical",
  "dashboard_url": "https://monitoring.example.com",
  "channels": [
    { "type": "telegram", "bot_token": "${TG_TOKEN}", "chat_id": "-1001234567890" },
    { "type": "webhook", "url": "https://hooks.example.com/ironpulse",
      "headers": { "Authorization": "Bearer ${HOOK_SECRET}" } }
  ]
}
```
- `min_severity: "critical"` —— 只发送超出限值和失联消息，不发送统计警告。
- Telegram 通道可以设置 `url`：如果工厂网络无法访问 `api.telegram.org`，可指向您自己的 Bot API 服务器或代理。
- Webhook 接收结构化 JSON：`type`（`alert`、`device_offline`、`device_online`）、`severity`、`title`、`text`、`timestamp`、`device_id`、`sensor_id`，以及包含 `kind`、`value`、`limit`、`direction`、`votes` 等字段的 `alert` 对象——便于对接工单和自动化系统。

发送在后台进行，绝不会拖慢轮询；服务器错误最多重试三次，间隔逐渐加长。缺少地址或令牌的通道会被禁用并在日志中给出警告，而不会导致系统停止。

## 安全
默认情况下 API 是开放的——便于初次体验，但不适合联网的服务器。请设置令牌：
```bash
# deploy/docker/.env
IRONPULSE_API_TOKEN=$(openssl rand -hex 32)
```
之后 REST API、实时 WebSocket 数据流和 `/metrics` 都需要 `Authorization: Bearer <令牌>`。仪表板只会询问一次令牌并在浏览器中记住它。`/healthz` 保持开放，供健康检查使用。

此外内置：请求参数校验（返回 `400` 而不是崩溃）、限制 WebSocket 帧和请求头大小、断开停滞的客户端、恒定时间比较令牌、文件系统安全的传感器 ID、容器内的非特权用户，以及沙箱化的 systemd 单元。

## 监控 Ironpulse 本身
`GET /metrics` 提供 Prometheus 指标——可用 Prometheus/Grafana 或 VictoriaMetrics 采集：

| 指标 | 含义 |
|---|---|
| `ironpulse_sensor_value{sensor,unit}` | 每个传感器的最新值 |
| `ironpulse_device_up{device}` | 1 —— 设备有响应，0 —— 无响应 |
| `ironpulse_readings_total{sensor}` | 收到的读数数量 |
| `ironpulse_alerts_total{sensor,kind,severity}` | 产生的告警数量 |
| `ironpulse_poll_errors_total{device}` | 对设备请求失败的次数 |
| `ironpulse_notifications_total{channel,result}` | 通知投递情况 |
| `ironpulse_websocket_clients`、`ironpulse_uptime_seconds`、`ironpulse_build_info{version}` | 服务自身的状态 |

```yaml
# prometheus.yml
scrape_configs:
  - job_name: ironpulse
    authorization: { credentials: "<令牌>" }   # 如果设置了 IRONPULSE_API_TOKEN
    static_configs: [{ targets: ["ironpulse-host:8080"] }]
```

## 导出数据流
除了仪表盘，Ironpulse 还可以把每一条读数写入紧凑的二进制文件，用于数据湖、历史数据库（historian）、离线分析或模型训练。磁盘写入在独立线程中进行：磁盘慢或写满时绝不会拖慢轮询和告警——最坏情况下部分读数未进入导出，并会在指标中体现。

```json
"export": { "enabled": true, "directory": "export", "segment_max_mb": 64 }
```

在 Docker 中，只需在 `.env` 中设置 `EXPORT_ENABLED=true`，文件会出现在数据卷的 `/app/data/export` 中。

文件名为 `telemetry-<时间>-NNNNNN.ipseg`：每条读数 24 字节，每个批次带 CRC-32C 校验，且每个文件内都含有传感器列表，因此即使配置已更改，文件也能独立读取。正在写入的文件以 `.part` 结尾——只取已完成的 `.ipseg` 文件。Ironpulse 不会删除它们，导出数据的保留由您自行负责。

```bash
ironpulse-export verify export/                       # 校验每个批次的 CRC
ironpulse-export dump export/ > readings.csv          # 全部读数导出为 CSV
ironpulse-export dump --sensor winding_temp export/   # 仅一个传感器
```

格式说明：[`docs/export-format.md`](docs/export-format.md)；无依赖的 Python 读取器：[`tools/export_reader/read_segment.py`](tools/export_reader/read_segment.py)；指标：`/metrics` 中的 `ironpulse_export_*`。

## 部署方式

| 场景 | 说明 |
|---|---|
| 只想在本地看看效果 | [快速开始](#quick-start) |
| 自己的服务器/VPS，上面没有其他服务 | [在独立 VPS 上使用 Docker Compose](#dedicated-vps) |
| 服务器上已有其他网站及其 nginx + HTTPS | [在现有 nginx 之后](#behind-nginx) |
| 不能用 Docker，直接构建 C++ | [从源代码构建](#build-from-source) |

<a id="dedicated-vps"></a>
### 在独立 VPS 上使用 Docker Compose
```bash
cd deploy/docker
cp .env.example .env          # 设置 IRONPULSE_API_TOKEN，以及（可选）通知
docker compose up -d --build
```
仪表板位于 `http://<服务器IP>:8080`（也可通过自带的 nginx 在 80 端口访问）。正式公开使用时请配置 TLS 证书——参见下一节。

<a id="behind-nginx"></a>
### 在现有 nginx（和域名）之后
如果您的 VPS 已经托管了其他网站，并有自己的 nginx 和 TLS 证书（例如通过 [certbot](https://certbot.eff.org/)），Ironpulse 可以作为又一个站点加入，而不必争夺 80/443 端口：
```bash
cd deploy/docker
cp .env.example .env
# 在 .env 中设置 IRONPULSE_BIND_HOST=127.0.0.1，使 ironpulse
# 只能从本机访问，而不能直接从互联网访问。
docker compose -f docker-compose.yml -f docker-compose.prod.yml up -d --build
```
这会把 ironpulse 的端口只绑定到 `127.0.0.1` 并跳过自带的 nginx（`docker-compose.prod.yml`），使您现有的 nginx 成为唯一的 TLS 终止点。然后添加虚拟主机——[`deploy/nginx/anton-tests.ru.conf`](deploy/nginx/anton-tests.ru.conf) 是现成的示例；复制它并替换为您的域名和证书路径：
```bash
sudo cp deploy/nginx/anton-tests.ru.conf /etc/nginx/sites-available/your-domain.tld
sudo ln -s /etc/nginx/sites-available/your-domain.tld /etc/nginx/sites-enabled/
sudo nginx -t && sudo systemctl reload nginx
```

### 上线前检查清单
- [ ] 已设置 `IRONPULSE_API_TOKEN`（足够长且随机：`openssl rand -hex 32`）
- [ ] 如果前面有反向代理，已设置 `IRONPULSE_BIND_HOST=127.0.0.1`（切勿直接暴露应用端口）
- [ ] TLS 证书有效并能自动续期（`certbot renew --dry-run`）
- [ ] 如需在重启后保留历史，已设置 `persistence_enabled: true`
- [ ] `retention_hours` 与磁盘容量相匹配——每个读数约 16 字节；旧记录每小时自动删除
- [ ] `devices` 指向您**真实的** Modbus 设备而不是自带的模拟器，且每个传感器都设置了 `limits`
- [ ] 已测试通知：临时把某个限值设得低于当前值，确认消息能收到
- [ ] 磁盘空间足够构建——Docker 临时需要约 3-4 GB，而最终镜像小于 250 MB（之后运行 `docker builder prune`）
- [ ] 所有健康检查均为绿色：`docker compose ps` 显示每个服务都是 `healthy`

<a id="build-from-source"></a>
### 从源代码构建（不使用 Docker）
需要 C++20 编译器（GCC ≥ 12 或 Clang ≥ 15）、CMake ≥ 3.20、Ninja 和 OpenSSL ≥ 3（`libssl-dev`，用于 HTTPS 通知）。其余依赖（Asio、spdlog、nlohmann/json、cpp-httplib、Catch2）会自动下载。
```bash
cmake --preset debug
cmake --build --preset debug -j$(nproc)
ctest --preset debug --output-on-failure
```
其他预设：`release`、`asan`（AddressSanitizer + UBSan）、`tsan`（ThreadSanitizer）。

实际运行需要可供轮询的 Modbus 设备——使用自带的模拟器即可完成独立演示：
```bash
pip install -r tools/modbus_simulator/requirements.txt
python3 tools/modbus_simulator/simulator.py --port 5020 &

./build/debug/ironpulse deploy/config/config.example.json
```
`ironpulse --version` 输出版本号。加固的 systemd 单元参见 [`deploy/systemd/ironpulse.service`](deploy/systemd/ironpulse.service)；它从 `/etc/ironpulse/ironpulse.env` 读取机密信息。

<a id="troubleshooting"></a>
## 故障排除
**启动 Docker 时出现 "Port is already allocated"。** 有其他程序占用了该端口。停止它，或在 `.env` 中更换端口。

**Ironpulse 退出并显示 `Failed to load config ... config: devices[0]...`。** 配置有误；消息中会指出具体字段和问题。

**仪表板要求输入我从未设置过的令牌。** 环境中设置了令牌（`.env` 中的 `IRONPULSE_API_TOKEN` 或配置中的 `api_token`）。输入它，或者如果访问应当开放，就删除它。

**设备显示“离线”，日志中出现 `device unreachable: cannot connect`。** Ironpulse 无法访问 `host`/`port`。使用 Docker Compose 时，`host` 必须是 Docker 服务名称（例如 `simulator`），而不是 `127.0.0.1`。

**日志中出现 `poll failed: device exception code 2`。** 设备有响应，但拒绝了所请求的寄存器：代码 2 表示“非法地址”。请对照设备的寄存器映射检查 `address`、`register_type` 和 `data_type`（常见错误是从 0 还是从 1 开始编号，以及混淆保持寄存器和输入寄存器）。

**数值看起来像乱码（巨大的数字，或正数变成负数）。** 几乎总是 `data_type` 或 `word_order` 的问题：32 位值试试 `word_order: "little"`，可能为负的量用 `int16` 代替 `uint16`，并检查 `scale`。

**收不到通知。** 查看日志：`Notification via telegram failed (HTTP 401)` —— 机器人令牌错误；`HTTP 400` —— `chat_id` 错误或机器人不在该聊天中；`network error` —— 服务器无法访问互联网（Telegram 通道可在 `url` 中设置代理或自建的 Bot API 服务器）。指标 `ironpulse_notifications_total{result="error"}` 统计失败的投递。

**修改了文件，但 Docker 仍提供旧版本。** 运行 `docker compose build --no-cache`；如果还不够，先运行 `docker builder prune -af`。

## 架构（面向开发者）
```
Modbus TCP 设备 → protocol（轮询、寄存器解码） → EventBus
                                                    │
           ┌──────────────────────┬─────────────────┴────┬───────────────────┐
           ▼                      ▼                      ▼                   ▼
storage（环形缓冲 + WAL）   analytics（限值、       api（REST、WebSocket、   metrics
                            检测器、法定票数）      /metrics）→ 仪表板
                                     │
                                     ▼
                             notify（Telegram、Slack、webhook）
```
每一层（`core`、`protocol`、`storage`、`ingest`、`analytics`、`api`、`notify`）都是独立的 CMake 目标，带有各自的测试。各层通过 `EventBus` 通信，而不是直接调用。并发模型参见 [`docs/architecture.md`](docs/architecture.md)，具体决策参见 [`docs/adr/`](docs/adr/)。

### API 参考
完整规范：[`docs/openapi.yaml`](docs/openapi.yaml)（OpenAPI 3——可在 Swagger UI 中打开或导入 Postman）。

| 端点 | 说明 |
|---|---|
| `GET /api/v1/devices` | 设备：`id`、`online`、`poll_interval_ms`、传感器列表 |
| `GET /api/v1/sensors` | 传感器：`id`、`name`、`unit`、`device_id`、`limits`、最新值 |
| `GET /api/v1/series/{sensor_id}?since=<秒>` | 指定时段的历史（默认 300 秒；启用 WAL 时可覆盖整个保留期）。较长时段会被抽稀到 10,000 个点 |
| `GET /api/v1/series/{sensor_id}?since=<秒>&format=csv` | 同上，CSV 格式，从不抽稀 |
| `GET /api/v1/alerts?limit=<n>` | 最近的告警，最新的在前（1–1000，默认 50）：`kind`、`severity`、`value`、`limit`、`direction`、`votes`、`detectors_total`、`confidence` |
| `GET /api/v1/config` | `ws_port`、`ws_host`、`auth_required`、`version`——仪表板启动所需的信息（无需令牌） |
| `GET /metrics` | Prometheus 指标 |
| `GET /healthz` | 存活检查和版本（无需令牌） |
| `WS /live`（反向代理后为 `/ws/live`） | 实时推送：`reading`、`anomaly`、`device_status` 事件。设置令牌时：`?token=<令牌>` |

### 工程实践
- 严格的编译器警告（`-Wall -Wextra -Wpedantic -Wconversion ...`），零警告构建；可选择将警告视为错误（`IRONPULSE_WARNINGS_AS_ERRORS`）
- 131 个测试：每个组件的单元测试、基于真实套接字的 HTTP API 和 WebSocket 握手测试、通过 TCP 连接模拟 Modbus 设备的端到端测试（解码、限值、超时、失联与恢复）。在 AddressSanitizer/UBSan 和 ThreadSanitizer 下全部通过
- CI：GCC 和 Clang、Sanitizer、`clang-format`、模拟器的 `ruff`、翻译完整性检查、Docker 构建，以及检查实时数据的 Compose 冒烟测试——[`.github/workflows/ci.yml`](.github/workflows/ci.yml)
- 基于标签的发布：在 amd64 和 arm64 上原生构建镜像并发布到 GitHub Container Registry——[`.github/workflows/release.yml`](.github/workflows/release.yml)
- 多阶段 Docker 构建、容器内非特权用户、每个服务都有健康检查
- 仪表板没有任何外部运行时依赖（Chart.js 已内置），因此可在受限或离线网络中使用

### 路线图
- [x] 异步 Modbus TCP 客户端 + 帧编解码器
- [x] 带 WAL 持久化和保留期的时序存储
- [x] Z-score、EWMA 和 CUSUM 检测，通过法定票数投票的 `RuleEngine` 组合
- [x] REST API + 手写的 WebSocket（RFC 6455）实时推送
- [x] 面向容器的环境变量驱动配置
- [x] 针对存储/协议热点路径的 Google Benchmark 套件
- [x] 集成测试：从模拟设备到发布告警的完整流水线
- [x] 定时自动清理 WAL
- [x] 多架构 Docker 镜像（linux/amd64 + linux/arm64）
- [x] 每台设备多个传感器、数据类型、缩放、输入寄存器
- [x] 硬限值、告警严重级别、冷却时间
- [x] 通知：Telegram、Slack、webhook——8 种语言
- [x] 令牌访问、Prometheus 指标、CSV 导出
- [x] 将全部读数导出为带 CRC 校验的文件（`ingest` 模块，原 apollonian_core_ingestor）
- [ ] 直接支持 Modbus RTU（RS-485），无需网关
- [ ] 以 OPC UA 和 MQTT 作为数据源
- [ ] 操作员确认告警及审计日志
- [ ] 以用户和角色替代单一令牌

## 参与贡献
欢迎提交 issue 和 pull request。提交 PR 之前：
```bash
cmake --preset debug && cmake --build --preset debug -j$(nproc)
ctest --preset debug --output-on-failure
find include src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format --dry-run --Werror
ruff check tools/ && ruff format --check tools/
```
CI 会在每个 pull request 上运行相同的检查（以及 Sanitizer 和 Docker 构建）。各版本的变更记录在 [CHANGELOG.md](CHANGELOG.md) 中。

## 许可证
MIT——参见 [LICENSE](LICENSE)。可自由使用、修改和部署，包括商业用途。
