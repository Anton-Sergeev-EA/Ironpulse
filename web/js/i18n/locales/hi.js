I18n.register("hi", {
    "page.title": "Ironpulse from Sergeev Anton — औद्योगिक टेलीमेट्री",
    "language.label": "इंटरफ़ेस की भाषा",

    "connection.connecting": "कनेक्ट हो रहा है...",
    "connection.online": "कनेक्टेड",
    "connection.reconnecting": "फिर से कनेक्ट हो रहा है...",

    "about.description":
        "<strong>Ironpulse</strong> रीयल-टाइम औद्योगिक टेलीमेट्री और विसंगति पहचान इंजन है। " +
        "यह Modbus TCP प्रोटोकॉल के ज़रिए सेंसरों से डेटा लेता है, रीडिंग को टाइम सीरीज़ के रूप में " +
        "सहेजता है (मेमोरी में और write-ahead log के माध्यम से डिस्क पर), कई सांख्यिकीय रणनीतियों " +
        "(z-score, EWMA, CUSUM) और कोरम वोटिंग से विसंगतियों का पता लगाता है, तय सीमाओं की निगरानी करता है, Telegram, Slack और webhook पर सूचनाएँ भेजता है, और परिणाम " +
        "REST + WebSocket API के ज़रिए इस डैशबोर्ड पर रीयल-टाइम में भेजता है।",
    "about.stack":
        "<strong>तकनीकें:</strong> C++20 (कोर, Modbus क्लाइंट, स्टोरेज, एनालिटिक्स, REST और " +
        "WebSocket API), JavaScript (डैशबोर्ड), Python (डेमो के लिए Modbus डिवाइस सिम्युलेटर), " +
        "Docker/Docker Compose और nginx (डिप्लॉयमेंट)।",

    "panel.devices": "डिवाइस",
    "panel.readings": "रीयल-टाइम रीडिंग",
    "panel.alerts": "सक्रिय अलर्ट",
    "alerts.empty": "कोई विसंगति नहीं मिली",

    "device.online": "ऑनलाइन",
    "device.offline": "ऑफ़लाइन",

    "alert.confirmed": "{total} में से {votes} डिटेक्टरों ने विसंगति की पुष्टि की",
    "alert.confidence": "विश्वसनीयता {percent}%",

    "auth.title": "एक्सेस टोकन आवश्यक है",
    "auth.description": "यह सर्वर सुरक्षित है। अपने व्यवस्थापक द्वारा दिया गया API टोकन दर्ज करें।",
    "auth.placeholder": "API टोकन",
    "auth.submit": "साइन इन करें",
    "auth.invalid": "अमान्य टोकन",
    "auth.unreachable": "सर्वर उपलब्ध नहीं है, फिर से प्रयास करें",
    "alert.limit_high": "मान {value} सीमा {limit} से ऊपर है",
    "alert.limit_low": "मान {value} सीमा {limit} से नीचे है",
    "severity.critical": "गंभीर",
    "severity.warning": "चेतावनी",
    "chart.download": "CSV डाउनलोड करें",
    "chart.limit_high": "अधिकतम",
    "chart.limit_low": "न्यूनतम",
    "devices.empty": "कोई डिवाइस कॉन्फ़िगर नहीं है",
    "device.sensors": "सेंसर: {count}",
});
