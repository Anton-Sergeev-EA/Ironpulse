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

    // Localized description built from the alert's structured fields;
    // payloads from older engines fall back to the engine's own text.
    _message(alert) {
        if (alert.kind === "limit" && Number.isFinite(alert.limit)) {
            return I18n.t(alert.direction === "low" ? "alert.limit_low" : "alert.limit_high", {
                value: Sensors.formatValue(alert.sensor_id, alert.value),
                limit: Sensors.formatValue(alert.sensor_id, alert.limit),
            });
        }
        if (Number.isFinite(alert.votes) && Number.isFinite(alert.detectors_total) && alert.detectors_total > 0) {
            return I18n.t("alert.confirmed", { votes: alert.votes, total: alert.detectors_total });
        }
        return alert.message;
    },

    render() {
        if (this.alerts.length === 0) {
            this.listEl.innerHTML = `<li class="empty-state" data-i18n="alerts.empty">${escapeHtml(I18n.t("alerts.empty"))}</li>`;
            return;
        }

        this.listEl.innerHTML = "";
        for (const alert of this.alerts) {
            const severity = alert.severity === "critical" ? "critical" : "warning";
            const meta = [I18n.formatTime(alert.timestamp)];
            if (alert.kind !== "limit" && Number.isFinite(alert.confidence)) {
                meta.unshift(I18n.t("alert.confidence", { percent: Math.round(alert.confidence * 100) }));
            }
            const item = document.createElement("li");
            item.className = `alert-item ${severity}`;
            item.innerHTML = `
                <div class="alert-head">
                    <span class="severity-badge ${severity}">${escapeHtml(I18n.t(`severity.${severity}`))}</span>
                    <span class="alert-sensor">${escapeHtml(Sensors.name(alert.sensor_id))}</span>
                </div>
                <div class="alert-title">${escapeHtml(this._message(alert))}</div>
                <div class="alert-meta">${escapeHtml(meta.join(" · "))}</div>
            `;
            this.listEl.appendChild(item);
        }
    },
};

const DevicesPanel = {
    listEl: null,
    devices: new Map(), // id -> { online, sensors }

    init() {
        this.listEl = document.getElementById("device-list");
        I18n.onChange(() => this.render());
    },

    setAll(devices) {
        for (const device of devices) {
            this.devices.set(device.id, { online: device.online, sensors: device.sensors?.length ?? 0 });
        }
        this.render();
    },

    setStatus(deviceId, online) {
        const current = this.devices.get(deviceId) ?? { sensors: 0 };
        this.devices.set(deviceId, { ...current, online });
        this.render();
    },

    render() {
        if (this.devices.size === 0) {
            this.listEl.innerHTML = `<li class="empty-state" data-i18n="devices.empty">${escapeHtml(I18n.t("devices.empty"))}</li>`;
            return;
        }
        this.listEl.innerHTML = "";
        for (const [id, { online, sensors }] of this.devices.entries()) {
            const item = document.createElement("li");
            item.className = "device-item";
            item.innerHTML = `
                <div class="device-name">
                    <span>${escapeHtml(id)}</span>
                    ${sensors ? `<span class="device-sub">${escapeHtml(I18n.t("device.sensors", { count: sensors }))}</span>` : ""}
                </div>
                <span class="status ${online ? "online" : "offline"}">
                    ${escapeHtml(I18n.t(online ? "device.online" : "device.offline"))}
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
