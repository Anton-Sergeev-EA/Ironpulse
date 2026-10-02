# Ironpulse

**रीयल-टाइम औद्योगिक निगरानी प्रणाली, जो आपके उपकरणों के सेंसरों की रीडिंग पर नज़र रखती है और कुछ गड़बड़ होने पर — खराबी से पहले — आपको सूचित करती है।**

[![CI](https://img.shields.io/badge/CI-GitHub_Actions-2088FF?logo=githubactions&logoColor=white)](.github/workflows/ci.yml)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](CMakeLists.txt)
[![Docker](https://img.shields.io/badge/Docker-amd64%20%7C%20arm64-2496ED?logo=docker&logoColor=white)](deploy/docker)
[![Languages](https://img.shields.io/badge/UI-8%20languages-8A2BE2)](#interface-languages)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-92%20passing-brightgreen)](tests)

[Русский](README.md) · [English](README.en.md) · [中文](README.zh.md) · **हिन्दी** · [Español](README.es.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md)

## यह क्या है?
दर्जनों सेंसरों वाले एक कारखाने की कल्पना करें: ट्रांसफ़ॉर्मर की वाइंडिंग का तापमान, मोटर बेयरिंग का कंपन, पंप का दबाव। आज किसी व्यक्ति को स्क्रीन पर ध्यान देना पड़ता है — या इससे भी बुरा, कोई तेज़ आवाज़ सुननी पड़ती है — तब जाकर पता चलता है कि कोई पुर्ज़ा ज़्यादा गर्म हो रहा है या टूटने वाला है।

**Ironpulse इन सेंसरों पर लगातार नज़र रखता है और जैसे ही रीडिंग तय सीमा से बाहर जाती है या असामान्य व्यवहार करने लगती है, तुरंत सूचित करता है** — उन्हीं सांख्यिकीय तरीकों से जिनका उपयोग प्रेडिक्टिव मेंटेनेंस इंजीनियर करते हैं। सख़्त सीमाएँ वह पकड़ती हैं जो पहले से ख़तरनाक है; सांख्यिकी अचानक आए उछाल और धीरे-धीरे बढ़ती गिरावट — दोनों को — मान के सीमा तक पहुँचने से बहुत पहले पकड़ लेती है।

आपको मिलता है:
- **8 भाषाओं में लाइव डैशबोर्ड** — हर सेंसर का मौजूदा मान उसकी इकाई में, सीमा-रेखाओं वाला चार्ट और गंभीरता के साथ अलर्ट की सूची।
- **सूचनाएँ** Telegram, Slack या किसी भी webhook पर — आपकी टीम की भाषा में, डिवाइस से संपर्क टूटने और फिर जुड़ने के संदेशों सहित।
- **डिस्क पर इतिहास** — रीस्टार्ट से कोई डेटा नहीं खोता, और किसी भी सेंसर का पूरे संग्रहण-अवधि का डेटा CSV में निर्यात किया जा सकता है।
- **अन्य प्रणालियों के लिए API और मेट्रिक्स**: REST, WebSocket और Prometheus/Grafana के लिए `/metrics`।
- **टोकन-आधारित पहुँच सुरक्षा** — API, लाइव स्ट्रीम और मेट्रिक्स के लिए।

Ironpulse वास्तविक औद्योगिक उपकरणों से **Modbus TCP** के माध्यम से बात करता है — यह प्रोटोकॉल दुनिया भर के बहुत बड़ी संख्या में सेंसर, PLC और मीटर उपयोग करते हैं। यह holding और input रजिस्टर पढ़ता है, किसी भी वर्ड-क्रम में 16- और 32-बिट पूर्णांक और फ़्लोटिंग-पॉइंट संख्याएँ समझता है, और स्केल व ऑफ़सेट लागू करता है — इसलिए यह केवल डेमो नहीं, बल्कि वास्तविक उपकरणों के रजिस्टर मैप के साथ काम करता है।

### यह किसके लिए है?
- **इंजीनियर और तकनीशियन**, जिन्हें व्यावसायिक SCADA/IIoT प्लेटफ़ॉर्म के लिए भुगतान किए बिना (या IT विभाग की मंज़ूरी का इंतज़ार किए बिना) एक हल्का, स्व-होस्टेड निगरानी उपकरण चाहिए।
- **डेवलपर**, जो प्रोडक्शन-स्तर के C++20 कोडबेस का उदाहरण देखना चाहते हैं: एसिंक्रोनस नेटवर्किंग, स्वयं लिखा गया WebSocket सर्वर, REST API, कंटेनर-आधारित डिप्लॉयमेंट और वास्तविक टेस्ट सूट — भारी फ़्रेमवर्क के बिना।
- **छात्र और उत्साही लोग**, जो देखना चाहते हैं कि औद्योगिक निगरानी पाइपलाइन शुरू से अंत तक कैसे काम करती है।

इसे चलाने के लिए C++ जानना **ज़रूरी नहीं** है — नीचे [त्वरित शुरुआत](#quick-start) देखें। Docker सब कुछ बना देता है।

## यह कैसा दिखता है
बाईं ओर डिवाइस और उनके कनेक्शन की स्थिति है। बीच में हर सेंसर का एक कार्ड है: नाम, सेंसर की इकाई में बड़ा मौजूदा मान (सीमा से बाहर होने पर लाल), डैश वाली सीमा-रेखाओं वाला लाइव चार्ट और CSV निर्यात बटन। दाईं ओर अलर्ट की सूची है: सीमा उल्लंघन के लिए लाल "गंभीर", सांख्यिकीय विसंगतियों के लिए पीला "चेतावनी"। सब कुछ बिना पेज रीलोड किए रीयल-टाइम में अपडेट होता है, और कंट्रोल-रूम की बड़ी स्क्रीन पर भी उतना ही सुविधाजनक है जितना फ़ोन पर।

<a id="interface-languages"></a>
## इंटरफ़ेस की भाषाएँ
डैशबोर्ड और सूचनाएँ 8 भाषाओं में उपलब्ध हैं: **रूसी** (मुख्य), अंग्रेज़ी, चीनी, हिन्दी, स्पेनिश, फ़्रेंच, जर्मन और इतालवी — भाषा बदलने का विकल्प ऊपर दाएँ कोने में है। यह दस्तावेज़ भी इन्हीं 8 भाषाओं में उपलब्ध है। डैशबोर्ड की भाषा इस क्रम में चुनी जाती है:
1. पते में `?lang=` पैरामीटर, जैसे `http://localhost:8080/?lang=hi` (सहकर्मी को लिंक भेजने के लिए सुविधाजनक);
2. इस ब्राउज़र में पहले चुनी गई भाषा;
3. ब्राउज़र की भाषा, यदि समर्थित हो;
4. अन्यथा रूसी।

सूचनाओं की भाषा कॉन्फ़िगरेशन में सेट होती है (`notifications.language`) — [सूचनाएँ](#notifications) देखें।

डैशबोर्ड के अनुवाद [`web/js/i18n/locales/`](web/js/i18n/locales) में हैं: नई भाषा जोड़ने के लिए `ru.js` की प्रति बनाएँ, मानों का अनुवाद करें, और फ़ाइल को `web/index.html` तथा [`web/js/i18n/i18n.js`](web/js/i18n/i18n.js) की `supported` सूची में जोड़ें। CI जाँचता है कि सभी भाषाओं में स्ट्रिंग्स का समूह बिल्कुल एक जैसा हो।

<a id="quick-start"></a>
## त्वरित शुरुआत
आपको केवल [Docker](https://docs.docker.com/get-docker/) चाहिए। बाकी सब कुछ (C++ कंपाइलर, सभी लाइब्रेरी) वह कंटेनर के अंदर बना देता है, ताकि आपका कंप्यूटर साफ़ रहे।
```bash
git clone https://github.com/Anton-Sergeev-EA/Ironpulse.git
cd Ironpulse/deploy/docker
docker compose up --build
```
पूरा होने तक प्रतीक्षा करें (पहली बार कुछ मिनट — C++ सोर्स से कंपाइल होता है), फिर खोलें:
- **http://localhost:8080** — लाइव डैशबोर्ड
- **http://localhost:8080/api/v1/sensors** — कच्चा API, यदि रुचि हो

बस इतना ही। अब आप दो सिम्युलेटेड डिवाइसों पर नज़र रख रहे हैं — एक ट्रांसफ़ॉर्मर (वाइंडिंग और तेल का तापमान) और एक पंप (बेयरिंग का कंपन और दबाव)। सिम्युलेटर समय-समय पर उछाल और लंबे विचलन पैदा करता है — "सक्रिय अलर्ट" पैनल में चेतावनियाँ और गंभीर घटनाएँ आती देखें।

हर रिलीज़ के साथ amd64 और arm64 (Raspberry Pi, ARM गेटवे) के लिए तैयार इमेज प्रकाशित होती हैं:
```bash
docker pull ghcr.io/anton-sergeev-ea/ironpulse:latest
```

### कुछ काम नहीं कर रहा?
- **पोर्ट पहले से उपयोग में है?** यदि 8080 पर पहले से कुछ चल रहा है (Jenkins, कोई और डैशबोर्ड आदि), तो यह बहुत आम है। `deploy/docker/` में `.env.example` को `.env` में कॉपी करें, `HTTP_PORT`/`WS_PORT`/`NGINX_PORT` को खाली पोर्ट पर बदलें और `docker compose up --build` फिर से चलाएँ। एक फ़ाइल, एक बदलाव।
- **अब भी नहीं चल रहा?** [समस्या समाधान](#troubleshooting) देखें।

## स्क्रीन पर क्या है और शब्दों का अर्थ

| शब्द | सरल व्याख्या |
|---|---|
| **Modbus** | दशकों पुरानी, पर आज भी हर जगह इस्तेमाल होने वाली "भाषा", जिससे औद्योगिक सेंसर और कंट्रोलर सॉफ़्टवेयर से बात करते हैं। Ironpulse इसे सीधे बोलता है। |
| **रजिस्टर** | डिवाइस की मेमोरी की वह कोठरी जिसमें रीडिंग रहती है। हर Modbus डिवाइस के दस्तावेज़ में रजिस्टर मैप होता है, जो बताता है कि कौन-सा पता क्या दर्शाता है। |
| **सीमा (limit)** | वह सीमा जिसे मान पार नहीं करना चाहिए, जैसे वाइंडिंग का तापमान 90 °C से ऊपर। सीमा पार करना **गंभीर** अलर्ट है। |
| **विसंगति** | ऐसी रीडिंग जो सेंसर के हाल के सामान्य पैटर्न में फ़िट नहीं होती — अचानक उछाल, या बहुत लंबे समय तक चला धीमा बहाव। यह **चेतावनी** है: मान अभी सीमा के भीतर है, पर व्यवहार असामान्य है। |
| **विश्वसनीयता (confidence)** | प्रणाली कितनी आश्वस्त है कि चिह्नित रीडिंग वास्तव में विसंगति है, केवल शोर नहीं। जितना अधिक, उतना निश्चित। |
| **Z-score, EWMA, CUSUM** | तीन सांख्यिकीय पहचान विधियाँ, हर एक अलग *प्रकार* की समस्या में अच्छी ([विसंगति पहचान कैसे काम करती है](#how-anomaly-detection-works) देखें)। Ironpulse एक साथ कई चलाता है और तभी अलार्म देता है जब पर्याप्त विधियाँ सहमत हों — कम झूठे अलार्म। |
| **WAL (write-ahead log)** | सुरक्षा कवच: हर रीडिंग आते ही डिस्क पर लिखी जाती है, इसलिए रीस्टार्ट से हाल का इतिहास नहीं खोता। |
| **REST API / WebSocket** | अन्य सॉफ़्टवेयर के लिए Ironpulse से डेटा लेने के दो तरीके: REST — "मुझे मौजूदा स्थिति दो", WebSocket — "मुझे रीयल-टाइम में बताते रहो"। |

<a id="how-anomaly-detection-works"></a>
## विसंगति पहचान कैसे काम करती है
हर रीडिंग पर दो स्वतंत्र तंत्र चलते हैं।

**सख़्त सीमाएँ** (`limits`)। `low` से नीचे या `high` से ऊपर का मान गंभीर अलर्ट देता है। यह एक बार तब चलता है जब मान अनुमत दायरे से बाहर जाता है — बाहर रहने के दौरान हर पोलिंग पर नहीं — और दोबारा तभी, जब मान वापस सामान्य हो चुका हो।

**सांख्यिकी।** एक निश्चित सीमा या तो धीमी समस्याएँ चूक जाती है (कई दिनों में बढ़ता बेयरिंग कंपन) या हानिरहित शोर पर झूठा अलार्म देती है। इसलिए हर सेंसर पर कई स्वतंत्र रणनीतियाँ चलाई जा सकती हैं, और अलार्म तभी उठता है जब पर्याप्त रणनीतियाँ (`votes_required`) सहमत हों:
- **Z-score** — ऐसी रीडिंग चिह्नित करता है जो सेंसर के हाल के औसत से असामान्य संख्या में मानक विचलन दूर हो। अचानक उछाल पकड़ने में अच्छा।
- **EWMA** (घातीय भारित चल औसत) — स्वयं समायोजित होने वाले "सामान्य" को ट्रैक करता है, जो वास्तविक बदलावों पर स्थिर विंडो से तेज़ प्रतिक्रिया देता है।
- **CUSUM** (संचयी योग) — *लगातार* बहाव के साक्ष्य जमा करता है और ऐसी धीमी गिरावट पकड़ता है जिसे एक-बिंदु जाँच गंभीर होने तक चूक जाती। इसे सेंसर का सामान्य औसत (`mean`) और फैलाव (`stddev`) चाहिए।

तीन और बातें अलर्ट सूची को शोरभरा नहीं, सार्थक रखती हैं:
- **वार्म-अप** — शुरू होने के बाद डिटेक्टर तब तक चुप रहते हैं जब तक पर्याप्त डेटा न जमा हो जाए (z-score: आधी विंडो, EWMA: ⌈3/α⌉ नमूने); वरना हर रीस्टार्ट झूठे अलार्म देता;
- **कूलडाउन** (`cooldown_seconds`) — लंबा विचलन एक सूचना देता है, हर सेकंड एक नहीं;
- **कोरम** — शोरभरे सेंसर पर किसी एक डिटेक्टर की एकल झलक ड्यूटी इंजीनियर को नहीं जगाती।

## अपने डिवाइस कॉन्फ़िगर करना
डिवाइस और उनके सेंसर कॉन्फ़िग फ़ाइल में बताए जाते हैं (Docker के बिना `deploy/config/config.example.json`, Docker के साथ `config.docker.json`)। तीन सेंसर वाला एक वास्तविक डिवाइस:
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
          "name": "वाइंडिंग का तापमान",
          "unit": "°C",
          "register_type": "holding",
          "address": 0,
          "data_type": "float32",
          "word_order": "big",
          "limits": { "high": 90 }
        },
        {
          "id": "oil_temp",
          "name": "तेल का तापमान",
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
          "name": "लोड करंट",
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

**डिवाइस:**

| फ़ील्ड | डिफ़ॉल्ट | अर्थ |
|---|---|---|
| `id` | — | डिवाइस का अद्वितीय नाम: लैटिन अक्षर, अंक, `_`, `-`, `.` |
| `host`, `port` | —, `502` | डिवाइस (या Modbus TCP गेटवे) कहाँ उपलब्ध है |
| `unit_id` | `1` | Modbus slave/unit पता (RS-485 गेटवे के पीछे महत्वपूर्ण) |
| `poll_interval_ms` | `1000` | पोलिंग की अवधि। पोलिंग स्थिर दर से चलती है और डिवाइस धीमा हो तब भी नहीं खिसकती |
| `timeout_ms` | `3000` | उत्तर की प्रतीक्षा कितनी देर। जो डिवाइस कनेक्शन स्वीकार कर चुप रहे, उसे अनुपलब्ध बताया जाता है |
| `sensors` | रजिस्टर 0 में एक `uint16` | डिवाइस के सेंसरों की सूची |

**सेंसर:**

| फ़ील्ड | डिफ़ॉल्ट | अर्थ |
|---|---|---|
| `id` | — | (सभी डिवाइसों में) अद्वितीय सेंसर नाम; इसकी इतिहास-फ़ाइल का नाम भी |
| `name`, `unit` | `id`, खाली | डैशबोर्ड और सूचनाओं के लिए नाम और इकाई |
| `register_type` | `holding` | `holding` (फ़ंक्शन 0x03) या `input` (0x04) |
| `address` | `0` | पहले रजिस्टर का पता (शून्य से, जैसा प्रोटोकॉल में) |
| `data_type` | `uint16` | `uint16`, `int16`, `uint32`, `int32`, `float32` (32-बिट प्रकार दो रजिस्टर लेते हैं) |
| `word_order` | `big` | 32-बिट मानों के लिए: `big` — उच्च वर्ड पहले (ABCD), `little` — निम्न वर्ड पहले (CDAB) |
| `scale`, `offset` | `1`, `0` | मान = कच्चा मान × `scale` + `offset` |
| `limits.low`, `limits.high` | नहीं | सख़्त सीमाएँ; पार करना गंभीर अलर्ट है |
| `detection.detectors` | z-score + EWMA | रणनीतियाँ: `{"type": "zscore", "window": 60, "threshold": 3}`, `{"type": "ewma", "alpha": 0.2, "threshold": 3}`, `{"type": "cusum", "mean": …, "stddev": …, "slack": 0.5, "threshold": 5}`। खाली सूची सांख्यिकी बंद कर देती है |
| `detection.votes_required` | `1` | कितनी रणनीतियों को सहमत होना चाहिए |
| `detection.cooldown_seconds` | `30` | एक सेंसर के एक ही प्रकार के दो अलर्ट के बीच न्यूनतम अंतराल |

Ironpulse एक डिवाइस के पड़ोसी रजिस्टरों को यथासंभव कम अनुरोधों में मिला देता है (हर अनुरोध में 125 रजिस्टर तक) — दस सेंसर आमतौर पर दस नहीं, बल्कि एक-दो अनुरोधों में पढ़े जाते हैं।

कॉन्फ़िग की जाँच स्टार्टअप पर होती है, और त्रुटियाँ ठीक उसी फ़ील्ड की ओर इशारा करती हैं, जैसे `config: devices[0].sensors[2].data_type: unknown value 'float64' (expected one of: uint16, int16, uint32, int32, float32)`। पुराने कॉन्फ़िग वाले डिवाइस (बिना `sensors` सूची के) पहले की तरह काम करते रहते हैं।

कोई भी स्ट्रिंग मान पर्यावरण चर का संदर्भ दे सकता है: `"host": "${PLC_HOST}"`। मुख्य सेटिंग्स को फ़ाइल बदले बिना `IRONPULSE_*` चरों से भी बदला जा सकता है — [`docs/adr/0003-env-driven-config.md`](docs/adr/0003-env-driven-config.md) देखें।

<a id="notifications"></a>
## सूचनाएँ
Ironpulse अलर्ट और डिवाइस से संपर्क टूटने की सूचनाएँ Telegram, Slack और किसी भी HTTP पते (webhook) पर भेजता है। पाठ चुनी गई भाषा में होता है, जिसमें सेंसर का नाम, मान, सीमा और इकाई होती है:
```
🔴 गंभीर
वाइंडिंग का तापमान: मान 142 °C सीमा 90 °C से ऊपर है
डिवाइस: transformer_01
समय: 2026-10-02 06:54:09 UTC
डैशबोर्ड: https://monitoring.example.com
```

सबसे आसान तरीका `.env` (Docker) या पर्यावरण चरों से चालू करना है:

| चर | अर्थ |
|---|---|
| `TELEGRAM_BOT_TOKEN`, `TELEGRAM_CHAT_ID` | [@BotFather](https://t.me/BotFather) से बनाया गया बॉट, जो आपकी चैट या समूह में जोड़ा गया हो |
| `SLACK_WEBHOOK_URL` | Slack सेटिंग्स से Incoming Webhook |
| `WEBHOOK_URL` | JSON POST स्वीकार करने वाला कोई भी पता |
| `NOTIFY_LANGUAGE` | `ru`, `en`, `zh`, `hi`, `es`, `fr`, `de` या `it` |
| `IRONPULSE_PUBLIC_URL` | डैशबोर्ड का पता — सूचनाओं में इसका लिंक दिखेगा |

(Docker के बिना: `IRONPULSE_TELEGRAM_BOT_TOKEN`, `IRONPULSE_TELEGRAM_CHAT_ID`, `IRONPULSE_SLACK_WEBHOOK_URL`, `IRONPULSE_WEBHOOK_URL`, `IRONPULSE_NOTIFY_LANGUAGE`।)

यही कॉन्फ़िग फ़ाइल में, अतिरिक्त विकल्पों के साथ:
```json
"notifications": {
  "language": "hi",
  "min_severity": "critical",
  "dashboard_url": "https://monitoring.example.com",
  "channels": [
    { "type": "telegram", "bot_token": "${TG_TOKEN}", "chat_id": "-1001234567890" },
    { "type": "webhook", "url": "https://hooks.example.com/ironpulse",
      "headers": { "Authorization": "Bearer ${HOOK_SECRET}" } }
  ]
}
```
- `min_severity: "critical"` — केवल सीमा उल्लंघन और संपर्क टूटना, सांख्यिकीय चेतावनियाँ नहीं।
- Telegram चैनल में `url` दिया जा सकता है: यदि कारखाने के नेटवर्क से `api.telegram.org` उपलब्ध नहीं है तो अपना Bot API सर्वर या प्रॉक्सी।
- Webhook को संरचित JSON मिलता है: `type` (`alert`, `device_offline`, `device_online`), `severity`, `title`, `text`, `timestamp`, `device_id`, `sensor_id` और `kind`, `value`, `limit`, `direction`, `votes` आदि फ़ील्ड वाला `alert` ऑब्जेक्ट — टिकटिंग और ऑटोमेशन प्रणालियों के लिए सुविधाजनक।

भेजना पृष्ठभूमि में होता है और पोलिंग को कभी धीमा नहीं करता; सर्वर त्रुटियों पर बढ़ते अंतराल के साथ तीन बार तक पुनः प्रयास होता है। जिस चैनल का पता या टोकन नहीं दिया गया, वह प्रणाली को रोकने के बजाय लॉग में चेतावनी के साथ बंद कर दिया जाता है।

## सुरक्षा
डिफ़ॉल्ट रूप से API खुला है — पहली बार देखने के लिए सुविधाजनक, पर नेटवर्क से जुड़े सर्वर के लिए नहीं। टोकन सेट करें:
```bash
# deploy/docker/.env
IRONPULSE_API_TOKEN=$(openssl rand -hex 32)
```
इसके बाद REST API, लाइव WebSocket स्ट्रीम और `/metrics` के लिए `Authorization: Bearer <टोकन>` आवश्यक है। डैशबोर्ड एक बार टोकन पूछता है और उसे ब्राउज़र में याद रखता है। `/healthz` हेल्थ-चेक के लिए खुला रहता है।

साथ ही पहले से शामिल: अनुरोध पैरामीटरों की जाँच (क्रैश के बजाय `400`), WebSocket फ़्रेम और हेडर के आकार की सीमा, अटके हुए क्लाइंटों को डिस्कनेक्ट करना, स्थिर-समय में टोकन तुलना, फ़ाइल-सिस्टम के लिए सुरक्षित सेंसर ID, कंटेनर में बिना विशेषाधिकार वाला उपयोगकर्ता और सैंडबॉक्स किया गया systemd यूनिट।

## Ironpulse की स्वयं की निगरानी
`GET /metrics` Prometheus प्रारूप में मेट्रिक्स देता है — इन्हें Prometheus/Grafana या VictoriaMetrics से संग्रहित करें:

| मेट्रिक | क्या दिखाता है |
|---|---|
| `ironpulse_sensor_value{sensor,unit}` | हर सेंसर का अंतिम मान |
| `ironpulse_device_up{device}` | 1 — डिवाइस उत्तर देता है, 0 — नहीं |
| `ironpulse_readings_total{sensor}` | प्राप्त रीडिंग की संख्या |
| `ironpulse_alerts_total{sensor,kind,severity}` | उठाए गए अलर्ट |
| `ironpulse_poll_errors_total{device}` | डिवाइस से असफल अनुरोध |
| `ironpulse_notifications_total{channel,result}` | सूचनाओं की डिलीवरी |
| `ironpulse_websocket_clients`, `ironpulse_uptime_seconds`, `ironpulse_build_info{version}` | सेवा की अपनी स्थिति |

```yaml
# prometheus.yml
scrape_configs:
  - job_name: ironpulse
    authorization: { credentials: "<टोकन>" }   # यदि IRONPULSE_API_TOKEN सेट है
    static_configs: [{ targets: ["ironpulse-host:8080"] }]
```

## डिप्लॉयमेंट के विकल्प

| स्थिति | निर्देश |
|---|---|
| बस स्थानीय रूप से देखना है कि यह कैसे काम करता है | [त्वरित शुरुआत](#quick-start) |
| अपना सर्वर/VPS, जिस पर और कुछ नहीं चलता | [अलग VPS पर Docker Compose](#dedicated-vps) |
| सर्वर पर पहले से अन्य साइटें अपने nginx + HTTPS के साथ हैं | [मौजूदा nginx के पीछे](#behind-nginx) |
| Docker नहीं चल सकता, C++ सीधे बनाना है | [सोर्स से बनाना](#build-from-source) |

<a id="dedicated-vps"></a>
### अलग VPS पर Docker Compose
```bash
cd deploy/docker
cp .env.example .env          # IRONPULSE_API_TOKEN और, चाहें तो, सूचनाएँ सेट करें
docker compose up -d --build
```
डैशबोर्ड `http://<सर्वर-ip>:8080` पर उपलब्ध है (और साथ आए nginx के माध्यम से पोर्ट 80 पर)। वास्तविक सार्वजनिक उपयोग के लिए आगे TLS प्रमाणपत्र लगाएँ — अगला भाग देखें।

<a id="behind-nginx"></a>
### मौजूदा nginx (और डोमेन) के पीछे
यदि आपके VPS पर पहले से अन्य साइटें अपने nginx और TLS प्रमाणपत्रों के साथ हैं (जैसे [certbot](https://certbot.eff.org/) से), तो Ironpulse पोर्ट 80/443 के लिए लड़ने के बजाय एक और साइट के रूप में जुड़ सकता है:
```bash
cd deploy/docker
cp .env.example .env
# .env में IRONPULSE_BIND_HOST=127.0.0.1 सेट करें, ताकि ironpulse
# केवल इसी मशीन से उपलब्ध हो, सीधे इंटरनेट से नहीं।
docker compose -f docker-compose.yml -f docker-compose.prod.yml up -d --build
```
यह ironpulse के पोर्ट केवल `127.0.0.1` से बाँधता है और साथ आए nginx को छोड़ देता है (`docker-compose.prod.yml`), ताकि आपका मौजूदा nginx TLS का एकमात्र समापन-बिंदु बने। फिर vhost जोड़ें — [`deploy/nginx/anton-tests.ru.conf`](deploy/nginx/anton-tests.ru.conf) तैयार उदाहरण है; इसे कॉपी करें और अपना डोमेन व प्रमाणपत्र के पथ डालें:
```bash
sudo cp deploy/nginx/anton-tests.ru.conf /etc/nginx/sites-available/your-domain.tld
sudo ln -s /etc/nginx/sites-available/your-domain.tld /etc/nginx/sites-enabled/
sudo nginx -t && sudo systemctl reload nginx
```

### प्रोडक्शन से पहले की जाँच-सूची
- [ ] `IRONPULSE_API_TOKEN` सेट है (लंबा और यादृच्छिक: `openssl rand -hex 32`)
- [ ] यदि आगे रिवर्स प्रॉक्सी है तो `IRONPULSE_BIND_HOST=127.0.0.1` (ऐप का कच्चा पोर्ट कभी इंटरनेट पर न खोलें)
- [ ] TLS प्रमाणपत्र मान्य है और स्वतः नवीनीकृत होता है (`certbot renew --dry-run`)
- [ ] यदि रीस्टार्ट के बाद इतिहास बचना चाहिए तो `persistence_enabled: true`
- [ ] `retention_hours` आपकी डिस्क के अनुरूप है — प्रति रीडिंग लगभग 16 बाइट; पुराने रिकॉर्ड हर घंटे स्वतः हटाए जाते हैं
- [ ] `devices` आपके **वास्तविक** Modbus डिवाइसों की ओर इशारा करते हैं, साथ आए सिम्युलेटर की ओर नहीं, और हर सेंसर की `limits` तय हैं
- [ ] सूचनाएँ जाँची गई हैं: अस्थायी रूप से किसी सीमा को मौजूदा मान से नीचे रखें और देखें कि संदेश आता है
- [ ] बिल्ड के लिए पर्याप्त डिस्क स्थान — Docker को अस्थायी रूप से ~3-4 GB चाहिए, जबकि अंतिम इमेज 250 MB से कम है (बाद में `docker builder prune`)
- [ ] सभी हेल्थ-चेक हरे हैं: `docker compose ps` हर सेवा को `healthy` दिखाता है

<a id="build-from-source"></a>
### सोर्स से बनाना (Docker के बिना)
C++20 कंपाइलर (GCC ≥ 12 या Clang ≥ 15), CMake ≥ 3.20, Ninja और OpenSSL ≥ 3 (`libssl-dev`, HTTPS सूचनाओं के लिए) चाहिए। बाकी सब (Asio, spdlog, nlohmann/json, cpp-httplib, Catch2) स्वतः डाउनलोड होता है।
```bash
cmake --preset debug
cmake --build --preset debug -j$(nproc)
ctest --preset debug --output-on-failure
```
अन्य प्रीसेट: `release`, `asan` (AddressSanitizer + UBSan), `tsan` (ThreadSanitizer)।

वास्तव में चलाने के लिए पोल करने योग्य Modbus डिवाइस चाहिए — स्वतंत्र डेमो के लिए साथ आए सिम्युलेटर का उपयोग करें:
```bash
pip install -r tools/modbus_simulator/requirements.txt
python3 tools/modbus_simulator/simulator.py --port 5020 &

./build/debug/ironpulse deploy/config/config.example.json
```
`ironpulse --version` संस्करण दिखाता है। सुरक्षित systemd यूनिट के लिए [`deploy/systemd/ironpulse.service`](deploy/systemd/ironpulse.service) देखें; यह गोपनीय जानकारी `/etc/ironpulse/ironpulse.env` से पढ़ता है।

<a id="troubleshooting"></a>
## समस्या समाधान
**Docker शुरू करते समय "Port is already allocated"।** कोई और प्रोग्राम वह पोर्ट उपयोग कर रहा है। उसे रोकें या `.env` में पोर्ट बदलें।

**Ironpulse `Failed to load config ... config: devices[0]...` के साथ बंद हो जाता है।** कॉन्फ़िग में त्रुटि है; संदेश ठीक-ठीक फ़ील्ड और समस्या बताता है।

**डैशबोर्ड ऐसा टोकन माँगता है जो मैंने कभी सेट नहीं किया।** टोकन पर्यावरण में सेट है (`.env` में `IRONPULSE_API_TOKEN` या कॉन्फ़िग में `api_token`)। उसे दर्ज करें, या यदि पहुँच खुली होनी चाहिए तो उसे हटा दें।

**डिवाइस "ऑफ़लाइन" दिखता है और लॉग में `device unreachable: cannot connect` है।** Ironpulse `host`/`port` तक नहीं पहुँच पा रहा। Docker Compose के साथ `host` Docker सेवा का नाम होना चाहिए (जैसे `simulator`), `127.0.0.1` नहीं।

**लॉग में `poll failed: device exception code 2` है।** डिवाइस उत्तर देता है पर माँगे गए रजिस्टर देने से मना करता है: कोड 2 का अर्थ "अमान्य पता" है। डिवाइस के रजिस्टर मैप से `address`, `register_type` और `data_type` मिलाएँ (आम गलतियाँ: शून्य बनाम एक से गिनती, और holding बनाम input)।

**मान कचरे जैसे दिखते हैं (बहुत बड़ी संख्याएँ, धनात्मक के बजाय ऋणात्मक)।** लगभग हमेशा `data_type` या `word_order` की समस्या: 32-बिट मानों के लिए `word_order: "little"` आज़माएँ, ऋणात्मक हो सकने वाली राशियों के लिए `uint16` की जगह `int16`, और `scale` जाँचें।

**सूचनाएँ नहीं आतीं।** लॉग देखें: `Notification via telegram failed (HTTP 401)` — बॉट टोकन गलत; `HTTP 400` — `chat_id` गलत या बॉट चैट में नहीं; `network error` — सर्वर को इंटरनेट उपलब्ध नहीं (Telegram चैनल `url` में प्रॉक्सी या अपना Bot API सर्वर स्वीकार करता है)। मेट्रिक `ironpulse_notifications_total{result="error"}` असफल डिलीवरी गिनता है।

**फ़ाइल बदली, पर Docker अब भी पुराना संस्करण देता है।** `docker compose build --no-cache`, और यदि पर्याप्त न हो तो पहले `docker builder prune -af`।

## आर्किटेक्चर (डेवलपर्स के लिए)
```
Modbus TCP डिवाइस → protocol (पोलिंग, रजिस्टर डिकोडिंग) → EventBus
                                                             │
           ┌──────────────────────┬──────────────────────┬───┴───────────────┐
           ▼                      ▼                      ▼                   ▼
storage (ring buffer + WAL)  analytics (सीमाएँ,      api (REST, WebSocket,  metrics
                             डिटेक्टर, कोरम)        /metrics) → डैशबोर्ड
                                     │
                                     ▼
                             notify (Telegram, Slack, webhook)
```
हर परत (`core`, `protocol`, `storage`, `analytics`, `api`, `notify`) अपने टेस्ट के साथ एक स्वतंत्र CMake लक्ष्य है। परतें सीधे नहीं, बल्कि `EventBus` के माध्यम से बात करती हैं। समवर्तिता मॉडल के लिए [`docs/architecture.md`](docs/architecture.md) और अलग-अलग निर्णयों के लिए [`docs/adr/`](docs/adr/) देखें।

### API संदर्भ
पूरा विवरण: [`docs/openapi.yaml`](docs/openapi.yaml) (OpenAPI 3 — Swagger UI में खोलें या Postman में आयात करें)।

| एंडपॉइंट | विवरण |
|---|---|
| `GET /api/v1/devices` | डिवाइस: `id`, `online`, `poll_interval_ms`, सेंसर सूची |
| `GET /api/v1/sensors` | सेंसर: `id`, `name`, `unit`, `device_id`, `limits`, अंतिम मान |
| `GET /api/v1/series/{sensor_id}?since=<सेकंड>` | दी गई अवधि का इतिहास (डिफ़ॉल्ट 300 सेकंड; WAL के साथ पूरी संग्रहण-अवधि)। लंबी अवधियाँ 10,000 बिंदुओं तक घटाई जाती हैं |
| `GET /api/v1/series/{sensor_id}?since=<सेकंड>&format=csv` | वही CSV में, कभी घटाया नहीं जाता |
| `GET /api/v1/alerts?limit=<n>` | हाल के अलर्ट, नए पहले (1–1000, डिफ़ॉल्ट 50): `kind`, `severity`, `value`, `limit`, `direction`, `votes`, `detectors_total`, `confidence` |
| `GET /api/v1/config` | `ws_port`, `ws_host`, `auth_required`, `version` — डैशबोर्ड शुरू करने के लिए आवश्यक जानकारी (टोकन नहीं चाहिए) |
| `GET /metrics` | Prometheus मेट्रिक्स |
| `GET /healthz` | जीवंतता जाँच और संस्करण (टोकन नहीं चाहिए) |
| `WS /live` (रिवर्स प्रॉक्सी के पीछे `/ws/live`) | लाइव पुश: `reading`, `anomaly`, `device_status` घटनाएँ। टोकन के साथ: `?token=<टोकन>` |

### इंजीनियरिंग प्रथाएँ
- सख़्त कंपाइलर चेतावनियाँ (`-Wall -Wextra -Wpedantic -Wconversion ...`), बिना एक भी चेतावनी का बिल्ड; वैकल्पिक रूप से त्रुटि के रूप में (`IRONPULSE_WARNINGS_AS_ERRORS`)
- 92 टेस्ट: हर घटक के यूनिट टेस्ट, वास्तविक सॉकेट पर HTTP API और WebSocket हैंडशेक टेस्ट, TCP पर नकली Modbus डिवाइस के साथ एंड-टू-एंड टेस्ट (डिकोडिंग, सीमाएँ, टाइमआउट, संपर्क टूटना और बहाली)। AddressSanitizer/UBSan और ThreadSanitizer के तहत सभी हरे
- CI: GCC और Clang, सैनिटाइज़र, `clang-format`, सिम्युलेटर के लिए `ruff`, अनुवादों की पूर्णता की जाँच, Docker बिल्ड और लाइव डेटा जाँचने वाला Compose स्मोक टेस्ट — [`.github/workflows/ci.yml`](.github/workflows/ci.yml)
- टैग से रिलीज़: amd64 और arm64 पर नेटिव इमेज बिल्ड और GitHub Container Registry में प्रकाशन — [`.github/workflows/release.yml`](.github/workflows/release.yml)
- मल्टी-स्टेज Docker बिल्ड, कंटेनर में बिना विशेषाधिकार वाला उपयोगकर्ता, हर सेवा पर हेल्थ-चेक
- डैशबोर्ड की कोई बाहरी रनटाइम निर्भरता नहीं (Chart.js अंतर्निहित है), इसलिए यह प्रतिबंधित या ऑफ़लाइन नेटवर्क में भी काम करता है

### रोडमैप
- [x] एसिंक्रोनस Modbus TCP क्लाइंट + फ़्रेम कोडेक
- [x] WAL स्थायित्व और संग्रहण-अवधि वाला टाइम-सीरीज़ भंडारण
- [x] कोरम-वोटिंग `RuleEngine` से संयोजित Z-score, EWMA और CUSUM पहचान
- [x] REST API + स्वयं लिखा WebSocket (RFC 6455) लाइव पुश
- [x] कंटेनरों के लिए पर्यावरण-चालित कॉन्फ़िगरेशन
- [x] storage/protocol के हॉट-पाथ के लिए Google Benchmark सूट
- [x] इंटीग्रेशन टेस्ट: नकली डिवाइस से प्रकाशित अलर्ट तक पूरी पाइपलाइन
- [x] WAL की स्वचालित आवधिक सफ़ाई
- [x] मल्टी-आर्किटेक्चर Docker इमेज (linux/amd64 + linux/arm64)
- [x] प्रति डिवाइस कई सेंसर, डेटा प्रकार, स्केलिंग, input रजिस्टर
- [x] सख़्त सीमाएँ, अलर्ट की गंभीरता, कूलडाउन
- [x] सूचनाएँ: Telegram, Slack, webhook — 8 भाषाओं में
- [x] टोकन पहुँच, Prometheus मेट्रिक्स, CSV निर्यात
- [ ] गेटवे के बिना सीधे Modbus RTU (RS-485)
- [ ] डेटा स्रोत के रूप में OPC UA और MQTT
- [ ] ऑपरेटरों द्वारा अलर्ट की पुष्टि और ऑडिट लॉग
- [ ] एकल टोकन के बजाय उपयोगकर्ता और भूमिकाएँ

## योगदान
Issues और pull request का स्वागत है। PR खोलने से पहले:
```bash
cmake --preset debug && cmake --build --preset debug -j$(nproc)
ctest --preset debug --output-on-failure
find include src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format --dry-run --Werror
ruff check tools/ && ruff format --check tools/
```
CI हर pull request पर यही जाँचें (साथ में सैनिटाइज़र और Docker बिल्ड) चलाता है। संस्करणों के अनुसार बदलाव [CHANGELOG.md](CHANGELOG.md) में हैं।

## लाइसेंस
MIT — [LICENSE](LICENSE) देखें। व्यावसायिक उपयोग सहित, उपयोग, संशोधन और डिप्लॉयमेंट के लिए स्वतंत्र।
