# Ironpulse

**Supervision industrielle en temps réel : surveille les capteurs de vos équipements et vous prévient quand quelque chose tourne mal, avant que ça casse.**

[![CI](https://img.shields.io/badge/CI-GitHub_Actions-2088FF?logo=githubactions&logoColor=white)](.github/workflows/ci.yml)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](CMakeLists.txt)
[![Docker](https://img.shields.io/badge/Docker-amd64%20%7C%20arm64-2496ED?logo=docker&logoColor=white)](deploy/docker)
[![Languages](https://img.shields.io/badge/UI-8%20languages-8A2BE2)](#interface-languages)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-131%20passing-brightgreen)](tests)

[Русский](README.md) · [English](README.en.md) · [中文](README.zh.md) · [हिन्दी](README.hi.md) · [Español](README.es.md) · **Français** · [Deutsch](README.de.md) · [Italiano](README.it.md)

## Qu’est-ce que c’est ?
Imaginez une usine avec des dizaines de capteurs : température d’enroulement d’un transformateur, vibration d’un roulement de moteur, pression d’une pompe. Aujourd’hui, quelqu’un doit le remarquer sur un écran — ou pire, entendre un bruit fort — avant de comprendre qu’une pièce surchauffe ou est sur le point de lâcher.

**Ironpulse surveille ces capteurs en continu et vous prévient immédiatement lorsque les mesures sortent de leur plage autorisée ou commencent à se comporter de façon inhabituelle**, avec les mêmes méthodes statistiques que les ingénieurs de maintenance prédictive. Les limites fixes détectent ce qui est déjà dangereux ; la statistique détecte à la fois les pics soudains et la dégradation lente et progressive, bien avant que la valeur n’atteigne sa limite.

Vous obtenez :
- **Un tableau de bord en direct** en 8 langues : la valeur actuelle de chaque capteur dans ses propres unités, un graphique avec les lignes de limite et une liste d’alertes avec leur gravité.
- **Des notifications** vers Telegram, Slack ou tout webhook, dans la langue de votre équipe, y compris les pertes de liaison avec un équipement et leur rétablissement.
- **Un historique sur disque** : un redémarrage ne perd rien, et chaque capteur peut être exporté en CSV sur toute la durée de conservation.
- **Des API et des métriques** pour les autres systèmes : REST, WebSocket et `/metrics` pour Prometheus/Grafana.
- **Un accès protégé par jeton** à l’API, au flux en direct et aux métriques.

Ironpulse dialogue avec de vrais équipements industriels via **Modbus TCP**, le protocole utilisé par une très grande partie des capteurs, automates et compteurs dans le monde. Il lit les registres holding et input, comprend les entiers 16 et 32 bits et les flottants dans n’importe quel ordre des mots, et applique échelle et décalage : il fonctionne avec les tables de registres de vrais équipements, pas seulement avec la démo.

### Pour qui ?
- **Ingénieurs et techniciens** qui ont besoin d’un outil de supervision léger et auto-hébergé, sans payer (ni attendre l’accord de la DSI pour) une plateforme SCADA/IIoT commerciale.
- **Développeurs** qui cherchent une base de code C++20 de niveau production : réseau asynchrone, serveur WebSocket écrit à la main, API REST, déploiement en conteneurs et une vraie suite de tests, sans framework lourd.
- **Étudiants et passionnés** qui veulent voir comment fonctionne de bout en bout une chaîne de supervision industrielle.

Vous n’avez **pas** besoin de connaître le C++ pour le lancer : voir le [Démarrage rapide](#quick-start). Docker compile tout.

## À quoi ça ressemble
À gauche, les équipements et l’état de leur liaison. Au centre, une carte par capteur : nom, grande valeur actuelle dans les unités du capteur (en rouge si elle sort de ses limites), graphique en direct avec lignes de limite en pointillés et bouton d’export CSV. À droite, la liste des alertes : « critique » en rouge pour les dépassements de limite, « avertissement » en orange pour les anomalies statistiques. Tout se met à jour en temps réel sans recharger la page, aussi bien sur l’écran d’une salle de contrôle que sur un téléphone.

<a id="interface-languages"></a>
## Langues de l’interface
Le tableau de bord et les notifications sont disponibles en 8 langues : **russe** (langue principale), anglais, chinois, hindi, espagnol, français, allemand et italien ; le sélecteur est en haut à droite. Cette documentation existe aussi dans ces 8 langues. La langue du tableau de bord est choisie dans cet ordre :
1. le paramètre `?lang=` de l’adresse, par ex. `http://localhost:8080/?lang=fr` (pratique pour partager un lien) ;
2. la langue choisie précédemment dans ce navigateur ;
3. la langue du navigateur, si elle est prise en charge ;
4. sinon, le russe.

La langue des notifications se règle dans la configuration (`notifications.language`) : voir [Notifications](#notifications).

Les traductions du tableau de bord se trouvent dans [`web/js/i18n/locales/`](web/js/i18n/locales) : pour ajouter une langue, copiez `ru.js`, traduisez les valeurs, puis ajoutez le fichier à `web/index.html` et à la liste `supported` de [`web/js/i18n/i18n.js`](web/js/i18n/i18n.js). La CI vérifie que toutes les langues définissent exactement le même ensemble de textes.

<a id="quick-start"></a>
## Démarrage rapide
Il vous faut seulement [Docker](https://docs.docker.com/get-docker/). Il compile tout le reste (le compilateur C++, toutes les bibliothèques) dans un conteneur, et votre machine reste propre.
```bash
git clone https://github.com/Anton-Sergeev-EA/Ironpulse.git
cd Ironpulse/deploy/docker
docker compose up --build
```
Attendez la fin (quelques minutes la première fois : le C++ est compilé depuis les sources), puis ouvrez :
- **http://localhost:8080** : le tableau de bord en direct
- **http://localhost:8080/api/v1/sensors** : l’API brute, par curiosité

C’est tout. Vous surveillez maintenant deux équipements simulés : un transformateur (température d’enroulement et d’huile) et une pompe (vibration du roulement et pression). Le simulateur produit de temps en temps des pics et des écarts prolongés : regardez apparaître avertissements et événements critiques dans le panneau « Alertes actives ».

À chaque version, des images prêtes à l’emploi sont publiées pour amd64 et arm64 (Raspberry Pi, passerelles ARM) :
```bash
docker pull ghcr.io/anton-sergeev-ea/ironpulse:latest
```

### Quelque chose ne marche pas ?
- **Port déjà utilisé ?** Très fréquent si quelque chose tourne déjà sur le 8080 (Jenkins, un autre tableau de bord…). Copiez `.env.example` en `.env` dans `deploy/docker/`, remplacez `HTTP_PORT`/`WS_PORT`/`NGINX_PORT` par des ports libres et relancez `docker compose up --build`. Un fichier, une modification.
- **Toujours bloqué ?** Voir [Dépannage](#troubleshooting).

## Ce qu’on voit à l’écran et ce que signifient les termes

| Terme | Explication simple |
|---|---|
| **Modbus** | Un « langage » vieux de plusieurs décennies mais toujours omniprésent, utilisé par les capteurs et automates industriels pour parler aux logiciels. Ironpulse le parle directement. |
| **Registre** | Une case mémoire de l’équipement qui contient une mesure. La documentation de tout équipement Modbus fournit une table des registres indiquant ce que signifie chaque adresse. |
| **Limite** | Une frontière que la valeur ne doit pas franchir, par ex. une température d’enroulement au-dessus de 90 °C. La franchir déclenche une alerte **critique**. |
| **Anomalie** | Une mesure qui ne correspond pas au comportement normal récent du capteur : un pic soudain, ou une dérive lente qui dure trop longtemps. C’est un **avertissement** : la valeur est encore dans ses limites mais se comporte de façon inhabituelle. |
| **Confiance** | À quel point le système est sûr qu’une mesure signalée est une vraie anomalie et non du bruit. Plus elle est élevée, plus il est sûr. |
| **Z-score, EWMA, CUSUM** | Trois méthodes statistiques de détection, chacune adaptée à un *type* de problème différent (voir [Comment fonctionne la détection d’anomalies](#how-anomaly-detection-works)). Ironpulse en exécute plusieurs à la fois et ne déclenche l’alarme que si suffisamment d’entre elles sont d’accord : moins de fausses alertes. |
| **WAL (write-ahead log)** | Un filet de sécurité : chaque mesure est écrite sur disque dès son arrivée, donc un redémarrage ne perd pas l’historique récent. |
| **API REST / WebSocket** | Deux façons pour d’autres logiciels de récupérer les données d’Ironpulse : REST, « donne-moi l’état actuel » ; WebSocket, « tiens-moi au courant en temps réel ». |

<a id="how-anomaly-detection-works"></a>
## Comment fonctionne la détection d’anomalies
Deux mécanismes indépendants traitent chaque mesure.

**Limites fixes** (`limits`). Une valeur sous `low` ou au-dessus de `high` déclenche une alerte critique. Elle se déclenche une fois quand la valeur sort de la plage autorisée — pas à chaque interrogation tant qu’elle reste dehors — et de nouveau seulement après son retour.

**Statistique.** Un seuil fixe unique soit rate les problèmes lents (la vibration d’un roulement qui augmente sur plusieurs jours), soit déclenche de fausses alertes sur du bruit inoffensif. Chaque capteur peut donc exécuter plusieurs stratégies indépendantes, et l’alarme n’est déclenchée que si suffisamment d’entre elles (`votes_required`) sont d’accord :
- **Z-score** : signale une mesure qui s’écarte de la moyenne récente du capteur d’un nombre inhabituel d’écarts-types. Efficace pour les pics soudains.
- **EWMA** (moyenne mobile à pondération exponentielle) : suit une « normale » auto-ajustée qui réagit aux vrais changements plus vite qu’une fenêtre fixe.
- **CUSUM** (somme cumulée) : accumule les indices d’une dérive *durable* et détecte une dégradation lente qu’une vérification point par point manquerait jusqu’à ce qu’elle devienne grave. Il a besoin de la moyenne (`mean`) et de la dispersion (`stddev`) normales du capteur.

Trois détails supplémentaires gardent la liste d’alertes utile plutôt que bruyante :
- **préchauffage** : juste après le démarrage, les détecteurs se taisent tant qu’ils n’ont pas assez de données (z-score : la moitié de sa fenêtre ; EWMA : ⌈3/α⌉ échantillons) ; sinon chaque redémarrage provoquerait de fausses alertes ;
- **délai de silence** (`cooldown_seconds`) : un écart prolongé produit une notification, pas une par seconde ;
- **quorum** : un pic isolé d’un détecteur sur un capteur bruité ne réveille pas l’ingénieur d’astreinte.

## Configurer vos équipements
Les équipements et leurs capteurs sont décrits dans le fichier de configuration (`deploy/config/config.example.json` sans Docker, `config.docker.json` avec Docker). Un vrai équipement avec trois capteurs :
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
          "name": "Température d’enroulement",
          "unit": "°C",
          "register_type": "holding",
          "address": 0,
          "data_type": "float32",
          "word_order": "big",
          "limits": { "high": 90 }
        },
        {
          "id": "oil_temp",
          "name": "Température d’huile",
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
          "name": "Courant de charge",
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

**Équipement :**

| Champ | Par défaut | Signification |
|---|---|---|
| `id` | — | Nom unique de l’équipement : lettres latines, chiffres, `_`, `-`, `.` |
| `host`, `port` | —, `502` | Où l’équipement (ou la passerelle Modbus TCP) est joignable |
| `unit_id` | `1` | Adresse Modbus de l’esclave/unité (importante derrière une passerelle RS-485) |
| `poll_interval_ms` | `1000` | Période d’interrogation. Elle suit un rythme fixe et ne dérive pas, même si l’équipement répond lentement |
| `timeout_ms` | `3000` | Combien de temps attendre une réponse. Un équipement qui accepte la connexion mais reste muet est signalé comme injoignable |
| `sensors` | un `uint16` dans le registre 0 | Les capteurs de l’équipement |

**Capteur :**

| Champ | Par défaut | Signification |
|---|---|---|
| `id` | — | Nom unique (sur l’ensemble des équipements) ; c’est aussi le nom de son fichier d’historique |
| `name`, `unit` | `id`, vide | Libellé et unités pour le tableau de bord et les notifications |
| `register_type` | `holding` | `holding` (fonction 0x03) ou `input` (0x04) |
| `address` | `0` | Adresse du premier registre (à partir de zéro, comme dans le protocole) |
| `data_type` | `uint16` | `uint16`, `int16`, `uint32`, `int32`, `float32` (les types 32 bits occupent deux registres) |
| `word_order` | `big` | Pour les valeurs 32 bits : `big`, mot de poids fort en premier (ABCD) ; `little`, mot de poids faible en premier (CDAB) |
| `scale`, `offset` | `1`, `0` | valeur = brut × `scale` + `offset` |
| `limits.low`, `limits.high` | aucune | Limites fixes ; les franchir est une alerte critique |
| `detection.detectors` | z-score + EWMA | Stratégies : `{"type": "zscore", "window": 60, "threshold": 3}`, `{"type": "ewma", "alpha": 0.2, "threshold": 3}`, `{"type": "cusum", "mean": …, "stddev": …, "slack": 0.5, "threshold": 5}`. Une liste vide désactive la statistique |
| `detection.votes_required` | `1` | Combien de stratégies doivent être d’accord |
| `detection.cooldown_seconds` | `30` | Délai minimal entre deux alertes du même type pour ce capteur |

Ironpulse regroupe les registres voisins d’un équipement en un minimum de requêtes (jusqu’à 125 registres chacune) : dix capteurs se lisent généralement en une ou deux requêtes, pas dix.

La configuration est validée au démarrage et les erreurs pointent le champ exact, par ex. `config: devices[0].sensors[2].data_type: unknown value 'float64' (expected one of: uint16, int16, uint32, int32, float32)`. Les équipements des anciennes configurations (sans liste `sensors`) continuent de fonctionner comme avant.

Toute valeur texte peut faire référence à une variable d’environnement : `"host": "${PLC_HOST}"`. Les réglages principaux peuvent aussi être surchargés par des variables `IRONPULSE_*` sans modifier le fichier : voir [`docs/adr/0003-env-driven-config.md`](docs/adr/0003-env-driven-config.md).

<a id="notifications"></a>
## Notifications
Ironpulse envoie les alertes et les pertes de liaison vers Telegram, Slack et toute adresse HTTP (webhook). Les textes sont dans la langue choisie et indiquent le capteur, la valeur, la limite et les unités :
```
🔴 CRITIQUE
Température d’enroulement : la valeur 142 °C dépasse la limite 90 °C
Équipement: transformer_01
Heure: 2026-10-02 06:54:09 UTC
Tableau de bord: https://monitoring.example.com
```

Le plus simple est de les activer via `.env` (Docker) ou des variables d’environnement :

| Variable | Signification |
|---|---|
| `TELEGRAM_BOT_TOKEN`, `TELEGRAM_CHAT_ID` | Un bot créé avec [@BotFather](https://t.me/BotFather) et ajouté à votre conversation ou groupe |
| `SLACK_WEBHOOK_URL` | Un Incoming Webhook des réglages de Slack |
| `WEBHOOK_URL` | Toute adresse acceptant un POST JSON |
| `NOTIFY_LANGUAGE` | `ru`, `en`, `zh`, `hi`, `es`, `fr`, `de` ou `it` |
| `IRONPULSE_PUBLIC_URL` | L’adresse du tableau de bord, liée depuis les notifications |

(Sans Docker : `IRONPULSE_TELEGRAM_BOT_TOKEN`, `IRONPULSE_TELEGRAM_CHAT_ID`, `IRONPULSE_SLACK_WEBHOOK_URL`, `IRONPULSE_WEBHOOK_URL`, `IRONPULSE_NOTIFY_LANGUAGE`.)

La même chose dans le fichier de configuration, avec des options supplémentaires :
```json
"notifications": {
  "language": "fr",
  "min_severity": "critical",
  "dashboard_url": "https://monitoring.example.com",
  "channels": [
    { "type": "telegram", "bot_token": "${TG_TOKEN}", "chat_id": "-1001234567890" },
    { "type": "webhook", "url": "https://hooks.example.com/ironpulse",
      "headers": { "Authorization": "Bearer ${HOOK_SECRET}" } }
  ]
}
```
- `min_severity: "critical"` : uniquement les dépassements de limite et les pertes de liaison, sans les avertissements statistiques.
- Un canal Telegram accepte une `url` : votre propre serveur Bot API ou un proxy, si `api.telegram.org` n’est pas joignable depuis le réseau de l’usine.
- Les webhooks reçoivent un JSON structuré : `type` (`alert`, `device_offline`, `device_online`), `severity`, `title`, `text`, `timestamp`, `device_id`, `sensor_id` et un objet `alert` avec `kind`, `value`, `limit`, `direction`, `votes`, etc. ; pratique pour les outils de ticketing et l’automatisation.

L’envoi se fait en arrière-plan et ne ralentit jamais l’interrogation ; les erreurs serveur sont réessayées jusqu’à trois fois avec des pauses croissantes. Un canal sans adresse ou sans jeton est désactivé avec un avertissement dans le journal au lieu d’arrêter le système.

## Sécurité
Par défaut, l’API est ouverte : pratique pour un premier essai, pas pour un serveur en réseau. Définissez un jeton :
```bash
# deploy/docker/.env
IRONPULSE_API_TOKEN=$(openssl rand -hex 32)
```
L’API REST, le flux WebSocket en direct et `/metrics` exigent alors `Authorization: Bearer <jeton>`. Le tableau de bord demande le jeton une seule fois et le mémorise dans le navigateur. `/healthz` reste ouvert pour les contrôles de santé.

Également intégrés : validation des paramètres des requêtes (`400` au lieu d’un plantage), taille limitée des trames et en-têtes WebSocket, déconnexion des clients bloqués, comparaison du jeton en temps constant, identifiants de capteurs sûrs pour le système de fichiers, utilisateur non privilégié dans le conteneur et unité systemd isolée.

## Superviser Ironpulse lui-même
`GET /metrics` expose des métriques Prometheus ; collectez-les avec Prometheus/Grafana ou VictoriaMetrics :

| Métrique | Indique |
|---|---|
| `ironpulse_sensor_value{sensor,unit}` | Dernière valeur de chaque capteur |
| `ironpulse_device_up{device}` | 1 : l’équipement répond ; 0 : il ne répond pas |
| `ironpulse_readings_total{sensor}` | Mesures reçues |
| `ironpulse_alerts_total{sensor,kind,severity}` | Alertes déclenchées |
| `ironpulse_poll_errors_total{device}` | Requêtes échouées vers un équipement |
| `ironpulse_notifications_total{channel,result}` | Envois de notifications |
| `ironpulse_websocket_clients`, `ironpulse_uptime_seconds`, `ironpulse_build_info{version}` | État du service lui-même |

```yaml
# prometheus.yml
scrape_configs:
  - job_name: ironpulse
    authorization: { credentials: "<jeton>" }   # si IRONPULSE_API_TOKEN est défini
    static_configs: [{ targets: ["ironpulse-host:8080"] }]
```

## Export du flux de données
En plus du tableau de bord, Ironpulse peut écrire chaque mesure dans des fichiers binaires compacts — pour un data lake, un historian, de l’analyse hors ligne ou l’entraînement de modèles. L’écriture sur disque se fait dans un thread séparé : un disque lent ou plein ne retarde jamais l’interrogation ni les alertes — au pire certaines mesures manquent à l’export, et les métriques le montrent.

```json
"export": { "enabled": true, "directory": "export", "segment_max_mb": 64 }
```

Sous Docker, il suffit de mettre `EXPORT_ENABLED=true` dans `.env` : les fichiers apparaissent dans le volume de données, sous `/app/data/export`.

Les fichiers s’appellent `telemetry-<heure>-NNNNNN.ipseg` : 24 octets par mesure, une somme CRC-32C par lot et la liste des capteurs dans chaque fichier, qui se lit donc seul, même après une modification de la configuration. Un fichier en cours d’écriture se termine par `.part` : ne récupérez que les `.ipseg` terminés. Ironpulse ne les supprime pas ; la conservation des données exportées vous revient.

```bash
ironpulse-export verify export/                       # vérifier le CRC de chaque lot
ironpulse-export dump export/ > readings.csv          # toutes les mesures en CSV
ironpulse-export dump --sensor winding_temp export/   # un seul capteur
```

Description du format : [`docs/export-format.md`](docs/export-format.md) ; lecteur Python sans dépendance : [`tools/export_reader/read_segment.py`](tools/export_reader/read_segment.py) ; métriques : `ironpulse_export_*` dans `/metrics`.

## Options de déploiement

| Scénario | Instructions |
|---|---|
| Je veux juste le voir tourner en local | [Démarrage rapide](#quick-start) |
| Mon propre serveur/VPS, rien d’autre dessus | [Docker Compose sur un VPS dédié](#dedicated-vps) |
| Le serveur héberge déjà d’autres sites avec leur nginx + HTTPS | [Derrière un nginx existant](#behind-nginx) |
| Pas de Docker, compilation directe du C++ | [Compiler depuis les sources](#build-from-source) |

<a id="dedicated-vps"></a>
### Docker Compose sur un VPS dédié
```bash
cd deploy/docker
cp .env.example .env          # définissez IRONPULSE_API_TOKEN et, si besoin, les notifications
docker compose up -d --build
```
Le tableau de bord est sur `http://<ip-du-serveur>:8080` (et sur le port 80 via le nginx fourni). Pour un vrai usage public, placez un certificat TLS devant : voir la section suivante.

<a id="behind-nginx"></a>
### Derrière un nginx existant (et un domaine)
Si votre VPS héberge déjà d’autres sites avec leur propre nginx et leurs certificats TLS (par ex. via [certbot](https://certbot.eff.org/)), Ironpulse peut s’ajouter comme un site de plus au lieu de se disputer les ports 80/443 :
```bash
cd deploy/docker
cp .env.example .env
# Définissez IRONPULSE_BIND_HOST=127.0.0.1 dans .env pour qu’ironpulse
# ne soit joignable que depuis cette machine, pas directement depuis internet.
docker compose -f docker-compose.yml -f docker-compose.prod.yml up -d --build
```
Les ports d’ironpulse sont alors liés uniquement à `127.0.0.1` et le nginx fourni est ignoré (`docker-compose.prod.yml`) : votre nginx existant devient le seul point de terminaison TLS. Ajoutez ensuite un vhost ; [`deploy/nginx/anton-tests.ru.conf`](deploy/nginx/anton-tests.ru.conf) est un exemple prêt à l’emploi : copiez-le et indiquez votre domaine et les chemins de votre certificat :
```bash
sudo cp deploy/nginx/anton-tests.ru.conf /etc/nginx/sites-available/your-domain.tld
sudo ln -s /etc/nginx/sites-available/your-domain.tld /etc/nginx/sites-enabled/
sudo nginx -t && sudo systemctl reload nginx
```

### Liste de contrôle avant la production
- [ ] `IRONPULSE_API_TOKEN` est défini (long et aléatoire : `openssl rand -hex 32`)
- [ ] `IRONPULSE_BIND_HOST=127.0.0.1` si un proxy inverse est placé devant (n’exposez jamais le port brut de l’application)
- [ ] Le certificat TLS est valide et se renouvelle automatiquement (`certbot renew --dry-run`)
- [ ] `persistence_enabled: true` si l’historique doit survivre aux redémarrages
- [ ] `retention_hours` est adapté à votre disque : environ 16 octets par mesure ; les anciens enregistrements sont supprimés automatiquement toutes les heures
- [ ] `devices` pointe vers vos **vrais** équipements Modbus, pas vers le simulateur fourni, et chaque capteur a ses `limits`
- [ ] Notifications testées : abaissez temporairement une limite sous la valeur actuelle et vérifiez que le message arrive
- [ ] Assez d’espace disque pour la compilation : Docker a besoin temporairement d’environ 3-4 Go, alors que l’image finale fait moins de 250 Mo (`docker builder prune` ensuite)
- [ ] Tous les contrôles de santé sont au vert : `docker compose ps` affiche chaque service comme `healthy`

<a id="build-from-source"></a>
### Compiler depuis les sources (sans Docker)
Il faut un compilateur C++20 (GCC ≥ 12 ou Clang ≥ 15), CMake ≥ 3.20, Ninja et OpenSSL ≥ 3 (`libssl-dev`, pour les notifications HTTPS). Tout le reste (Asio, spdlog, nlohmann/json, cpp-httplib, Catch2) est téléchargé automatiquement.
```bash
cmake --preset debug
cmake --build --preset debug -j$(nproc)
ctest --preset debug --output-on-failure
```
Autres presets : `release`, `asan` (AddressSanitizer + UBSan), `tsan` (ThreadSanitizer).

Pour le faire vraiment tourner, il faut des équipements Modbus à interroger ; utilisez le simulateur fourni pour une démo autonome :
```bash
pip install -r tools/modbus_simulator/requirements.txt
python3 tools/modbus_simulator/simulator.py --port 5020 &

./build/debug/ironpulse deploy/config/config.example.json
```
`ironpulse --version` affiche la version. Voir [`deploy/systemd/ironpulse.service`](deploy/systemd/ironpulse.service) pour une unité systemd durcie ; elle lit les secrets dans `/etc/ironpulse/ironpulse.env`.

<a id="troubleshooting"></a>
## Dépannage
**« Port is already allocated » au démarrage de Docker.** Un autre programme utilise ce port. Arrêtez-le ou changez le port dans `.env`.

**Ironpulse s’arrête avec `Failed to load config ... config: devices[0]...`.** La configuration contient une erreur ; le message indique le champ exact et le problème.

**Le tableau de bord demande un jeton que je n’ai jamais défini.** Un jeton est défini dans l’environnement (`IRONPULSE_API_TOKEN` dans `.env`, ou `api_token` dans la configuration). Saisissez-le, ou supprimez-le si l’accès doit rester ouvert.

**L’équipement apparaît « hors ligne » et le journal indique `device unreachable: cannot connect`.** Ironpulse n’atteint pas `host`/`port`. Avec Docker Compose, `host` doit être le nom du service Docker (par ex. `simulator`), pas `127.0.0.1`.

**Le journal indique `poll failed: device exception code 2`.** L’équipement répond mais refuse les registres demandés : le code 2 signifie « adresse invalide ». Comparez `address`, `register_type` et `data_type` à la table des registres de l’équipement (les erreurs classiques : numérotation à partir de zéro ou de un, et holding contre input).

**Les valeurs ressemblent à n’importe quoi (nombres énormes, négatifs au lieu de positifs).** C’est presque toujours `data_type` ou `word_order` : essayez `word_order: "little"` pour les valeurs 32 bits, `int16` au lieu de `uint16` pour les grandeurs qui peuvent être négatives, et vérifiez `scale`.

**Les notifications n’arrivent pas.** Consultez le journal : `Notification via telegram failed (HTTP 401)` : jeton du bot erroné ; `HTTP 400` : `chat_id` erroné ou bot absent de la conversation ; `network error` : le serveur n’a pas accès à internet (les canaux Telegram acceptent un proxy ou votre propre serveur Bot API dans `url`). La métrique `ironpulse_notifications_total{result="error"}` compte les envois échoués.

**J’ai modifié un fichier mais Docker sert toujours l’ancienne version.** `docker compose build --no-cache` et, si cela ne suffit pas, d’abord `docker builder prune -af`.

## Architecture (pour les développeurs)
```
Équipements Modbus TCP → protocol (interrogation, décodage) → EventBus
                                                                │
           ┌──────────────────────┬──────────────────────┬──────┴────────────┐
           ▼                      ▼                      ▼                   ▼
storage (ring buffer + WAL)  analytics (limites,    api (REST, WebSocket,  metrics
                             détecteurs, quorum)    /metrics) → tableau de bord
                                     │
                                     ▼
                             notify (Telegram, Slack, webhook)
```
Chaque couche (`core`, `protocol`, `storage`, `ingest`, `analytics`, `api`, `notify`) est une cible CMake indépendante avec ses propres tests. Les couches communiquent via l’`EventBus`, jamais directement. Voir [`docs/architecture.md`](docs/architecture.md) pour le modèle de concurrence et [`docs/adr/`](docs/adr/) pour les décisions individuelles.

### Référence de l’API
Contrat complet : [`docs/openapi.yaml`](docs/openapi.yaml) (OpenAPI 3 ; ouvrez-le dans Swagger UI ou importez-le dans Postman).

| Endpoint | Description |
|---|---|
| `GET /api/v1/devices` | Équipements : `id`, `online`, `poll_interval_ms`, liste des capteurs |
| `GET /api/v1/sensors` | Capteurs : `id`, `name`, `unit`, `device_id`, `limits`, dernière valeur |
| `GET /api/v1/series/{sensor_id}?since=<secondes>` | Historique de la période (300 s par défaut ; avec le WAL, toute la durée de conservation). Les longues périodes sont réduites à 10 000 points |
| `GET /api/v1/series/{sensor_id}?since=<secondes>&format=csv` | La même chose en CSV, jamais réduite |
| `GET /api/v1/alerts?limit=<n>` | Alertes récentes, les plus récentes d’abord (1–1000, 50 par défaut) : `kind`, `severity`, `value`, `limit`, `direction`, `votes`, `detectors_total`, `confidence` |
| `GET /api/v1/config` | `ws_port`, `ws_host`, `auth_required`, `version` : ce dont le tableau de bord a besoin pour démarrer (sans jeton) |
| `GET /metrics` | Métriques Prometheus |
| `GET /healthz` | Contrôle de santé et version (sans jeton) |
| `WS /live` (`/ws/live` derrière un proxy inverse) | Envoi en direct : événements `reading`, `anomaly`, `device_status`. Avec jeton : `?token=<jeton>` |

### Pratiques d’ingénierie
- Avertissements stricts du compilateur (`-Wall -Wextra -Wpedantic -Wconversion ...`) et compilation sans aucun avertissement ; en option, traités comme des erreurs (`IRONPULSE_WARNINGS_AS_ERRORS`)
- 131 tests : tests unitaires de chaque composant, tests de l’API HTTP et du handshake WebSocket sur de vrais sockets, tests de bout en bout avec un faux équipement Modbus en TCP (décodage, limites, délais d’attente, perte et rétablissement de la liaison). Tous au vert sous AddressSanitizer/UBSan et ThreadSanitizer
- CI : GCC et Clang, sanitizers, `clang-format`, `ruff` pour le simulateur, complétude des traductions, compilation Docker et test de fumée Compose qui vérifie les données en direct : [`.github/workflows/ci.yml`](.github/workflows/ci.yml)
- Versions déclenchées par étiquette : images compilées nativement pour amd64 et arm64 et publiées dans GitHub Container Registry : [`.github/workflows/release.yml`](.github/workflows/release.yml)
- Compilation Docker multi-étapes, utilisateur non privilégié dans le conteneur, contrôles de santé sur chaque service
- Le tableau de bord n’a aucune dépendance externe à l’exécution (Chart.js est embarqué) : il fonctionne dans des réseaux restreints ou hors ligne

### Feuille de route
- [x] Client Modbus TCP asynchrone + codec de trames
- [x] Stockage de séries temporelles avec persistance WAL et conservation
- [x] Détection z-score, EWMA et CUSUM combinée par un `RuleEngine` à vote par quorum
- [x] API REST + WebSocket écrit à la main (RFC 6455) pour l’envoi en direct
- [x] Configuration par variables d’environnement pour les conteneurs
- [x] Suite Google Benchmark pour les chemins critiques de storage/protocol
- [x] Test d’intégration : d’un équipement simulé jusqu’à une alerte publiée
- [x] Conservation automatique et périodique du WAL
- [x] Images Docker multi-architectures (linux/amd64 + linux/arm64)
- [x] Plusieurs capteurs par équipement, types de données, mise à l’échelle, registres input
- [x] Limites fixes, gravité des alertes, délai de silence
- [x] Notifications : Telegram, Slack, webhook, en 8 langues
- [x] Accès par jeton, métriques Prometheus, export CSV
- [x] Export de toutes les mesures dans des fichiers avec CRC (module `ingest`, ex-apollonian_core_ingestor)
- [ ] Modbus RTU (RS-485) en direct, sans passerelle
- [ ] OPC UA et MQTT comme sources de données
- [ ] Acquittement des alertes par les opérateurs et journal d’audit
- [ ] Utilisateurs et rôles au lieu d’un jeton unique

## Contribuer
Les issues et pull requests sont les bienvenues. Avant d’ouvrir une PR :
```bash
cmake --preset debug && cmake --build --preset debug -j$(nproc)
ctest --preset debug --output-on-failure
find include src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format --dry-run --Werror
ruff check tools/ && ruff format --check tools/
```
La CI exécute les mêmes vérifications (plus les sanitizers et la compilation Docker) sur chaque pull request. Les changements de chaque version sont listés dans [CHANGELOG.md](CHANGELOG.md).

## Licence
MIT : voir [LICENSE](LICENSE). Libre d’utilisation, de modification et de déploiement, y compris commercial.
