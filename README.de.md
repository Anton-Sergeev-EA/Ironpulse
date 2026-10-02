# Ironpulse

**Industrielle Echtzeitüberwachung: beobachtet die Sensoren Ihrer Anlagen und meldet sich, wenn etwas schiefläuft – bevor etwas kaputtgeht.**

[![CI](https://img.shields.io/badge/CI-GitHub_Actions-2088FF?logo=githubactions&logoColor=white)](.github/workflows/ci.yml)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](CMakeLists.txt)
[![Docker](https://img.shields.io/badge/Docker-amd64%20%7C%20arm64-2496ED?logo=docker&logoColor=white)](deploy/docker)
[![Languages](https://img.shields.io/badge/UI-8%20languages-8A2BE2)](#interface-languages)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-92%20passing-brightgreen)](tests)

[Русский](README.md) · [English](README.en.md) · [中文](README.zh.md) · [हिन्दी](README.hi.md) · [Español](README.es.md) · [Français](README.fr.md) · **Deutsch** · [Italiano](README.it.md)

## Was ist das?
Stellen Sie sich ein Werk mit Dutzenden Sensoren vor: Wicklungstemperatur eines Transformators, Lagerschwingung eines Motors, Druck einer Pumpe. Heute muss jemand es auf einem Bildschirm bemerken – oder schlimmer, ein lautes Geräusch hören –, bevor klar wird, dass ein Teil überhitzt oder kurz vor dem Ausfall steht.

**Ironpulse überwacht diese Sensoren kontinuierlich und meldet sofort, wenn Messwerte ihren zulässigen Bereich verlassen oder sich ungewöhnlich verhalten** – mit denselben statistischen Verfahren, die Ingenieure in der vorausschauenden Instandhaltung verwenden. Feste Grenzwerte erkennen, was bereits gefährlich ist; die Statistik erkennt sowohl plötzliche Spitzen als auch langsame, schleichende Verschlechterung – lange bevor der Wert seinen Grenzwert erreicht.

Sie erhalten:
- **Ein Live-Dashboard** in 8 Sprachen – der aktuelle Wert jedes Sensors in seiner Einheit, ein Diagramm mit Grenzwertlinien und eine Alarmliste mit Schweregraden.
- **Benachrichtigungen** an Telegram, Slack oder beliebige Webhooks – in der Sprache Ihres Teams, einschließlich Verbindungsausfällen und deren Wiederherstellung.
- **Verlauf auf der Festplatte** – ein Neustart verliert nichts, und jeder Sensor lässt sich für die gesamte Aufbewahrungsdauer als CSV exportieren.
- **APIs und Metriken** für andere Systeme: REST, WebSocket und `/metrics` für Prometheus/Grafana.
- **Tokengeschützten Zugriff** auf API, Live-Stream und Metriken.

Ironpulse spricht mit echten Industrieanlagen über **Modbus TCP**, das Protokoll, das ein großer Teil der Sensoren, SPS und Zähler weltweit verwendet. Es liest Holding- und Input-Register, versteht 16- und 32-Bit-Ganzzahlen sowie Gleitkommazahlen in beliebiger Wortreihenfolge und wendet Skalierung und Offset an – es arbeitet also mit den Registertabellen echter Geräte, nicht nur mit der Demo.

### Für wen ist es gedacht?
- **Ingenieure und Techniker**, die ein schlankes, selbst gehostetes Überwachungswerkzeug brauchen – ohne für eine kommerzielle SCADA-/IIoT-Plattform zu bezahlen (oder auf die Freigabe der IT zu warten).
- **Entwickler**, die eine C++20-Codebasis in Produktionsqualität suchen: asynchrones Networking, ein selbst geschriebener WebSocket-Server, eine REST-API, Container-Deployment und eine echte Testsuite – ohne schwere Frameworks.
- **Studierende und Interessierte**, die sehen möchten, wie eine industrielle Überwachungskette von Anfang bis Ende funktioniert.

Sie müssen **kein** C++ können, um es zu starten – siehe [Schnellstart](#quick-start). Docker baut alles.

## So sieht es aus
Links die Geräte und ihr Verbindungsstatus. In der Mitte eine Karte pro Sensor: Name, großer aktueller Wert in der Einheit des Sensors (rot, wenn außerhalb der Grenzwerte), ein Live-Diagramm mit gestrichelten Grenzwertlinien und eine CSV-Export-Schaltfläche. Rechts die Alarmliste: rot „kritisch“ für Grenzwertverletzungen, gelb „Warnung“ für statistische Anomalien. Alles aktualisiert sich in Echtzeit ohne Neuladen und funktioniert auf dem großen Bildschirm einer Leitwarte ebenso gut wie auf dem Smartphone.

<a id="interface-languages"></a>
## Sprachen der Oberfläche
Dashboard und Benachrichtigungen gibt es in 8 Sprachen: **Russisch** (Hauptsprache), Englisch, Chinesisch, Hindi, Spanisch, Französisch, Deutsch und Italienisch – die Auswahl befindet sich oben rechts. Auch diese Dokumentation liegt in diesen 8 Sprachen vor. Die Sprache des Dashboards wird in dieser Reihenfolge bestimmt:
1. der Parameter `?lang=` in der Adresse, z. B. `http://localhost:8080/?lang=de` (praktisch, um einen Link zu teilen);
2. die zuvor in diesem Browser gewählte Sprache;
3. die Sprache des Browsers, falls unterstützt;
4. andernfalls Russisch.

Die Sprache der Benachrichtigungen wird in der Konfiguration festgelegt (`notifications.language`) – siehe [Benachrichtigungen](#notifications).

Die Übersetzungen des Dashboards liegen in [`web/js/i18n/locales/`](web/js/i18n/locales): Um eine Sprache hinzuzufügen, kopieren Sie `ru.js`, übersetzen die Werte und tragen die Datei in `web/index.html` sowie in die Liste `supported` in [`web/js/i18n/i18n.js`](web/js/i18n/i18n.js) ein. Die CI prüft, dass alle Sprachen genau dieselben Texte definieren.

<a id="quick-start"></a>
## Schnellstart
Sie brauchen nur [Docker](https://docs.docker.com/get-docker/). Es baut alles Weitere (den C++-Compiler, alle Bibliotheken) in einem Container, Ihr Rechner bleibt sauber.
```bash
git clone https://github.com/Anton-Sergeev-EA/Ironpulse.git
cd Ironpulse/deploy/docker
docker compose up --build
```
Warten Sie, bis es fertig ist (beim ersten Mal ein paar Minuten – C++ wird aus dem Quellcode kompiliert), und öffnen Sie dann:
- **http://localhost:8080** – das Live-Dashboard
- **http://localhost:8080/api/v1/sensors** – die rohe API, falls Sie neugierig sind

Das war’s. Sie beobachten jetzt zwei simulierte Geräte – einen Transformator (Wicklungs- und Öltemperatur) und eine Pumpe (Lagerschwingung und Druck). Der Simulator erzeugt ab und zu Spitzen und anhaltende Abweichungen – sehen Sie zu, wie Warnungen und kritische Ereignisse im Bereich „Aktive Alarme“ erscheinen.

Mit jedem Release werden fertige Images für amd64 und arm64 (Raspberry Pi, ARM-Gateways) veröffentlicht:
```bash
docker pull ghcr.io/anton-sergeev-ea/ironpulse:latest
```

### Etwas funktioniert nicht?
- **Port bereits belegt?** Sehr häufig, wenn auf 8080 schon etwas läuft (Jenkins, ein anderes Dashboard …). Kopieren Sie `.env.example` nach `.env` in `deploy/docker/`, ändern Sie `HTTP_PORT`/`WS_PORT`/`NGINX_PORT` auf freie Ports und starten Sie `docker compose up --build` erneut. Eine Datei, eine Änderung.
- **Klappt es immer noch nicht?** Siehe [Fehlerbehebung](#troubleshooting).

## Was auf dem Bildschirm zu sehen ist und was die Begriffe bedeuten

| Begriff | Einfache Erklärung |
|---|---|
| **Modbus** | Eine jahrzehntealte, aber immer noch allgegenwärtige „Sprache“, mit der industrielle Sensoren und Steuerungen mit Software kommunizieren. Ironpulse spricht sie direkt. |
| **Register** | Eine Speicherzelle im Gerät, die einen Messwert enthält. Die Dokumentation jedes Modbus-Geräts enthält eine Registertabelle, die angibt, was welche Adresse bedeutet. |
| **Grenzwert** | Eine Grenze, die der Wert nicht überschreiten darf, z. B. eine Wicklungstemperatur über 90 °C. Eine Überschreitung löst einen **kritischen** Alarm aus. |
| **Anomalie** | Ein Messwert, der nicht zum jüngsten normalen Verlauf des Sensors passt – eine plötzliche Spitze oder eine langsame Drift, die zu lange anhält. Das ist eine **Warnung**: Der Wert liegt noch innerhalb der Grenzwerte, verhält sich aber ungewöhnlich. |
| **Konfidenz** | Wie sicher sich das System ist, dass ein markierter Messwert eine echte Anomalie und kein Rauschen ist. Je höher, desto sicherer. |
| **Z-Score, EWMA, CUSUM** | Drei statistische Erkennungsverfahren, jedes gut für eine andere *Art* von Problem (siehe [Wie die Anomalieerkennung funktioniert](#how-anomaly-detection-works)). Ironpulse betreibt mehrere gleichzeitig und schlägt nur Alarm, wenn genügend von ihnen übereinstimmen – weniger Fehlalarme. |
| **WAL (Write-Ahead-Log)** | Ein Sicherheitsnetz: Jeder Messwert wird bei Ankunft auf die Festplatte geschrieben, sodass ein Neustart den jüngsten Verlauf nicht verliert. |
| **REST-API / WebSocket** | Zwei Wege, wie andere Software Daten aus Ironpulse bekommt: REST – „gib mir den aktuellen Zustand“, WebSocket – „halte mich in Echtzeit auf dem Laufenden“. |

<a id="how-anomaly-detection-works"></a>
## Wie die Anomalieerkennung funktioniert
Für jeden Messwert laufen zwei unabhängige Mechanismen.

**Feste Grenzwerte** (`limits`). Ein Wert unter `low` oder über `high` löst einen kritischen Alarm aus. Er wird einmal ausgelöst, wenn der Wert den zulässigen Bereich verlässt – nicht bei jeder Abfrage, solange er draußen bleibt – und erneut erst, nachdem er zurückgekehrt ist.

**Statistik.** Ein einzelner fester Schwellenwert übersieht entweder langsame Probleme (eine Lagerschwingung, die über Tage ansteigt) oder schlägt bei harmlosem Rauschen falschen Alarm. Deshalb kann jeder Sensor mehrere unabhängige Strategien ausführen, und Alarm wird nur ausgelöst, wenn genügend von ihnen (`votes_required`) übereinstimmen:
- **Z-Score** – markiert einen Messwert, der um ungewöhnlich viele Standardabweichungen vom jüngsten Mittelwert des Sensors abweicht. Gut für plötzliche Spitzen.
- **EWMA** (exponentiell gewichteter gleitender Mittelwert) – verfolgt einen sich selbst anpassenden „Normalwert“, der schneller als ein festes Fenster auf echte Verschiebungen reagiert.
- **CUSUM** (kumulierte Summe) – sammelt Hinweise auf eine *anhaltende* Drift und erkennt langsame Verschlechterung, die eine Einzelpunktprüfung übersehen würde, bis sie ernst wird. Es braucht den normalen Mittelwert (`mean`) und die Streuung (`stddev`) des Sensors.

Drei weitere Details halten die Alarmliste aussagekräftig statt laut:
- **Aufwärmphase** – direkt nach dem Start bleiben die Detektoren still, bis genügend Daten vorliegen (Z-Score: ein halbes Fenster, EWMA: ⌈3/α⌉ Werte); sonst würde jeder Neustart Fehlalarme auslösen;
- **Ruhezeit** (`cooldown_seconds`) – eine anhaltende Abweichung erzeugt eine Benachrichtigung, nicht eine pro Sekunde;
- **Quorum** – ein einzelner Ausreißer eines Detektors an einem verrauschten Sensor weckt nicht den Bereitschaftsingenieur.

## Eigene Geräte konfigurieren
Geräte und ihre Sensoren werden in der Konfigurationsdatei beschrieben (`deploy/config/config.example.json` ohne Docker, `config.docker.json` mit Docker). Ein echtes Gerät mit drei Sensoren:
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
          "name": "Wicklungstemperatur",
          "unit": "°C",
          "register_type": "holding",
          "address": 0,
          "data_type": "float32",
          "word_order": "big",
          "limits": { "high": 90 }
        },
        {
          "id": "oil_temp",
          "name": "Öltemperatur",
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
          "name": "Laststrom",
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

**Gerät:**

| Feld | Standard | Bedeutung |
|---|---|---|
| `id` | — | Eindeutiger Gerätename: lateinische Buchstaben, Ziffern, `_`, `-`, `.` |
| `host`, `port` | —, `502` | Wo das Gerät (oder das Modbus-TCP-Gateway) erreichbar ist |
| `unit_id` | `1` | Modbus-Slave-/Unit-Adresse (wichtig hinter RS-485-Gateways) |
| `poll_interval_ms` | `1000` | Abfrageintervall. Die Abfrage läuft mit fester Rate und driftet nicht, auch wenn das Gerät langsam antwortet |
| `timeout_ms` | `3000` | Wie lange auf eine Antwort gewartet wird. Ein Gerät, das die Verbindung annimmt, aber schweigt, wird als nicht erreichbar gemeldet |
| `sensors` | ein `uint16` in Register 0 | Die Sensoren des Geräts |

**Sensor:**

| Feld | Standard | Bedeutung |
|---|---|---|
| `id` | — | Eindeutiger (über alle Geräte) Sensorname; zugleich Name seiner Verlaufsdatei |
| `name`, `unit` | `id`, leer | Bezeichnung und Einheit für Dashboard und Benachrichtigungen |
| `register_type` | `holding` | `holding` (Funktion 0x03) oder `input` (0x04) |
| `address` | `0` | Adresse des ersten Registers (ab null, wie im Protokoll) |
| `data_type` | `uint16` | `uint16`, `int16`, `uint32`, `int32`, `float32` (32-Bit-Typen belegen zwei Register) |
| `word_order` | `big` | Für 32-Bit-Werte: `big` – höherwertiges Wort zuerst (ABCD), `little` – niederwertiges Wort zuerst (CDAB) |
| `scale`, `offset` | `1`, `0` | Wert = Rohwert × `scale` + `offset` |
| `limits.low`, `limits.high` | keine | Feste Grenzwerte; eine Überschreitung ist ein kritischer Alarm |
| `detection.detectors` | Z-Score + EWMA | Strategien: `{"type": "zscore", "window": 60, "threshold": 3}`, `{"type": "ewma", "alpha": 0.2, "threshold": 3}`, `{"type": "cusum", "mean": …, "stddev": …, "slack": 0.5, "threshold": 5}`. Eine leere Liste schaltet die Statistik ab |
| `detection.votes_required` | `1` | Wie viele Strategien übereinstimmen müssen |
| `detection.cooldown_seconds` | `30` | Mindestabstand zwischen zwei Alarmen derselben Art für diesen Sensor |

Ironpulse fasst benachbarte Register eines Geräts zu möglichst wenigen Anfragen zusammen (bis zu 125 Register pro Anfrage) – zehn Sensoren werden meist mit ein bis zwei Anfragen gelesen, nicht mit zehn.

Die Konfiguration wird beim Start geprüft, und Fehler zeigen auf das genaue Feld, z. B. `config: devices[0].sensors[2].data_type: unknown value 'float64' (expected one of: uint16, int16, uint32, int32, float32)`. Geräte aus älteren Konfigurationen (ohne `sensors`-Liste) funktionieren weiter wie bisher.

Jeder Textwert kann auf eine Umgebungsvariable verweisen: `"host": "${PLC_HOST}"`. Die wichtigsten Einstellungen lassen sich auch über `IRONPULSE_*`-Variablen überschreiben, ohne die Datei zu ändern – siehe [`docs/adr/0003-env-driven-config.md`](docs/adr/0003-env-driven-config.md).

<a id="notifications"></a>
## Benachrichtigungen
Ironpulse sendet Alarme und Verbindungsausfälle an Telegram, Slack und beliebige HTTP-Adressen (Webhooks). Die Texte sind in der gewählten Sprache und nennen Sensor, Wert, Grenzwert und Einheit:
```
🔴 KRITISCH
Wicklungstemperatur: Wert 142 °C liegt über dem Grenzwert 90 °C
Gerät: transformer_01
Zeit: 2026-10-02 06:54:09 UTC
Dashboard: https://monitoring.example.com
```

Am einfachsten schalten Sie sie über `.env` (Docker) oder Umgebungsvariablen ein:

| Variable | Bedeutung |
|---|---|
| `TELEGRAM_BOT_TOKEN`, `TELEGRAM_CHAT_ID` | Ein mit [@BotFather](https://t.me/BotFather) erstellter Bot, der zu Ihrem Chat oder Ihrer Gruppe hinzugefügt wurde |
| `SLACK_WEBHOOK_URL` | Ein Incoming Webhook aus den Slack-Einstellungen |
| `WEBHOOK_URL` | Jede Adresse, die einen JSON-POST annimmt |
| `NOTIFY_LANGUAGE` | `ru`, `en`, `zh`, `hi`, `es`, `fr`, `de` oder `it` |
| `IRONPULSE_PUBLIC_URL` | Die Adresse des Dashboards, auf die Benachrichtigungen verlinken |

(Ohne Docker: `IRONPULSE_TELEGRAM_BOT_TOKEN`, `IRONPULSE_TELEGRAM_CHAT_ID`, `IRONPULSE_SLACK_WEBHOOK_URL`, `IRONPULSE_WEBHOOK_URL`, `IRONPULSE_NOTIFY_LANGUAGE`.)

Dasselbe in der Konfigurationsdatei, mit zusätzlichen Optionen:
```json
"notifications": {
  "language": "de",
  "min_severity": "critical",
  "dashboard_url": "https://monitoring.example.com",
  "channels": [
    { "type": "telegram", "bot_token": "${TG_TOKEN}", "chat_id": "-1001234567890" },
    { "type": "webhook", "url": "https://hooks.example.com/ironpulse",
      "headers": { "Authorization": "Bearer ${HOOK_SECRET}" } }
  ]
}
```
- `min_severity: "critical"` – nur Grenzwertverletzungen und Verbindungsausfälle, keine statistischen Warnungen.
- Ein Telegram-Kanal akzeptiert eine `url`: Ihren eigenen Bot-API-Server oder einen Proxy, falls `api.telegram.org` aus dem Werksnetz nicht erreichbar ist.
- Webhooks erhalten strukturiertes JSON: `type` (`alert`, `device_offline`, `device_online`), `severity`, `title`, `text`, `timestamp`, `device_id`, `sensor_id` und ein `alert`-Objekt mit `kind`, `value`, `limit`, `direction`, `votes` usw. – praktisch für Ticketsysteme und Automatisierung.

Der Versand läuft im Hintergrund und bremst die Abfrage nie; Serverfehler werden bis zu dreimal mit wachsenden Pausen wiederholt. Ein Kanal ohne Adresse oder Token wird mit einer Warnung im Log deaktiviert, statt das System anzuhalten.

## Sicherheit
Standardmäßig ist die API offen – bequem für einen ersten Blick, nicht für einen Server im Netz. Setzen Sie ein Token:
```bash
# deploy/docker/.env
IRONPULSE_API_TOKEN=$(openssl rand -hex 32)
```
Danach verlangen REST-API, Live-WebSocket-Stream und `/metrics` den Header `Authorization: Bearer <Token>`. Das Dashboard fragt einmal nach dem Token und merkt es sich im Browser. `/healthz` bleibt für Health-Checks offen.

Außerdem eingebaut: Prüfung der Anfrageparameter (`400` statt Absturz), begrenzte Größe von WebSocket-Frames und -Headern, Trennen hängender Clients, Token-Vergleich in konstanter Zeit, dateisystemsichere Sensor-IDs, ein unprivilegierter Benutzer im Container und eine abgeschottete systemd-Unit.

## Ironpulse selbst überwachen
`GET /metrics` liefert Prometheus-Metriken – erfassen Sie sie mit Prometheus/Grafana oder VictoriaMetrics:

| Metrik | Zeigt |
|---|---|
| `ironpulse_sensor_value{sensor,unit}` | Letzter Wert jedes Sensors |
| `ironpulse_device_up{device}` | 1 – Gerät antwortet, 0 – nicht |
| `ironpulse_readings_total{sensor}` | Empfangene Messwerte |
| `ironpulse_alerts_total{sensor,kind,severity}` | Ausgelöste Alarme |
| `ironpulse_poll_errors_total{device}` | Fehlgeschlagene Anfragen an ein Gerät |
| `ironpulse_notifications_total{channel,result}` | Zustellung von Benachrichtigungen |
| `ironpulse_websocket_clients`, `ironpulse_uptime_seconds`, `ironpulse_build_info{version}` | Zustand des Dienstes selbst |

```yaml
# prometheus.yml
scrape_configs:
  - job_name: ironpulse
    authorization: { credentials: "<Token>" }   # falls IRONPULSE_API_TOKEN gesetzt ist
    static_configs: [{ targets: ["ironpulse-host:8080"] }]
```

## Bereitstellungsoptionen

| Szenario | Anleitung |
|---|---|
| Ich will es nur lokal laufen sehen | [Schnellstart](#quick-start) |
| Eigener Server/VPS, auf dem sonst nichts läuft | [Docker Compose auf einem eigenen VPS](#dedicated-vps) |
| Auf dem Server laufen bereits andere Sites mit eigenem nginx + HTTPS | [Hinter einem vorhandenen nginx](#behind-nginx) |
| Kein Docker, C++ direkt bauen | [Aus dem Quellcode bauen](#build-from-source) |

<a id="dedicated-vps"></a>
### Docker Compose auf einem eigenen VPS
```bash
cd deploy/docker
cp .env.example .env          # IRONPULSE_API_TOKEN und bei Bedarf Benachrichtigungen setzen
docker compose up -d --build
```
Das Dashboard ist unter `http://<Server-IP>:8080` erreichbar (und über den mitgelieferten nginx auf Port 80). Für echten öffentlichen Betrieb schalten Sie ein TLS-Zertifikat davor – siehe nächster Abschnitt.

<a id="behind-nginx"></a>
### Hinter einem vorhandenen nginx (und einer Domain)
Wenn Ihr VPS bereits andere Sites mit eigenem nginx und TLS-Zertifikaten hostet (z. B. über [certbot](https://certbot.eff.org/)), kann Ironpulse als weitere Site hinzukommen, statt um die Ports 80/443 zu konkurrieren:
```bash
cd deploy/docker
cp .env.example .env
# In .env IRONPULSE_BIND_HOST=127.0.0.1 setzen, damit ironpulse nur von
# diesem Rechner aus erreichbar ist, nicht direkt aus dem Internet.
docker compose -f docker-compose.yml -f docker-compose.prod.yml up -d --build
```
Damit werden die Ports von ironpulse nur an `127.0.0.1` gebunden und der mitgelieferte nginx übersprungen (`docker-compose.prod.yml`), sodass Ihr vorhandener nginx der einzige TLS-Endpunkt ist. Fügen Sie dann einen vhost hinzu – [`deploy/nginx/anton-tests.ru.conf`](deploy/nginx/anton-tests.ru.conf) ist ein fertiges Beispiel; kopieren Sie es und tragen Sie Ihre Domain und die Zertifikatspfade ein:
```bash
sudo cp deploy/nginx/anton-tests.ru.conf /etc/nginx/sites-available/your-domain.tld
sudo ln -s /etc/nginx/sites-available/your-domain.tld /etc/nginx/sites-enabled/
sudo nginx -t && sudo systemctl reload nginx
```

### Checkliste vor dem Produktivbetrieb
- [ ] `IRONPULSE_API_TOKEN` ist gesetzt (lang und zufällig: `openssl rand -hex 32`)
- [ ] `IRONPULSE_BIND_HOST=127.0.0.1`, wenn ein Reverse Proxy davorsteht (den rohen App-Port nie offenlegen)
- [ ] Das TLS-Zertifikat ist gültig und erneuert sich automatisch (`certbot renew --dry-run`)
- [ ] `persistence_enabled: true`, wenn der Verlauf Neustarts überleben soll
- [ ] `retention_hours` passt zu Ihrer Festplatte – etwa 16 Byte pro Messwert; alte Einträge werden stündlich automatisch entfernt
- [ ] `devices` zeigt auf Ihre **echten** Modbus-Geräte, nicht auf den mitgelieferten Simulator, und jeder Sensor hat `limits`
- [ ] Benachrichtigungen getestet: einen Grenzwert vorübergehend unter den aktuellen Wert setzen und prüfen, dass die Nachricht ankommt
- [ ] Genug Speicherplatz für den Build – Docker braucht vorübergehend ~3-4 GB, das fertige Image ist kleiner als 250 MB (danach `docker builder prune`)
- [ ] Alle Health-Checks sind grün: `docker compose ps` zeigt jeden Dienst als `healthy`

<a id="build-from-source"></a>
### Aus dem Quellcode bauen (ohne Docker)
Sie brauchen einen C++20-Compiler (GCC ≥ 12 oder Clang ≥ 15), CMake ≥ 3.20, Ninja und OpenSSL ≥ 3 (`libssl-dev`, für HTTPS-Benachrichtigungen). Alles andere (Asio, spdlog, nlohmann/json, cpp-httplib, Catch2) wird automatisch geladen.
```bash
cmake --preset debug
cmake --build --preset debug -j$(nproc)
ctest --preset debug --output-on-failure
```
Weitere Presets: `release`, `asan` (AddressSanitizer + UBSan), `tsan` (ThreadSanitizer).

Für den echten Betrieb brauchen Sie abfragbare Modbus-Geräte – nutzen Sie den mitgelieferten Simulator für eine eigenständige Demo:
```bash
pip install -r tools/modbus_simulator/requirements.txt
python3 tools/modbus_simulator/simulator.py --port 5020 &

./build/debug/ironpulse deploy/config/config.example.json
```
`ironpulse --version` gibt die Version aus. Eine gehärtete systemd-Unit finden Sie in [`deploy/systemd/ironpulse.service`](deploy/systemd/ironpulse.service); sie liest Geheimnisse aus `/etc/ironpulse/ironpulse.env`.

<a id="troubleshooting"></a>
## Fehlerbehebung
**„Port is already allocated“ beim Start von Docker.** Ein anderes Programm belegt den Port. Beenden Sie es oder ändern Sie den Port in `.env`.

**Ironpulse beendet sich mit `Failed to load config ... config: devices[0]...`.** Die Konfiguration enthält einen Fehler; die Meldung nennt das genaue Feld und das Problem.

**Das Dashboard fragt nach einem Token, das ich nie gesetzt habe.** In der Umgebung ist ein Token gesetzt (`IRONPULSE_API_TOKEN` in `.env` oder `api_token` in der Konfiguration). Geben Sie es ein oder entfernen Sie es, wenn der Zugriff offen sein soll.

**Das Gerät wird als „offline“ angezeigt und das Log meldet `device unreachable: cannot connect`.** Ironpulse erreicht `host`/`port` nicht. Mit Docker Compose muss `host` der Name des Docker-Dienstes sein (z. B. `simulator`), nicht `127.0.0.1`.

**Das Log meldet `poll failed: device exception code 2`.** Das Gerät antwortet, verweigert aber die angefragten Register: Code 2 bedeutet „ungültige Adresse“. Gleichen Sie `address`, `register_type` und `data_type` mit der Registertabelle des Geräts ab (typische Verwechslungen: Zählung ab null oder ab eins sowie Holding- und Input-Register).

**Die Werte sehen wie Unsinn aus (riesige Zahlen, negativ statt positiv).** Fast immer liegt es an `data_type` oder `word_order`: Versuchen Sie `word_order: "little"` bei 32-Bit-Werten, `int16` statt `uint16` für Größen, die negativ sein können, und prüfen Sie `scale`.

**Benachrichtigungen kommen nicht an.** Prüfen Sie das Log: `Notification via telegram failed (HTTP 401)` – falsches Bot-Token; `HTTP 400` – falsche `chat_id` oder der Bot ist nicht im Chat; `network error` – der Server hat keinen Internetzugang (Telegram-Kanäle akzeptieren in `url` einen Proxy oder einen eigenen Bot-API-Server). Die Metrik `ironpulse_notifications_total{result="error"}` zählt fehlgeschlagene Zustellungen.

**Ich habe eine Datei geändert, aber Docker liefert noch die alte Version.** `docker compose build --no-cache` und, falls das nicht reicht, vorher `docker builder prune -af`.

## Architektur (für Entwickler)
```
Modbus-TCP-Geräte → protocol (Abfrage, Registerdekodierung) → EventBus
                                                                 │
           ┌──────────────────────┬──────────────────────┬───────┴───────────┐
           ▼                      ▼                      ▼                   ▼
storage (Ringpuffer + WAL)   analytics (Grenzwerte, api (REST, WebSocket,  metrics
                             Detektoren, Quorum)    /metrics) → Dashboard
                                     │
                                     ▼
                             notify (Telegram, Slack, Webhook)
```
Jede Schicht (`core`, `protocol`, `storage`, `analytics`, `api`, `notify`) ist ein eigenständiges CMake-Target mit eigenen Tests. Die Schichten kommunizieren über den `EventBus`, nicht direkt. Siehe [`docs/architecture.md`](docs/architecture.md) für das Nebenläufigkeitsmodell und [`docs/adr/`](docs/adr/) für einzelne Entscheidungen.

### API-Referenz
Vollständiger Vertrag: [`docs/openapi.yaml`](docs/openapi.yaml) (OpenAPI 3 – in Swagger UI öffnen oder in Postman importieren).

| Endpunkt | Beschreibung |
|---|---|
| `GET /api/v1/devices` | Geräte: `id`, `online`, `poll_interval_ms`, Sensorliste |
| `GET /api/v1/sensors` | Sensoren: `id`, `name`, `unit`, `device_id`, `limits`, letzter Wert |
| `GET /api/v1/series/{sensor_id}?since=<Sekunden>` | Verlauf des Zeitraums (Standard 300 s; mit WAL die gesamte Aufbewahrungsdauer). Lange Zeiträume werden auf 10.000 Punkte ausgedünnt |
| `GET /api/v1/series/{sensor_id}?since=<Sekunden>&format=csv` | Dasselbe als CSV, nie ausgedünnt |
| `GET /api/v1/alerts?limit=<n>` | Aktuelle Alarme, neueste zuerst (1–1000, Standard 50): `kind`, `severity`, `value`, `limit`, `direction`, `votes`, `detectors_total`, `confidence` |
| `GET /api/v1/config` | `ws_port`, `ws_host`, `auth_required`, `version` – was das Dashboard zum Start braucht (kein Token nötig) |
| `GET /metrics` | Prometheus-Metriken |
| `GET /healthz` | Lebendigkeitsprüfung und Version (kein Token nötig) |
| `WS /live` (`/ws/live` hinter einem Reverse Proxy) | Live-Push: Ereignisse `reading`, `anomaly`, `device_status`. Mit Token: `?token=<Token>` |

### Engineering-Praxis
- Strenge Compiler-Warnungen (`-Wall -Wextra -Wpedantic -Wconversion ...`) und ein Build ohne eine einzige Warnung; optional als Fehler (`IRONPULSE_WARNINGS_AS_ERRORS`)
- 92 Tests: Unit-Tests für jede Komponente, Tests der HTTP-API und des WebSocket-Handshakes über echte Sockets, End-to-End-Tests mit einem gefälschten Modbus-Gerät über TCP (Dekodierung, Grenzwerte, Timeouts, Ausfall und Wiederherstellung). Alle grün unter AddressSanitizer/UBSan und ThreadSanitizer
- CI: GCC und Clang, Sanitizer, `clang-format`, `ruff` für den Simulator, Vollständigkeit der Übersetzungen, Docker-Build und ein Compose-Smoke-Test, der Live-Daten prüft – [`.github/workflows/ci.yml`](.github/workflows/ci.yml)
- Releases per Tag: nativ gebaute Images für amd64 und arm64, veröffentlicht in der GitHub Container Registry – [`.github/workflows/release.yml`](.github/workflows/release.yml)
- Mehrstufiger Docker-Build, unprivilegierter Benutzer im Container, Health-Checks für jeden Dienst
- Das Dashboard hat keine externen Laufzeitabhängigkeiten (Chart.js ist eingebettet) und funktioniert daher auch in eingeschränkten oder Offline-Netzen

### Roadmap
- [x] Asynchroner Modbus-TCP-Client + Frame-Codec
- [x] Zeitreihenspeicher mit WAL-Persistenz und Aufbewahrung
- [x] Z-Score-, EWMA- und CUSUM-Erkennung, kombiniert über eine `RuleEngine` mit Quorum-Abstimmung
- [x] REST-API + selbst geschriebener WebSocket (RFC 6455) für Live-Push
- [x] Konfiguration über Umgebungsvariablen für Container
- [x] Google-Benchmark-Suite für die Hot Paths von storage/protocol
- [x] Integrationstest: vollständige Kette vom simulierten Gerät bis zum veröffentlichten Alarm
- [x] Automatische, periodische WAL-Bereinigung
- [x] Multi-Architektur-Docker-Images (linux/amd64 + linux/arm64)
- [x] Mehrere Sensoren pro Gerät, Datentypen, Skalierung, Input-Register
- [x] Feste Grenzwerte, Alarmschweregrade, Ruhezeit
- [x] Benachrichtigungen: Telegram, Slack, Webhook – in 8 Sprachen
- [x] Tokenzugriff, Prometheus-Metriken, CSV-Export
- [ ] Modbus RTU (RS-485) direkt, ohne Gateway
- [ ] OPC UA und MQTT als Datenquellen
- [ ] Quittierung von Alarmen durch Bediener und Audit-Log
- [ ] Benutzer und Rollen statt eines einzelnen Tokens

## Mitwirken
Issues und Pull Requests sind willkommen. Bevor Sie einen PR öffnen:
```bash
cmake --preset debug && cmake --build --preset debug -j$(nproc)
ctest --preset debug --output-on-failure
find include src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format --dry-run --Werror
ruff check tools/ && ruff format --check tools/
```
Die CI führt dieselben Prüfungen (plus Sanitizer und Docker-Build) für jeden Pull Request aus. Die Änderungen je Version stehen in [CHANGELOG.md](CHANGELOG.md).

## Lizenz
MIT – siehe [LICENSE](LICENSE). Frei nutzbar, veränderbar und einsetzbar, auch kommerziell.
