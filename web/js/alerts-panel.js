const AlertsPanel = {
    listEl: null,
    maxItems: 20,
    alerts: [], // newest first; kept so the list can be re-rendered on a language switch

    init() {
        this.listEl = document.getElementById("alerts-list");
        I18n.onChange(() => this.render());
    },

    addAlert(alert) {
        this.alerts.unshift(alert);
        // Keep the list from growing unbounded.
        this.alerts.length = Math.min(this.alerts.length, this.maxItems);
        this.render();
    },

    // Engines that report vote counts get a fully localized message; older
    // payloads without them fall back to the engine's own text.
    _message(alert) {
        if (Number.isFinite(alert.votes) && Number.isFinite(alert.detectors_total) && alert.detectors_total > 0) {
            return I18n.t("alert.confirmed", { votes: alert.votes, total: alert.detectors_total });
        }
        return alert.message;
    },

    render() {
        if (this.alerts.length === 0) {
            this.listEl.innerHTML = `<li class="empty-state" data-i18n="alerts.empty">${I18n.t("alerts.empty")}</li>`;
            return;
        }

        this.listEl.innerHTML = "";
        for (const alert of this.alerts) {
            const item = document.createElement("li");
            item.className = "alert-item";
            item.innerHTML = `
                <div class="alert-title">${alert.sensor_id}: ${this._message(alert)}</div>
                <div class="alert-meta">
                    ${I18n.t("alert.confidence", { percent: Math.round(alert.confidence * 100) })} ·
                    ${I18n.formatTime(alert.timestamp)}
                </div>
            `;
            this.listEl.appendChild(item);
        }
    },
};

const DevicesPanel = {
    listEl: null,
    devices: new Map(),

    init() {
        this.listEl = document.getElementById("device-list");
        I18n.onChange(() => this.render());
    },

    setStatus(deviceId, online) {
        this.devices.set(deviceId, online);
        this.render();
    },

    render() {
        this.listEl.innerHTML = "";
        for (const [id, online] of this.devices.entries()) {
            const item = document.createElement("li");
            item.className = "device-item";
            item.innerHTML = `
                <span>${id}</span>
                <span class="status ${online ? "online" : "offline"}">
                    ${I18n.t(online ? "device.online" : "device.offline")}
                </span>
            `;
            this.listEl.appendChild(item);
        }
    },
};

document.addEventListener("DOMContentLoaded", () => {
    AlertsPanel.init();
    DevicesPanel.init();
});
