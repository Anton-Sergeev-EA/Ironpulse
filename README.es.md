# Ironpulse

**Monitorización industrial en tiempo real: vigila los sensores de sus equipos y le avisa cuando algo va mal, antes de que se rompa.**

[![CI](https://img.shields.io/badge/CI-GitHub_Actions-2088FF?logo=githubactions&logoColor=white)](.github/workflows/ci.yml)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](CMakeLists.txt)
[![Docker](https://img.shields.io/badge/Docker-amd64%20%7C%20arm64-2496ED?logo=docker&logoColor=white)](deploy/docker)
[![Languages](https://img.shields.io/badge/UI-8%20languages-8A2BE2)](#interface-languages)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-131%20passing-brightgreen)](tests)

[Русский](README.md) · [English](README.en.md) · [中文](README.zh.md) · [हिन्दी](README.hi.md) · **Español** · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md)

## ¿Qué es?
Imagine una planta con decenas de sensores: temperatura del devanado de un transformador, vibración del rodamiento de un motor, presión de una bomba. Hoy alguien tiene que notarlo en una pantalla —o peor, oír un ruido fuerte— para darse cuenta de que una pieza se está sobrecalentando o está a punto de fallar.

**Ironpulse vigila esos sensores continuamente y le avisa de inmediato cuando las lecturas salen de su rango permitido o empiezan a comportarse de forma inusual**, con los mismos métodos estadísticos que usan los ingenieros de mantenimiento predictivo. Los límites fijos detectan lo que ya es peligroso; la estadística detecta tanto los picos repentinos como el deterioro lento y progresivo, mucho antes de que el valor alcance su límite.

Obtiene:
- **Un panel en vivo** en 8 idiomas: el valor actual de cada sensor en sus propias unidades, un gráfico con líneas de límite y una lista de alertas con su gravedad.
- **Notificaciones** a Telegram, Slack o cualquier webhook, en el idioma de su equipo, incluidas las caídas y recuperaciones de los dispositivos.
- **Historial en disco**: un reinicio no pierde nada, y cualquier sensor puede exportarse a CSV durante todo el periodo de retención.
- **API y métricas** para otros sistemas: REST, WebSocket y `/metrics` para Prometheus/Grafana.
- **Acceso protegido por token** a la API, al flujo en vivo y a las métricas.

Ironpulse se comunica con equipos industriales reales mediante **Modbus TCP**, el protocolo que usa una enorme parte de los sensores, PLC y medidores del mundo. Lee registros holding e input, entiende enteros de 16 y 32 bits y números de coma flotante en cualquier orden de palabras, y aplica escala y desplazamiento: funciona con los mapas de registros de dispositivos reales, no solo con la demo.

### ¿Para quién es?
- **Ingenieros y técnicos** que necesitan una herramienta de monitorización ligera y autoalojada, sin pagar (ni esperar a que TI apruebe) una plataforma SCADA/IIoT comercial.
- **Desarrolladores** interesados en una base de código C++20 de nivel de producción: red asíncrona, un servidor WebSocket propio, API REST, despliegue en contenedores y un conjunto de pruebas real, sin frameworks pesados.
- **Estudiantes y aficionados** que quieren ver cómo funciona de principio a fin una cadena de monitorización industrial.

**No** hace falta saber C++ para ejecutarlo: vea el [Inicio rápido](#quick-start). Docker lo compila todo.

## Cómo se ve
A la izquierda, los dispositivos y su estado de conexión. En el centro, una tarjeta por sensor: nombre, valor actual grande en las unidades del sensor (en rojo si está fuera de sus límites), un gráfico en vivo con líneas de límite discontinuas y un botón de exportación a CSV. A la derecha, la lista de alertas: «crítico» en rojo para límites superados, «advertencia» en ámbar para anomalías estadísticas. Todo se actualiza en tiempo real sin recargar la página y funciona igual de bien en la pantalla de una sala de control que en un teléfono.

<a id="interface-languages"></a>
## Idiomas de la interfaz
El panel y las notificaciones están disponibles en 8 idiomas: **ruso** (principal), inglés, chino, hindi, español, francés, alemán e italiano; el selector está en la esquina superior derecha. Esta documentación también está en estos 8 idiomas. El idioma del panel se elige en este orden:
1. el parámetro `?lang=` de la dirección, p. ej. `http://localhost:8080/?lang=es` (útil para compartir un enlace);
2. el idioma elegido anteriormente en este navegador;
3. el idioma del navegador, si es compatible;
4. en otro caso, ruso.

El idioma de las notificaciones se define en la configuración (`notifications.language`); vea [Notificaciones](#notifications).

Las traducciones del panel están en [`web/js/i18n/locales/`](web/js/i18n/locales): para añadir un idioma, copie `ru.js`, traduzca los valores y añada el archivo a `web/index.html` y a la lista `supported` de [`web/js/i18n/i18n.js`](web/js/i18n/i18n.js). La CI comprueba que todos los idiomas definan exactamente el mismo conjunto de textos.

<a id="quick-start"></a>
## Inicio rápido
Solo necesita [Docker](https://docs.docker.com/get-docker/). Compila todo lo demás (el compilador de C++ y todas las bibliotecas) dentro de un contenedor, así que su equipo queda limpio.
```bash
git clone https://github.com/Anton-Sergeev-EA/Ironpulse.git
cd Ironpulse/deploy/docker
docker compose up --build
```
Espere a que termine (un par de minutos la primera vez: se compila C++ desde el código fuente) y abra:
- **http://localhost:8080**: el panel en vivo
- **http://localhost:8080/api/v1/sensors**: la API en bruto, si le interesa

Listo. Ahora está observando dos dispositivos simulados: un transformador (temperatura del devanado y del aceite) y una bomba (vibración del rodamiento y presión). El simulador produce de vez en cuando picos y desviaciones prolongadas: observe cómo aparecen advertencias y eventos críticos en el panel «Alertas activas».

Con cada versión se publican imágenes listas para amd64 y arm64 (Raspberry Pi, pasarelas ARM):
```bash
docker pull ghcr.io/anton-sergeev-ea/ironpulse:latest
```

### ¿Algo no funciona?
- **¿Puerto ocupado?** Es muy habitual si ya hay algo en el 8080 (Jenkins, otro panel…). Copie `.env.example` a `.env` en `deploy/docker/`, cambie `HTTP_PORT`/`WS_PORT`/`NGINX_PORT` por puertos libres y vuelva a ejecutar `docker compose up --build`. Un archivo, un cambio.
- **¿Sigue sin funcionar?** Vea [Solución de problemas](#troubleshooting).

## Qué hay en pantalla y qué significan los términos

| Término | Explicación sencilla |
|---|---|
| **Modbus** | Un «idioma» con décadas de antigüedad, pero todavía omnipresente, que usan sensores y controladores industriales para comunicarse con el software. Ironpulse lo habla directamente. |
| **Registro** | Una celda de memoria del dispositivo que contiene una lectura. La documentación de todo dispositivo Modbus incluye un mapa de registros que indica qué significa cada dirección. |
| **Límite** | Una frontera que el valor no debe cruzar, p. ej. temperatura del devanado superior a 90 °C. Cruzarla genera una alerta **crítica**. |
| **Anomalía** | Una lectura que no encaja con el patrón normal reciente del sensor: un pico repentino o una deriva lenta que dura demasiado. Es una **advertencia**: el valor sigue dentro de los límites, pero se comporta de forma inusual. |
| **Confianza** | Lo seguro que está el sistema de que una lectura marcada es una anomalía real y no ruido. Cuanto más alta, más certeza. |
| **Z-score, EWMA, CUSUM** | Tres métodos estadísticos de detección, cada uno bueno para un *tipo* distinto de problema (vea [Cómo funciona la detección de anomalías](#how-anomaly-detection-works)). Ironpulse ejecuta varios a la vez y solo da la alarma cuando suficientes coinciden: menos falsos positivos. |
| **WAL (write-ahead log)** | Una red de seguridad: cada lectura se escribe en disco al llegar, así que un reinicio no pierde el historial reciente. |
| **API REST / WebSocket** | Dos formas de obtener datos de Ironpulse desde otro software: REST, «dame el estado actual»; WebSocket, «mantenme informado en tiempo real». |

<a id="how-anomaly-detection-works"></a>
## Cómo funciona la detección de anomalías
Dos mecanismos independientes procesan cada lectura.

**Límites fijos** (`limits`). Un valor por debajo de `low` o por encima de `high` genera una alerta crítica. Se dispara una vez cuando el valor sale del rango permitido —no en cada sondeo mientras sigue fuera— y otra vez solo después de que haya vuelto.

**Estadística.** Un único umbral fijo o bien pasa por alto los problemas lentos (la vibración de un rodamiento que sube durante días), o bien da falsas alarmas con ruido inofensivo. Por eso cada sensor puede ejecutar varias estrategias independientes, y la alarma solo se da cuando suficientes de ellas (`votes_required`) coinciden:
- **Z-score**: marca una lectura que se aleja de la media reciente del sensor un número inusual de desviaciones estándar. Bueno para picos repentinos.
- **EWMA** (media móvil ponderada exponencialmente): sigue una «normalidad» autoajustable que reacciona a los cambios reales más rápido que una ventana fija.
- **CUSUM** (suma acumulada): acumula indicios de una deriva *sostenida* y detecta degradaciones lentas que una comprobación punto a punto pasaría por alto hasta que fueran graves. Necesita la media (`mean`) y la dispersión (`stddev`) normales del sensor.

Tres detalles más mantienen la lista de alertas útil en lugar de ruidosa:
- **calentamiento**: justo después del arranque los detectores no votan hasta tener datos suficientes (z-score: media ventana; EWMA: ⌈3/α⌉ muestras); de lo contrario, cada reinicio produciría falsas alarmas;
- **tiempo de silencio** (`cooldown_seconds`): una desviación prolongada genera una notificación, no una por segundo;
- **quórum**: un pico aislado de un detector en un sensor ruidoso no despierta al ingeniero de guardia.

## Configurar sus dispositivos
Los dispositivos y sus sensores se describen en el archivo de configuración (`deploy/config/config.example.json` sin Docker, `config.docker.json` con Docker). Un dispositivo real con tres sensores:
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
          "name": "Temperatura del devanado",
          "unit": "°C",
          "register_type": "holding",
          "address": 0,
          "data_type": "float32",
          "word_order": "big",
          "limits": { "high": 90 }
        },
        {
          "id": "oil_temp",
          "name": "Temperatura del aceite",
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
          "name": "Corriente de carga",
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

**Dispositivo:**

| Campo | Por defecto | Significado |
|---|---|---|
| `id` | — | Nombre único del dispositivo: letras latinas, dígitos, `_`, `-`, `.` |
| `host`, `port` | —, `502` | Dónde está accesible el dispositivo (o la pasarela Modbus TCP) |
| `unit_id` | `1` | Dirección Modbus del esclavo/unidad (importante tras pasarelas RS-485) |
| `poll_interval_ms` | `1000` | Periodo de sondeo. Va a ritmo fijo y no se desplaza aunque el dispositivo responda despacio |
| `timeout_ms` | `3000` | Cuánto esperar la respuesta. Un dispositivo que acepta la conexión pero no responde se marca como inaccesible |
| `sensors` | un `uint16` en el registro 0 | Lista de sensores del dispositivo |

**Sensor:**

| Campo | Por defecto | Significado |
|---|---|---|
| `id` | — | Nombre único (entre todos los dispositivos); también es el nombre de su archivo de historial |
| `name`, `unit` | `id`, vacío | Nombre y unidades para el panel y las notificaciones |
| `register_type` | `holding` | `holding` (función 0x03) o `input` (0x04) |
| `address` | `0` | Dirección del primer registro (desde cero, como en el protocolo) |
| `data_type` | `uint16` | `uint16`, `int16`, `uint32`, `int32`, `float32` (los de 32 bits ocupan dos registros) |
| `word_order` | `big` | Para valores de 32 bits: `big`, palabra alta primero (ABCD); `little`, palabra baja primero (CDAB) |
| `scale`, `offset` | `1`, `0` | valor = bruto × `scale` + `offset` |
| `limits.low`, `limits.high` | ninguno | Límites fijos; cruzarlos es una alerta crítica |
| `detection.detectors` | z-score + EWMA | Estrategias: `{"type": "zscore", "window": 60, "threshold": 3}`, `{"type": "ewma", "alpha": 0.2, "threshold": 3}`, `{"type": "cusum", "mean": …, "stddev": …, "slack": 0.5, "threshold": 5}`. Una lista vacía desactiva la estadística |
| `detection.votes_required` | `1` | Cuántas estrategias deben coincidir |
| `detection.cooldown_seconds` | `30` | Intervalo mínimo entre dos alertas del mismo tipo para este sensor |

Ironpulse agrupa los registros vecinos de un dispositivo en el menor número posible de peticiones (hasta 125 registros cada una): diez sensores suelen leerse en una o dos peticiones, no en diez.

La configuración se valida al arrancar y los errores señalan el campo exacto, p. ej. `config: devices[0].sensors[2].data_type: unknown value 'float64' (expected one of: uint16, int16, uint32, int32, float32)`. Los dispositivos de configuraciones antiguas (sin lista `sensors`) siguen funcionando como antes.

Cualquier valor de texto puede hacer referencia a una variable de entorno: `"host": "${PLC_HOST}"`. Los ajustes principales también pueden sobrescribirse con variables `IRONPULSE_*` sin editar el archivo; vea [`docs/adr/0003-env-driven-config.md`](docs/adr/0003-env-driven-config.md).

<a id="notifications"></a>
## Notificaciones
Ironpulse envía alertas y caídas de dispositivos a Telegram, Slack y cualquier dirección HTTP (webhook). Los textos están en el idioma elegido e indican el sensor, el valor, el límite y las unidades:
```
🔴 CRÍTICO
Temperatura del devanado: el valor 142 °C supera el límite 90 °C
Dispositivo: transformer_01
Hora: 2026-10-02 06:54:09 UTC
Panel: https://monitoring.example.com
```

La forma más sencilla de activarlas es `.env` (Docker) o variables de entorno:

| Variable | Significado |
|---|---|
| `TELEGRAM_BOT_TOKEN`, `TELEGRAM_CHAT_ID` | Un bot creado con [@BotFather](https://t.me/BotFather) y añadido a su chat o grupo |
| `SLACK_WEBHOOK_URL` | Un Incoming Webhook de la configuración de Slack |
| `WEBHOOK_URL` | Cualquier dirección que acepte un POST con JSON |
| `NOTIFY_LANGUAGE` | `ru`, `en`, `zh`, `hi`, `es`, `fr`, `de` o `it` |
| `IRONPULSE_PUBLIC_URL` | La dirección del panel, enlazada desde las notificaciones |

(Sin Docker: `IRONPULSE_TELEGRAM_BOT_TOKEN`, `IRONPULSE_TELEGRAM_CHAT_ID`, `IRONPULSE_SLACK_WEBHOOK_URL`, `IRONPULSE_WEBHOOK_URL`, `IRONPULSE_NOTIFY_LANGUAGE`.)

Lo mismo en el archivo de configuración, con opciones adicionales:
```json
"notifications": {
  "language": "es",
  "min_severity": "critical",
  "dashboard_url": "https://monitoring.example.com",
  "channels": [
    { "type": "telegram", "bot_token": "${TG_TOKEN}", "chat_id": "-1001234567890" },
    { "type": "webhook", "url": "https://hooks.example.com/ironpulse",
      "headers": { "Authorization": "Bearer ${HOOK_SECRET}" } }
  ]
}
```
- `min_severity: "critical"`: solo límites superados y caídas, sin advertencias estadísticas.
- Un canal de Telegram admite `url`: su propio servidor de Bot API o un proxy, si `api.telegram.org` no es accesible desde la red de la planta.
- Los webhooks reciben JSON estructurado: `type` (`alert`, `device_offline`, `device_online`), `severity`, `title`, `text`, `timestamp`, `device_id`, `sensor_id` y un objeto `alert` con `kind`, `value`, `limit`, `direction`, `votes`, etc.; cómodo para sistemas de tickets y automatización.

El envío se hace en segundo plano y nunca ralentiza el sondeo; los errores del servidor se reintentan hasta tres veces con pausas crecientes. Un canal sin dirección o token se desactiva con una advertencia en el registro en lugar de detener el sistema.

## Seguridad
Por defecto la API está abierta: cómodo para un primer vistazo, no para un servidor en red. Defina un token:
```bash
# deploy/docker/.env
IRONPULSE_API_TOKEN=$(openssl rand -hex 32)
```
A partir de entonces la API REST, el flujo WebSocket en vivo y `/metrics` exigen `Authorization: Bearer <token>`. El panel pide el token una vez y lo recuerda en el navegador. `/healthz` sigue abierto para las comprobaciones de estado.

También incluye: validación de los parámetros de las peticiones (`400` en lugar de un fallo), límites al tamaño de las tramas y cabeceras WebSocket, desconexión de clientes bloqueados, comparación del token en tiempo constante, identificadores de sensores seguros para el sistema de archivos, un usuario sin privilegios en el contenedor y una unidad systemd aislada.

## Monitorizar el propio Ironpulse
`GET /metrics` sirve métricas de Prometheus; recójalas con Prometheus/Grafana o VictoriaMetrics:

| Métrica | Muestra |
|---|---|
| `ironpulse_sensor_value{sensor,unit}` | Último valor de cada sensor |
| `ironpulse_device_up{device}` | 1: el dispositivo responde; 0: no responde |
| `ironpulse_readings_total{sensor}` | Lecturas recibidas |
| `ironpulse_alerts_total{sensor,kind,severity}` | Alertas generadas |
| `ironpulse_poll_errors_total{device}` | Peticiones fallidas a un dispositivo |
| `ironpulse_notifications_total{channel,result}` | Entregas de notificaciones |
| `ironpulse_websocket_clients`, `ironpulse_uptime_seconds`, `ironpulse_build_info{version}` | Estado del propio servicio |

```yaml
# prometheus.yml
scrape_configs:
  - job_name: ironpulse
    authorization: { credentials: "<token>" }   # si IRONPULSE_API_TOKEN está definido
    static_configs: [{ targets: ["ironpulse-host:8080"] }]
```

## Exportación del flujo de datos
Además del panel, Ironpulse puede escribir cada lectura en archivos binarios compactos: para un data lake, un historiador, análisis sin conexión o entrenamiento de modelos. La escritura en disco se hace en un hilo aparte: un disco lento o lleno nunca retrasa el sondeo ni las alertas; en el peor caso algunas lecturas no llegan a la exportación, y las métricas lo muestran.

```json
"export": { "enabled": true, "directory": "export", "segment_max_mb": 64 }
```

En Docker basta con `EXPORT_ENABLED=true` en `.env`: los archivos aparecen en el volumen de datos, en `/app/data/export`.

Los archivos se llaman `telemetry-<hora>-NNNNNN.ipseg`: 24 bytes por lectura, una suma CRC-32C por lote y la lista de sensores dentro de cada archivo, así que cada uno se puede leer por sí solo, incluso después de cambiar la configuración. Un archivo que aún se está escribiendo termina en `.part`: recoja solo los `.ipseg` terminados. Ironpulse no los borra; la retención de los datos exportados corre de su cuenta.

```bash
ironpulse-export verify export/                       # comprobar el CRC de cada lote
ironpulse-export dump export/ > readings.csv          # todas las lecturas en CSV
ironpulse-export dump --sensor winding_temp export/   # solo un sensor
```

Descripción del formato: [`docs/export-format.md`](docs/export-format.md); lector en Python sin dependencias: [`tools/export_reader/read_segment.py`](tools/export_reader/read_segment.py); métricas: `ironpulse_export_*` en `/metrics`.

## Opciones de despliegue

| Escenario | Instrucciones |
|---|---|
| Solo quiero verlo funcionar en local | [Inicio rápido](#quick-start) |
| Mi propio servidor/VPS, sin nada más | [Docker Compose en un VPS dedicado](#dedicated-vps) |
| El servidor ya aloja otros sitios con su nginx + HTTPS | [Detrás de un nginx existente](#behind-nginx) |
| Sin Docker, compilando C++ directamente | [Compilar desde el código fuente](#build-from-source) |

<a id="dedicated-vps"></a>
### Docker Compose en un VPS dedicado
```bash
cd deploy/docker
cp .env.example .env          # defina IRONPULSE_API_TOKEN y, si quiere, las notificaciones
docker compose up -d --build
```
El panel está en `http://<ip-del-servidor>:8080` (y en el puerto 80 a través del nginx incluido). Para un uso público real, ponga un certificado TLS delante; vea la siguiente sección.

<a id="behind-nginx"></a>
### Detrás de un nginx existente (y un dominio)
Si su VPS ya aloja otros sitios con su propio nginx y certificados TLS (p. ej. con [certbot](https://certbot.eff.org/)), Ironpulse puede sumarse como un sitio más en lugar de pelear por los puertos 80/443:
```bash
cd deploy/docker
cp .env.example .env
# Defina IRONPULSE_BIND_HOST=127.0.0.1 en .env para que ironpulse
# solo sea accesible desde esta máquina, no directamente desde internet.
docker compose -f docker-compose.yml -f docker-compose.prod.yml up -d --build
```
Así los puertos de ironpulse se vinculan solo a `127.0.0.1` y se omite el nginx incluido (`docker-compose.prod.yml`), de modo que su nginx existente es el único punto de terminación TLS. Después añada un vhost: [`deploy/nginx/anton-tests.ru.conf`](deploy/nginx/anton-tests.ru.conf) es un ejemplo listo; cópielo y ponga su dominio y las rutas de su certificado:
```bash
sudo cp deploy/nginx/anton-tests.ru.conf /etc/nginx/sites-available/your-domain.tld
sudo ln -s /etc/nginx/sites-available/your-domain.tld /etc/nginx/sites-enabled/
sudo nginx -t && sudo systemctl reload nginx
```

### Lista de comprobación para producción
- [ ] `IRONPULSE_API_TOKEN` está definido (largo y aleatorio: `openssl rand -hex 32`)
- [ ] `IRONPULSE_BIND_HOST=127.0.0.1` si hay un proxy inverso delante (nunca exponga el puerto directo de la aplicación)
- [ ] El certificado TLS es válido y se renueva solo (`certbot renew --dry-run`)
- [ ] `persistence_enabled: true` si el historial debe sobrevivir a los reinicios
- [ ] `retention_hours` se ajusta a su disco: unos 16 bytes por lectura; los registros antiguos se eliminan automáticamente cada hora
- [ ] `devices` apunta a sus dispositivos Modbus **reales**, no al simulador incluido, y cada sensor tiene `limits`
- [ ] Notificaciones probadas: baje temporalmente un límite por debajo del valor actual y compruebe que llega el mensaje
- [ ] Espacio en disco suficiente para la compilación: Docker necesita temporalmente unos 3-4 GB, aunque la imagen final ocupa menos de 250 MB (`docker builder prune` después)
- [ ] Todas las comprobaciones de estado en verde: `docker compose ps` muestra cada servicio como `healthy`

<a id="build-from-source"></a>
### Compilar desde el código fuente (sin Docker)
Necesita un compilador de C++20 (GCC ≥ 12 o Clang ≥ 15), CMake ≥ 3.20, Ninja y OpenSSL ≥ 3 (`libssl-dev`, para las notificaciones por HTTPS). Todo lo demás (Asio, spdlog, nlohmann/json, cpp-httplib, Catch2) se descarga automáticamente.
```bash
cmake --preset debug
cmake --build --preset debug -j$(nproc)
ctest --preset debug --output-on-failure
```
Otros presets: `release`, `asan` (AddressSanitizer + UBSan), `tsan` (ThreadSanitizer).

Para ejecutarlo de verdad necesita dispositivos Modbus que sondear; use el simulador incluido para una demo autónoma:
```bash
pip install -r tools/modbus_simulator/requirements.txt
python3 tools/modbus_simulator/simulator.py --port 5020 &

./build/debug/ironpulse deploy/config/config.example.json
```
`ironpulse --version` muestra la versión. Vea [`deploy/systemd/ironpulse.service`](deploy/systemd/ironpulse.service) para una unidad systemd reforzada; lee los secretos de `/etc/ironpulse/ironpulse.env`.

<a id="troubleshooting"></a>
## Solución de problemas
**«Port is already allocated» al iniciar Docker.** Otro programa usa ese puerto. Deténgalo o cambie el puerto en `.env`.

**Ironpulse se cierra con `Failed to load config ... config: devices[0]...`.** La configuración tiene un error; el mensaje indica el campo exacto y el problema.

**El panel pide un token que nunca definí.** Hay un token en el entorno (`IRONPULSE_API_TOKEN` en `.env` o `api_token` en la configuración). Introdúzcalo, o elimínelo si el acceso debe ser abierto.

**El dispositivo aparece «desconectado» y el registro dice `device unreachable: cannot connect`.** Ironpulse no alcanza `host`/`port`. Con Docker Compose, `host` debe ser el nombre del servicio Docker (p. ej. `simulator`), no `127.0.0.1`.

**El registro dice `poll failed: device exception code 2`.** El dispositivo responde pero rechaza los registros pedidos: el código 2 significa «dirección no válida». Compare `address`, `register_type` y `data_type` con el mapa de registros del dispositivo (lo habitual es confundir la numeración desde cero o desde uno, o holding con input).

**Los valores parecen basura (números enormes, negativos en lugar de positivos).** Casi siempre es `data_type` o `word_order`: pruebe `word_order: "little"` en valores de 32 bits, `int16` en lugar de `uint16` para magnitudes que pueden ser negativas, y revise `scale`.

**No llegan las notificaciones.** Revise el registro: `Notification via telegram failed (HTTP 401)`: token del bot incorrecto; `HTTP 400`: `chat_id` incorrecto o el bot no está en el chat; `network error`: el servidor no tiene acceso a internet (los canales de Telegram aceptan un proxy o su propio servidor de Bot API en `url`). La métrica `ironpulse_notifications_total{result="error"}` cuenta las entregas fallidas.

**Cambié un archivo pero Docker sigue sirviendo la versión anterior.** `docker compose build --no-cache` y, si no basta, antes `docker builder prune -af`.

## Arquitectura (para desarrolladores)
```
Dispositivos Modbus TCP → protocol (sondeo, decodificación) → EventBus
                                                                │
           ┌──────────────────────┬──────────────────────┬──────┴────────────┐
           ▼                      ▼                      ▼                   ▼
storage (ring buffer + WAL)  analytics (límites,    api (REST, WebSocket,  metrics
                             detectores, quórum)    /metrics) → panel
                                     │
                                     ▼
                             notify (Telegram, Slack, webhook)
```
Cada capa (`core`, `protocol`, `storage`, `ingest`, `analytics`, `api`, `notify`) es un objetivo CMake independiente con sus propias pruebas. Las capas se comunican mediante el `EventBus`, no directamente. Vea [`docs/architecture.md`](docs/architecture.md) para el modelo de concurrencia y [`docs/adr/`](docs/adr/) para las decisiones concretas.

### Referencia de la API
Contrato completo: [`docs/openapi.yaml`](docs/openapi.yaml) (OpenAPI 3; ábralo en Swagger UI o impórtelo en Postman).

| Endpoint | Descripción |
|---|---|
| `GET /api/v1/devices` | Dispositivos: `id`, `online`, `poll_interval_ms`, lista de sensores |
| `GET /api/v1/sensors` | Sensores: `id`, `name`, `unit`, `device_id`, `limits`, último valor |
| `GET /api/v1/series/{sensor_id}?since=<segundos>` | Historial del periodo (300 s por defecto; con WAL, todo el periodo de retención). Los rangos largos se reducen a 10 000 puntos |
| `GET /api/v1/series/{sensor_id}?since=<segundos>&format=csv` | Lo mismo en CSV, nunca reducido |
| `GET /api/v1/alerts?limit=<n>` | Alertas recientes, primero las más nuevas (1–1000, 50 por defecto): `kind`, `severity`, `value`, `limit`, `direction`, `votes`, `detectors_total`, `confidence` |
| `GET /api/v1/config` | `ws_port`, `ws_host`, `auth_required`, `version`: lo que el panel necesita para arrancar (no requiere token) |
| `GET /metrics` | Métricas de Prometheus |
| `GET /healthz` | Comprobación de estado y versión (no requiere token) |
| `WS /live` (`/ws/live` tras un proxy inverso) | Envío en vivo: eventos `reading`, `anomaly`, `device_status`. Con token: `?token=<token>` |

### Prácticas de ingeniería
- Avisos estrictos del compilador (`-Wall -Wextra -Wpedantic -Wconversion ...`) y compilación sin un solo aviso; opcionalmente como errores (`IRONPULSE_WARNINGS_AS_ERRORS`)
- 131 pruebas: unitarias de cada componente, de la API HTTP y del handshake WebSocket sobre sockets reales, y de extremo a extremo con un dispositivo Modbus falso por TCP (decodificación, límites, tiempos de espera, caída y recuperación). Todas en verde con AddressSanitizer/UBSan y ThreadSanitizer
- CI: GCC y Clang, sanitizers, `clang-format`, `ruff` para el simulador, completitud de las traducciones, compilación de Docker y una prueba de humo con Compose que verifica datos en vivo: [`.github/workflows/ci.yml`](.github/workflows/ci.yml)
- Versiones por etiqueta: imágenes compiladas de forma nativa para amd64 y arm64 y publicadas en GitHub Container Registry: [`.github/workflows/release.yml`](.github/workflows/release.yml)
- Compilación Docker en varias etapas, usuario sin privilegios en el contenedor y comprobaciones de estado en cada servicio
- El panel no tiene dependencias externas en tiempo de ejecución (Chart.js va incluido), por lo que funciona en redes restringidas o sin conexión

### Hoja de ruta
- [x] Cliente Modbus TCP asíncrono + códec de tramas
- [x] Almacenamiento de series temporales con persistencia WAL y retención
- [x] Detección z-score, EWMA y CUSUM combinada por un `RuleEngine` con votación por quórum
- [x] API REST + WebSocket propio (RFC 6455) para envío en vivo
- [x] Configuración por variables de entorno para contenedores
- [x] Suite de Google Benchmark para las rutas críticas de storage/protocol
- [x] Prueba de integración: de un dispositivo simulado a una alerta publicada
- [x] Retención automática y periódica del WAL
- [x] Imágenes Docker multiarquitectura (linux/amd64 + linux/arm64)
- [x] Varios sensores por dispositivo, tipos de datos, escalado, registros input
- [x] Límites fijos, gravedad de las alertas, tiempo de silencio
- [x] Notificaciones: Telegram, Slack, webhook, en 8 idiomas
- [x] Acceso por token, métricas de Prometheus, exportación a CSV
- [x] Exportación de todas las lecturas a archivos con CRC (módulo `ingest`, antes apollonian_core_ingestor)
- [ ] Modbus RTU (RS-485) directo, sin pasarela
- [ ] OPC UA y MQTT como fuentes de datos
- [ ] Reconocimiento de alertas por los operadores y registro de auditoría
- [ ] Usuarios y roles en lugar de un único token

## Contribuir
Las issues y pull requests son bienvenidas. Antes de abrir un PR:
```bash
cmake --preset debug && cmake --build --preset debug -j$(nproc)
ctest --preset debug --output-on-failure
find include src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format --dry-run --Werror
ruff check tools/ && ruff format --check tools/
```
La CI ejecuta las mismas comprobaciones (más los sanitizers y la compilación de Docker) en cada pull request. Los cambios de cada versión están en [CHANGELOG.md](CHANGELOG.md).

## Licencia
MIT: vea [LICENSE](LICENSE). Libre para usar, modificar y desplegar, también con fines comerciales.
