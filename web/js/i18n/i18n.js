// Dashboard localization.
//
// Locale dictionaries live in web/js/i18n/locales/*.js and register
// themselves via I18n.register(). They are plain scripts rather than JSON
// fetched at runtime for the same reason Chart.js is vendored: the
// dashboard must work in restricted/offline networks and must not show a
// half-translated page while a dictionary request is in flight.
//
// Language resolution order:
//   1. ?lang=<code> in the URL (handy for sharing a link in a given language)
//   2. the viewer's previous choice (localStorage)
//   3. the browser's preferred languages (navigator.languages)
//   4. Russian — the primary interface language
//
// Markup opts in with:
//   data-i18n="key"        -> element.textContent
//   data-i18n-html="key"   -> element.innerHTML (trusted dictionary markup only)
//   data-i18n-attr="attr:key[,attr:key]" -> element attributes
const I18n = {
    defaultLocale: "ru",
    storageKey: "ironpulse.locale",

    // Display order: Russian first, then by number of speakers worldwide.
    supported: [
        { code: "ru", name: "Русский", intl: "ru-RU" },
        { code: "en", name: "English", intl: "en-US" },
        { code: "zh", name: "中文", intl: "zh-CN" },
        { code: "hi", name: "हिन्दी", intl: "hi-IN" },
        { code: "es", name: "Español", intl: "es-ES" },
        { code: "fr", name: "Français", intl: "fr-FR" },
        { code: "de", name: "Deutsch", intl: "de-DE" },
        { code: "it", name: "Italiano", intl: "it-IT" },
    ],

    dictionaries: {},
    locale: null,
    listeners: [],

    register(code, dictionary) {
        this.dictionaries[code] = dictionary;
    },

    init() {
        this.locale = this._resolveInitialLocale();
        this._renderSwitcher();
        this.apply();
    },

    setLocale(code) {
        if (!this._isAvailable(code) || code === this.locale) {
            return;
        }
        this.locale = code;
        try {
            localStorage.setItem(this.storageKey, code);
        } catch {
            // Storage can be unavailable (private mode, blocked site data);
            // the choice simply won't persist across reloads.
        }
        this.apply();
        for (const listener of this.listeners) {
            listener(code);
        }
    },

    // Panels that render text dynamically subscribe here to re-render.
    onChange(listener) {
        this.listeners.push(listener);
    },

    // Translates `key`, substituting {placeholders} from `params`. Falls back
    // to Russian, then to the key itself, so a missing entry never blanks
    // out part of the UI.
    t(key, params = {}) {
        const template =
            this.dictionaries[this.locale]?.[key] ?? this.dictionaries[this.defaultLocale]?.[key] ?? key;
        return template.replace(/\{(\w+)\}/g, (match, name) => (name in params ? String(params[name]) : match));
    },

    intlLocale() {
        return this.supported.find((l) => l.code === this.locale)?.intl ?? "ru-RU";
    },

    formatTime(timestamp) {
        return new Date(timestamp).toLocaleTimeString(this.intlLocale());
    },

    apply() {
        document.documentElement.lang = this.locale;
        document.title = this.t("page.title");

        for (const el of document.querySelectorAll("[data-i18n]")) {
            el.textContent = this.t(el.dataset.i18n);
        }
        for (const el of document.querySelectorAll("[data-i18n-html]")) {
            el.innerHTML = this.t(el.dataset.i18nHtml);
        }
        for (const el of document.querySelectorAll("[data-i18n-attr]")) {
            for (const pair of el.dataset.i18nAttr.split(",")) {
                const [attr, key] = pair.split(":").map((s) => s.trim());
                el.setAttribute(attr, this.t(key));
            }
        }

        const select = document.getElementById("language-select");
        if (select) {
            select.value = this.locale;
        }
    },

    _isAvailable(code) {
        return this.supported.some((l) => l.code === code) && code in this.dictionaries;
    },

    _resolveInitialLocale() {
        const fromUrl = new URLSearchParams(window.location.search).get("lang");
        if (fromUrl && this._isAvailable(fromUrl.toLowerCase())) {
            return fromUrl.toLowerCase();
        }

        try {
            const stored = localStorage.getItem(this.storageKey);
            if (stored && this._isAvailable(stored)) {
                return stored;
            }
        } catch {
            // Fall through to browser preferences.
        }

        for (const tag of navigator.languages || [navigator.language]) {
            const base = (tag || "").toLowerCase().split("-")[0];
            if (this._isAvailable(base)) {
                return base;
            }
        }

        return this.defaultLocale;
    },

    _renderSwitcher() {
        const select = document.getElementById("language-select");
        if (!select) {
            return;
        }
        select.innerHTML = "";
        for (const { code, name } of this.supported) {
            if (!(code in this.dictionaries)) {
                continue;
            }
            const option = document.createElement("option");
            option.value = code;
            option.lang = code;
            option.textContent = name;
            select.appendChild(option);
        }
        select.addEventListener("change", () => this.setLocale(select.value));
    },
};

// Runs before the panels' own DOMContentLoaded handlers (this script is
// included first), so every panel starts in the resolved language.
document.addEventListener("DOMContentLoaded", () => {
    I18n.init();
});
