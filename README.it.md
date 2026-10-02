# Ironpulse

**Monitoraggio industriale in tempo reale: sorveglia i sensori dei tuoi impianti e ti avvisa quando qualcosa non va, prima che si rompa.**

[![CI](https://img.shields.io/badge/CI-GitHub_Actions-2088FF?logo=githubactions&logoColor=white)](.github/workflows/ci.yml)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](CMakeLists.txt)
[![Docker](https://img.shields.io/badge/Docker-amd64%20%7C%20arm64-2496ED?logo=docker&logoColor=white)](deploy/docker)
[![Languages](https://img.shields.io/badge/UI-8%20languages-8A2BE2)](#interface-languages)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-137%20passing-brightgreen)](tests)

[Русский](README.md) · [English](README.en.md) · [中文](README.zh.md) · [हिन्दी](README.hi.md) · [Español](README.es.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · **Italiano**

## Che cos’è?
Immagina uno stabilimento con decine di sensori: temperatura dell’avvolgimento di un trasformatore, vibrazione del cuscinetto di un motore, pressione di una pompa. Oggi qualcuno deve accorgersene su uno schermo — o peggio, sentire un forte rumore — prima di capire che un componente si sta surriscaldando o sta per guastarsi.

**Ironpulse sorveglia questi sensori in continuo e ti avvisa subito quando le letture escono dall’intervallo consentito o iniziano a comportarsi in modo insolito**, con gli stessi metodi statistici usati dagli ingegneri della manutenzione predittiva. I limiti fissi rilevano ciò che è già pericoloso; la statistica rileva sia i picchi improvvisi sia il degrado lento e progressivo, molto prima che il valore raggiunga il suo limite.

Ottieni:
- **Una dashboard in tempo reale** in 8 lingue: il valore attuale di ogni sensore nella sua unità di misura, un grafico con le linee dei limiti e un elenco di allarmi con la relativa gravità.
- **Notifiche** su Telegram, Slack o qualsiasi webhook, nella lingua del tuo team, comprese le perdite di comunicazione con un dispositivo e il loro ripristino.
- **Storico su disco**: un riavvio non perde nulla e ogni sensore può essere esportato in CSV per l’intero periodo di conservazione.
- **API e metriche** per altri sistemi: REST, WebSocket e `/metrics` per Prometheus/Grafana.
- **Accesso protetto da token** ad API, flusso in tempo reale e metriche.

Ironpulse comunica con impianti industriali reali tramite **Modbus TCP**, il protocollo usato da una larghissima parte di sensori, PLC e contatori nel mondo. Legge registri holding e input, interpreta interi a 16 e 32 bit e numeri in virgola mobile in qualsiasi ordine delle parole e applica scala e offset: funziona con le mappe dei registri di dispositivi reali, non solo con la demo.

### A chi è rivolto?
- **Ingegneri e tecnici** che hanno bisogno di uno strumento di monitoraggio leggero e self-hosted, senza pagare (né aspettare l’approvazione dell’IT per) una piattaforma SCADA/IIoT commerciale.
- **Sviluppatori** interessati a una codebase C++20 di livello produttivo: networking asincrono, un server WebSocket scritto a mano, API REST, deployment in container e una vera suite di test, senza framework pesanti.
- **Studenti e appassionati** che vogliono vedere come funziona dall’inizio alla fine una catena di monitoraggio industriale.

**Non** serve conoscere il C++ per avviarlo: vedi l’[Avvio rapido](#quick-start). Docker compila tutto.

## Com’è fatto
A sinistra i dispositivi e lo stato della loro connessione. Al centro una scheda per ogni sensore: nome, valore attuale in grande nell’unità del sensore (in rosso se fuori dai limiti), un grafico in tempo reale con linee tratteggiate dei limiti e un pulsante di esportazione CSV. A destra l’elenco degli allarmi: «critico» in rosso per i limiti superati, «avviso» in giallo per le anomalie statistiche. Tutto si aggiorna in tempo reale senza ricaricare la pagina e funziona altrettanto bene sul grande schermo di una sala controllo e su uno smartphone.

<a id="interface-languages"></a>
## Lingue dell’interfaccia
La dashboard e le notifiche sono disponibili in 8 lingue: **russo** (lingua principale), inglese, cinese, hindi, spagnolo, francese, tedesco e italiano; il selettore è in alto a destra. Anche questa documentazione è disponibile in queste 8 lingue. La lingua della dashboard viene scelta in quest’ordine:
1. il parametro `?lang=` nell’indirizzo, ad es. `http://localhost:8080/?lang=it` (comodo per condividere un link);
2. la lingua scelta in precedenza in questo browser;
3. la lingua del browser, se supportata;
4. altrimenti il russo.

La lingua delle notifiche si imposta nella configurazione (`notifications.language`): vedi [Notifiche](#notifications).

Le traduzioni della dashboard si trovano in [`web/js/i18n/locales/`](web/js/i18n/locales): per aggiungere una lingua, copia `ru.js`, traduci i valori e aggiungi il file a `web/index.html` e all’elenco `supported` in [`web/js/i18n/i18n.js`](web/js/i18n/i18n.js). La CI verifica che tutte le lingue definiscano esattamente lo stesso insieme di testi.

<a id="quick-start"></a>
## Avvio rapido
Ti serve solo [Docker](https://docs.docker.com/get-docker/). Compila tutto il resto (il compilatore C++ e tutte le librerie) all’interno di un container, così il tuo computer resta pulito.
```bash
git clone https://github.com/Anton-Sergeev-EA/Ironpulse.git
cd Ironpulse/deploy/docker
docker compose up --build
```
Attendi il completamento (un paio di minuti la prima volta: il C++ viene compilato dai sorgenti), poi apri:
- **http://localhost:8080** — la dashboard in tempo reale
- **http://localhost:8080/api/v1/sensors** — l’API grezza, per curiosità

Fatto. Ora stai osservando due dispositivi simulati: un trasformatore (temperatura dell’avvolgimento e dell’olio) e una pompa (vibrazione del cuscinetto e pressione). Il simulatore produce di tanto in tanto picchi e scostamenti prolungati: guarda comparire avvisi ed eventi critici nel pannello «Allarmi attivi».

A ogni rilascio vengono pubblicate immagini pronte per amd64 e arm64 (Raspberry Pi, gateway ARM):
```bash
docker pull ghcr.io/anton-sergeev-ea/ironpulse:latest
```

### Qualcosa non funziona?
- **Porta già occupata?** Capita spesso se sulla 8080 gira già qualcosa (Jenkins, un’altra dashboard…). Copia `.env.example` in `.env` dentro `deploy/docker/`, cambia `HTTP_PORT`/`WS_PORT`/`NGINX_PORT` con porte libere e rilancia `docker compose up --build`. Un file, una modifica.
- **Ancora bloccato?** Vedi [Risoluzione dei problemi](#troubleshooting).

## Cosa c’è sullo schermo e cosa significano i termini

| Termine | Spiegazione semplice |
|---|---|
| **Modbus** | Un «linguaggio» vecchio di decenni ma ancora onnipresente, con cui sensori e controllori industriali parlano con il software. Ironpulse lo parla direttamente. |
| **Registro** | Una cella di memoria del dispositivo che contiene una lettura. La documentazione di ogni dispositivo Modbus include una mappa dei registri che indica il significato di ogni indirizzo. |
| **Limite** | Un confine che il valore non deve superare, ad es. una temperatura dell’avvolgimento oltre i 90 °C. Superarlo genera un allarme **critico**. |
| **Anomalia** | Una lettura che non rientra nell’andamento normale recente del sensore: un picco improvviso o una deriva lenta che dura troppo. È un **avviso**: il valore è ancora entro i limiti ma si comporta in modo insolito. |
| **Affidabilità** | Quanto il sistema è sicuro che una lettura segnalata sia una vera anomalia e non rumore. Più è alta, maggiore la certezza. |
| **Z-score, EWMA, CUSUM** | Tre metodi statistici di rilevamento, ognuno adatto a un *tipo* diverso di problema (vedi [Come funziona il rilevamento delle anomalie](#how-anomaly-detection-works)). Ironpulse ne esegue diversi contemporaneamente e dà l’allarme solo quando un numero sufficiente concorda: meno falsi allarmi. |
| **WAL (write-ahead log)** | Una rete di sicurezza: ogni lettura viene scritta su disco appena arriva, quindi un riavvio non perde lo storico recente. |
| **API REST / WebSocket** | Due modi in cui altri software ottengono dati da Ironpulse: REST, «dammi lo stato attuale»; WebSocket, «tienimi aggiornato in tempo reale». |

<a id="how-anomaly-detection-works"></a>
## Come funziona il rilevamento delle anomalie
Ogni lettura passa per due meccanismi indipendenti.

**Limiti fissi** (`limits`). Un valore sotto `low` o sopra `high` genera un allarme critico. Scatta una volta quando il valore esce dall’intervallo consentito — non a ogni interrogazione finché resta fuori — e di nuovo solo dopo che è rientrato.

**Statistica.** Una singola soglia fissa o non vede i problemi lenti (la vibrazione di un cuscinetto che cresce per giorni) o dà falsi allarmi su rumore innocuo. Per questo ogni sensore può eseguire più strategie indipendenti, e l’allarme scatta solo quando un numero sufficiente di esse (`votes_required`) concorda:
- **Z-score** — segnala una lettura che si discosta dalla media recente del sensore di un numero insolito di deviazioni standard. Adatto ai picchi improvvisi.
- **EWMA** (media mobile a pesi esponenziali) — segue una «normalità» autoregolante che reagisce ai cambiamenti reali più rapidamente di una finestra fissa.
- **CUSUM** (somma cumulativa) — accumula indizi di una deriva *persistente* e rileva un degrado lento che un controllo punto per punto non vedrebbe finché non diventa grave. Richiede la media (`mean`) e la dispersione (`stddev`) normali del sensore.

Altri tre dettagli mantengono l’elenco degli allarmi utile invece che rumoroso:
- **riscaldamento** — subito dopo l’avvio i rilevatori restano in silenzio finché non hanno dati sufficienti (z-score: metà finestra; EWMA: ⌈3/α⌉ campioni); altrimenti ogni riavvio provocherebbe falsi allarmi;
- **tempo di silenzio** (`cooldown_seconds`) — uno scostamento prolungato produce una notifica, non una al secondo;
- **quorum** — un picco isolato di un rilevatore su un sensore rumoroso non sveglia l’ingegnere reperibile.

## Configurare i tuoi dispositivi
I dispositivi e i loro sensori sono descritti nel file di configurazione (`deploy/config/config.example.json` senza Docker, `config.docker.json` con Docker). Un dispositivo reale con tre sensori:
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
          "name": "Temperatura dell’avvolgimento",
          "unit": "°C",
          "register_type": "holding",
          "address": 0,
          "data_type": "float32",
          "word_order": "big",
          "limits": { "high": 90 }
        },
        {
          "id": "oil_temp",
          "name": "Temperatura dell’olio",
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
          "name": "Corrente di carico",
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

| Campo | Predefinito | Significato |
|---|---|---|
| `id` | — | Nome univoco del dispositivo: lettere latine, cifre, `_`, `-`, `.` |
| `host`, `port` | —, `502` | Dove è raggiungibile il dispositivo (o il gateway Modbus TCP) |
| `unit_id` | `1` | Indirizzo Modbus slave/unit (importante dietro i gateway RS-485) |
| `poll_interval_ms` | `1000` | Periodo di interrogazione. Procede a ritmo fisso e non deriva anche se il dispositivo risponde lentamente |
| `timeout_ms` | `3000` | Quanto attendere una risposta. Un dispositivo che accetta la connessione ma resta muto viene segnalato come irraggiungibile |
| `sensors` | un `uint16` nel registro 0 | I sensori del dispositivo |

**Sensore:**

| Campo | Predefinito | Significato |
|---|---|---|
| `id` | — | Nome univoco (tra tutti i dispositivi); è anche il nome del suo file di storico |
| `name`, `unit` | `id`, vuoto | Etichetta e unità per la dashboard e le notifiche |
| `register_type` | `holding` | `holding` (funzione 0x03) o `input` (0x04) |
| `address` | `0` | Indirizzo del primo registro (da zero, come nel protocollo) |
| `data_type` | `uint16` | `uint16`, `int16`, `uint32`, `int32`, `float32` (i tipi a 32 bit occupano due registri) |
| `word_order` | `big` | Per i valori a 32 bit: `big`, parola alta per prima (ABCD); `little`, parola bassa per prima (CDAB) |
| `scale`, `offset` | `1`, `0` | valore = grezzo × `scale` + `offset` |
| `limits.low`, `limits.high` | nessuno | Limiti fissi; superarli è un allarme critico |
| `detection.detectors` | z-score + EWMA | Strategie: `{"type": "zscore", "window": 60, "threshold": 3}`, `{"type": "ewma", "alpha": 0.2, "threshold": 3}`, `{"type": "cusum", "mean": …, "stddev": …, "slack": 0.5, "threshold": 5}`. Un elenco vuoto disattiva la statistica |
| `detection.votes_required` | `1` | Quante strategie devono concordare |
| `detection.cooldown_seconds` | `30` | Intervallo minimo tra due allarmi dello stesso tipo per questo sensore |

Ironpulse raggruppa i registri vicini di un dispositivo nel minor numero possibile di richieste (fino a 125 registri ciascuna): dieci sensori si leggono di solito con una o due richieste, non dieci.

La configurazione viene convalidata all’avvio e gli errori indicano il campo esatto, ad es. `config: devices[0].sensors[2].data_type: unknown value 'float64' (expected one of: uint16, int16, uint32, int32, float32)`. I dispositivi delle configurazioni precedenti (senza elenco `sensors`) continuano a funzionare come prima.

Qualsiasi valore di testo può fare riferimento a una variabile d’ambiente: `"host": "${PLC_HOST}"`. Le impostazioni principali si possono anche sovrascrivere con variabili `IRONPULSE_*` senza modificare il file: vedi [`docs/adr/0003-env-driven-config.md`](docs/adr/0003-env-driven-config.md).

<a id="notifications"></a>
## Notifiche
Ironpulse invia allarmi e perdite di comunicazione a Telegram, Slack e a qualsiasi indirizzo HTTP (webhook). I testi sono nella lingua scelta e indicano sensore, valore, limite e unità:
```
🔴 CRITICO
Temperatura dell’avvolgimento: il valore 142 °C supera il limite 90 °C
Dispositivo: transformer_01
Ora: 2026-10-02 06:54:09 UTC
Dashboard: https://monitoring.example.com
```

Il modo più semplice per attivarle è `.env` (Docker) o le variabili d’ambiente:

| Variabile | Significato |
|---|---|
| `TELEGRAM_BOT_TOKEN`, `TELEGRAM_CHAT_ID` | Un bot creato con [@BotFather](https://t.me/BotFather) e aggiunto alla tua chat o al tuo gruppo |
| `SLACK_WEBHOOK_URL` | Un Incoming Webhook dalle impostazioni di Slack |
| `WEBHOOK_URL` | Qualsiasi indirizzo che accetti un POST JSON |
| `NOTIFY_LANGUAGE` | `ru`, `en`, `zh`, `hi`, `es`, `fr`, `de` o `it` |
| `IRONPULSE_PUBLIC_URL` | L’indirizzo della dashboard, linkato dalle notifiche |

(Senza Docker: `IRONPULSE_TELEGRAM_BOT_TOKEN`, `IRONPULSE_TELEGRAM_CHAT_ID`, `IRONPULSE_SLACK_WEBHOOK_URL`, `IRONPULSE_WEBHOOK_URL`, `IRONPULSE_NOTIFY_LANGUAGE`.)

Lo stesso nel file di configurazione, con opzioni aggiuntive:
```json
"notifications": {
  "language": "it",
  "min_severity": "critical",
  "dashboard_url": "https://monitoring.example.com",
  "channels": [
    { "type": "telegram", "bot_token": "${TG_TOKEN}", "chat_id": "-1001234567890" },
    { "type": "webhook", "url": "https://hooks.example.com/ironpulse",
      "headers": { "Authorization": "Bearer ${HOOK_SECRET}" } }
  ]
}
```
- `min_severity: "critical"` — solo limiti superati e perdite di comunicazione, senza avvisi statistici.
- Un canale Telegram accetta un `url`: il tuo server Bot API o un proxy, se `api.telegram.org` non è raggiungibile dalla rete dello stabilimento.
- I webhook ricevono JSON strutturato: `type` (`alert`, `device_offline`, `device_online`), `severity`, `title`, `text`, `timestamp`, `device_id`, `sensor_id` e un oggetto `alert` con `kind`, `value`, `limit`, `direction`, `votes` ecc.; comodo per sistemi di ticketing e automazione.

L’invio avviene in background e non rallenta mai l’interrogazione; gli errori del server vengono ritentati fino a tre volte con pause crescenti. Un canale privo di indirizzo o token viene disattivato con un avviso nel log, invece di fermare il sistema.

## Sicurezza
Per impostazione predefinita l’API è aperta: comodo per una prima prova, non per un server in rete. Imposta un token:
```bash
# deploy/docker/.env
IRONPULSE_API_TOKEN=$(openssl rand -hex 32)
```
Da quel momento API REST, flusso WebSocket in tempo reale e `/metrics` richiedono `Authorization: Bearer <token>`. La dashboard chiede il token una sola volta e lo ricorda nel browser. `/healthz` resta aperto per i controlli di stato.

Inoltre, già inclusi: convalida dei parametri delle richieste (`400` invece di un crash), dimensione limitata di frame e intestazioni WebSocket, disconnessione dei client bloccati, confronto del token a tempo costante, identificatori dei sensori sicuri per il filesystem, un utente non privilegiato nel container e un’unità systemd isolata.

## Monitorare Ironpulse stesso
`GET /metrics` espone metriche Prometheus; raccoglile con Prometheus/Grafana o VictoriaMetrics:

| Metrica | Mostra |
|---|---|
| `ironpulse_sensor_value{sensor,unit}` | Ultimo valore di ogni sensore |
| `ironpulse_device_up{device}` | 1 — il dispositivo risponde, 0 — no |
| `ironpulse_readings_total{sensor}` | Letture ricevute |
| `ironpulse_alerts_total{sensor,kind,severity}` | Allarmi generati |
| `ironpulse_poll_errors_total{device}` | Richieste fallite verso un dispositivo |
| `ironpulse_notifications_total{channel,result}` | Consegne delle notifiche |
| `ironpulse_websocket_clients`, `ironpulse_uptime_seconds`, `ironpulse_build_info{version}` | Stato del servizio stesso |

```yaml
# prometheus.yml
scrape_configs:
  - job_name: ironpulse
    authorization: { credentials: "<token>" }   # se IRONPULSE_API_TOKEN è impostato
    static_configs: [{ targets: ["ironpulse-host:8080"] }]
```

## Esportazione del flusso di dati
Oltre alla dashboard, Ironpulse può scrivere ogni lettura in file binari compatti — per un data lake, un historian, analisi offline o addestramento di modelli. La scrittura su disco avviene in un thread separato: un disco lento o pieno non rallenta mai l’interrogazione né gli allarmi — nel caso peggiore alcune letture mancano dall’esportazione, e le metriche lo mostrano.

```json
"export": { "enabled": true, "directory": "export", "segment_max_mb": 64 }
```

In Docker basta `EXPORT_ENABLED=true` in `.env`: i file compaiono nel volume dei dati, in `/app/data/export`.

I file si chiamano `telemetry-<ora>-NNNNNN.ipseg`: 24 byte per lettura, un checksum CRC-32C per batch e l’elenco dei sensori dentro ogni file, che quindi si legge da solo, anche dopo una modifica della configurazione. Un file ancora in scrittura termina con `.part`: prelevate solo i `.ipseg` completati. Lo spazio è limitato: quando i file di esportazione superano `EXPORT_MAX_MB` (1024 MB di default; `export.max_total_mb` nella configurazione), i più vecchi vengono cancellati automaticamente, così il disco non si riempie mai. Per conservare tutta la storia, copiate i file altrove prima che scadano, oppure impostate `0` per non cancellare nulla.

```bash
ironpulse-export verify export/                       # verificare il CRC di ogni batch
ironpulse-export dump export/ > readings.csv          # tutte le letture in CSV
ironpulse-export dump --sensor winding_temp export/   # un solo sensore
```

Descrizione del formato: [`docs/export-format.md`](docs/export-format.md); lettore Python senza dipendenze: [`tools/export_reader/read_segment.py`](tools/export_reader/read_segment.py); metriche: `ironpulse_export_*` in `/metrics`.

## Opzioni di deployment

| Scenario | Istruzioni |
|---|---|
| Voglio solo vederlo funzionare in locale | [Avvio rapido](#quick-start) |
| Il mio server/VPS, su cui non gira altro | [Docker Compose su un VPS dedicato](#dedicated-vps) |
| Il server ospita già altri siti con il proprio nginx + HTTPS | [Dietro un nginx esistente](#behind-nginx) |
| Niente Docker, compilo il C++ direttamente | [Compilare dai sorgenti](#build-from-source) |

<a id="dedicated-vps"></a>
### Docker Compose su un VPS dedicato
```bash
cd deploy/docker
cp .env.example .env          # imposta IRONPULSE_API_TOKEN e, se vuoi, le notifiche
docker compose up -d --build
```
La dashboard è su `http://<ip-del-server>:8080` (e sulla porta 80 tramite l’nginx incluso). Per un vero uso pubblico metti davanti un certificato TLS: vedi la sezione successiva.

<a id="behind-nginx"></a>
### Dietro un nginx esistente (e un dominio)
Se il tuo VPS ospita già altri siti con il proprio nginx e certificati TLS (ad es. tramite [certbot](https://certbot.eff.org/)), Ironpulse può aggiungersi come un sito in più invece di contendersi le porte 80/443:
```bash
cd deploy/docker
cp .env.example .env
# Imposta IRONPULSE_BIND_HOST=127.0.0.1 in .env, così ironpulse è
# raggiungibile solo da questa macchina, non direttamente da internet.
docker compose -f docker-compose.yml -f docker-compose.prod.yml up -d --build
```
In questo modo le porte di ironpulse sono legate solo a `127.0.0.1` e l’nginx incluso viene saltato (`docker-compose.prod.yml`): il tuo nginx esistente diventa l’unico punto di terminazione TLS. Poi aggiungi un vhost — [`deploy/nginx/anton-tests.ru.conf`](deploy/nginx/anton-tests.ru.conf) è un esempio pronto; copialo e inserisci il tuo dominio e i percorsi del certificato:
```bash
sudo cp deploy/nginx/anton-tests.ru.conf /etc/nginx/sites-available/your-domain.tld
sudo ln -s /etc/nginx/sites-available/your-domain.tld /etc/nginx/sites-enabled/
sudo nginx -t && sudo systemctl reload nginx
```

### Checklist prima della produzione
- [ ] `IRONPULSE_API_TOKEN` è impostato (lungo e casuale: `openssl rand -hex 32`)
- [ ] `IRONPULSE_BIND_HOST=127.0.0.1` se davanti c’è un reverse proxy (non esporre mai la porta diretta dell’applicazione)
- [ ] Il certificato TLS è valido e si rinnova automaticamente (`certbot renew --dry-run`)
- [ ] `persistence_enabled: true` se lo storico deve sopravvivere ai riavvii
- [ ] `retention_hours` è adeguato al tuo disco: circa 16 byte per lettura; i record vecchi vengono eliminati automaticamente ogni ora
- [ ] `devices` punta ai tuoi dispositivi Modbus **reali**, non al simulatore incluso, e ogni sensore ha i suoi `limits`
- [ ] Notifiche verificate: abbassa temporaneamente un limite sotto il valore attuale e controlla che il messaggio arrivi
- [ ] Spazio su disco sufficiente per la compilazione: Docker richiede temporaneamente ~3-4 GB, mentre l’immagine finale è sotto i 250 MB (`docker builder prune` dopo)
- [ ] Tutti i controlli di stato sono verdi: `docker compose ps` mostra ogni servizio come `healthy`

<a id="build-from-source"></a>
### Compilare dai sorgenti (senza Docker)
Servono un compilatore C++20 (GCC ≥ 12 o Clang ≥ 15), CMake ≥ 3.20, Ninja e OpenSSL ≥ 3 (`libssl-dev`, per le notifiche HTTPS). Tutto il resto (Asio, spdlog, nlohmann/json, cpp-httplib, Catch2) viene scaricato automaticamente.
```bash
cmake --preset debug
cmake --build --preset debug -j$(nproc)
ctest --preset debug --output-on-failure
```
Altri preset: `release`, `asan` (AddressSanitizer + UBSan), `tsan` (ThreadSanitizer).

Per farlo funzionare davvero servono dispositivi Modbus da interrogare; usa il simulatore incluso per una demo autonoma:
```bash
pip install -r tools/modbus_simulator/requirements.txt
python3 tools/modbus_simulator/simulator.py --port 5020 &

./build/debug/ironpulse deploy/config/config.example.json
```
`ironpulse --version` mostra la versione. Vedi [`deploy/systemd/ironpulse.service`](deploy/systemd/ironpulse.service) per un’unità systemd irrobustita; legge i segreti da `/etc/ironpulse/ironpulse.env`.

<a id="troubleshooting"></a>
## Risoluzione dei problemi
**«Port is already allocated» all’avvio di Docker.** Un altro programma usa quella porta. Fermalo o cambia la porta in `.env`.

**Ironpulse si chiude con `Failed to load config ... config: devices[0]...`.** La configurazione contiene un errore; il messaggio indica il campo esatto e il problema.

**La dashboard chiede un token che non ho mai impostato.** Nell’ambiente è impostato un token (`IRONPULSE_API_TOKEN` in `.env` o `api_token` nella configurazione). Inseriscilo, oppure eliminalo se l’accesso deve restare aperto.

**Il dispositivo risulta «offline» e il log riporta `device unreachable: cannot connect`.** Ironpulse non raggiunge `host`/`port`. Con Docker Compose, `host` deve essere il nome del servizio Docker (ad es. `simulator`), non `127.0.0.1`.

**Il log riporta `poll failed: device exception code 2`.** Il dispositivo risponde ma rifiuta i registri richiesti: il codice 2 significa «indirizzo non valido». Confronta `address`, `register_type` e `data_type` con la mappa dei registri del dispositivo (errori tipici: numerazione da zero o da uno, e holding al posto di input).

**I valori sembrano spazzatura (numeri enormi, negativi invece che positivi).** Quasi sempre dipende da `data_type` o `word_order`: prova `word_order: "little"` per i valori a 32 bit, `int16` invece di `uint16` per grandezze che possono essere negative, e controlla `scale`.

**Le notifiche non arrivano.** Controlla il log: `Notification via telegram failed (HTTP 401)` — token del bot errato; `HTTP 400` — `chat_id` errato o bot non presente nella chat; `network error` — il server non ha accesso a internet (i canali Telegram accettano in `url` un proxy o un tuo server Bot API). La metrica `ironpulse_notifications_total{result="error"}` conta le consegne fallite.

**Ho modificato un file ma Docker serve ancora la versione vecchia.** `docker compose build --no-cache` e, se non basta, prima `docker builder prune -af`.

## Architettura (per sviluppatori)
```
Dispositivi Modbus TCP → protocol (interrogazione, decodifica) → EventBus
                                                                  │
           ┌──────────────────────┬──────────────────────┬────────┴──────────┐
           ▼                      ▼                      ▼                   ▼
storage (ring buffer + WAL)  analytics (limiti,     api (REST, WebSocket,  metrics
                             rilevatori, quorum)    /metrics) → dashboard
                                     │
                                     ▼
                             notify (Telegram, Slack, webhook)
```
Ogni livello (`core`, `protocol`, `storage`, `ingest`, `analytics`, `api`, `notify`) è un target CMake indipendente con i propri test. I livelli comunicano tramite l’`EventBus`, non direttamente. Vedi [`docs/architecture.md`](docs/architecture.md) per il modello di concorrenza e [`docs/adr/`](docs/adr/) per le singole decisioni.

### Riferimento API
Contratto completo: [`docs/openapi.yaml`](docs/openapi.yaml) (OpenAPI 3 — aprilo in Swagger UI o importalo in Postman).

| Endpoint | Descrizione |
|---|---|
| `GET /api/v1/devices` | Dispositivi: `id`, `online`, `poll_interval_ms`, elenco dei sensori |
| `GET /api/v1/sensors` | Sensori: `id`, `name`, `unit`, `device_id`, `limits`, ultimo valore |
| `GET /api/v1/series/{sensor_id}?since=<secondi>` | Storico del periodo (predefinito 300 s; con il WAL, l’intero periodo di conservazione). I periodi lunghi vengono ridotti a 10.000 punti |
| `GET /api/v1/series/{sensor_id}?since=<secondi>&format=csv` | Lo stesso in CSV, mai ridotto |
| `GET /api/v1/alerts?limit=<n>` | Allarmi recenti, dal più nuovo (1–1000, predefinito 50): `kind`, `severity`, `value`, `limit`, `direction`, `votes`, `detectors_total`, `confidence` |
| `GET /api/v1/config` | `ws_port`, `ws_host`, `auth_required`, `version` — ciò che serve alla dashboard per avviarsi (nessun token richiesto) |
| `GET /metrics` | Metriche Prometheus |
| `GET /healthz` | Controllo di stato e versione (nessun token richiesto) |
| `WS /live` (`/ws/live` dietro un reverse proxy) | Push in tempo reale: eventi `reading`, `anomaly`, `device_status`. Con token: `?token=<token>` |

### Pratiche di ingegneria
- Avvisi del compilatore rigorosi (`-Wall -Wextra -Wpedantic -Wconversion ...`) e compilazione senza un solo avviso; facoltativamente come errori (`IRONPULSE_WARNINGS_AS_ERRORS`)
- 137 test: unit test di ogni componente, test dell’API HTTP e dell’handshake WebSocket su socket reali, test end-to-end con un falso dispositivo Modbus via TCP (decodifica, limiti, timeout, perdita e ripristino della comunicazione). Tutti verdi con AddressSanitizer/UBSan e ThreadSanitizer
- CI: GCC e Clang, sanitizer, `clang-format`, `ruff` per il simulatore, completezza delle traduzioni, build Docker e uno smoke test con Compose che verifica i dati in tempo reale — [`.github/workflows/ci.yml`](.github/workflows/ci.yml)
- Rilasci tramite tag: immagini compilate nativamente per amd64 e arm64 e pubblicate su GitHub Container Registry — [`.github/workflows/release.yml`](.github/workflows/release.yml)
- Build Docker multi-stage, utente non privilegiato nel container, controlli di stato su ogni servizio
- La dashboard non ha dipendenze esterne a runtime (Chart.js è incluso), quindi funziona in reti ristrette o offline

### Roadmap
- [x] Client Modbus TCP asincrono + codec dei frame
- [x] Archiviazione di serie temporali con persistenza WAL e conservazione
- [x] Rilevamento z-score, EWMA e CUSUM combinato da un `RuleEngine` con voto a quorum
- [x] API REST + WebSocket scritto a mano (RFC 6455) per il push in tempo reale
- [x] Configurazione tramite variabili d’ambiente per i container
- [x] Suite Google Benchmark per i percorsi critici di storage/protocol
- [x] Test di integrazione: dal dispositivo simulato all’allarme pubblicato
- [x] Pulizia automatica e periodica del WAL
- [x] Immagini Docker multi-architettura (linux/amd64 + linux/arm64)
- [x] Più sensori per dispositivo, tipi di dati, scalatura, registri input
- [x] Limiti fissi, gravità degli allarmi, tempo di silenzio
- [x] Notifiche: Telegram, Slack, webhook — in 8 lingue
- [x] Accesso con token, metriche Prometheus, esportazione CSV
- [x] Esportazione di tutte le letture in file con CRC (modulo `ingest`, ex apollonian_core_ingestor)
- [ ] Modbus RTU (RS-485) diretto, senza gateway
- [ ] OPC UA e MQTT come sorgenti dati
- [ ] Presa in carico degli allarmi da parte degli operatori e registro di audit
- [ ] Utenti e ruoli al posto di un unico token

## Contribuire
Issue e pull request sono benvenute. Prima di aprire una PR:
```bash
cmake --preset debug && cmake --build --preset debug -j$(nproc)
ctest --preset debug --output-on-failure
find include src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format --dry-run --Werror
ruff check tools/ && ruff format --check tools/
```
La CI esegue gli stessi controlli (più i sanitizer e la build Docker) su ogni pull request. Le modifiche di ogni versione sono elencate in [CHANGELOG.md](CHANGELOG.md).

## Licenza
MIT — vedi [LICENSE](LICENSE). Libero da usare, modificare e distribuire, anche a fini commerciali.
