const AlertsPanel = {
    listEl: null,

    init() {
        this.listEl = document.getElementById("alerts-list");
    },

    addAlert(alert) {
        if (this.listEl.querySelector(".empty-state")) {
            this.listEl.innerHTML = "";
        }

        const item = document.createElement("li");
        item.className = "alert-item";
        item.innerHTML = `
            <div class="alert-title">${alert.sensor_id}: ${alert.message}</div>
            <div class="alert-meta">
                уверенность ${Math.round(alert.confidence * 100)}% ·
                ${new Date(alert.timestamp).toLocaleTimeString()}
            </div>
        `;
        this.listEl.prepend(item);

        // Keep the list from growing unbounded.
        while (this.listEl.children.length > 20) {
            this.listEl.removeChild(this.listEl.lastChild);
        }
    },
};

const DevicesPanel = {
    listEl: null,
    devices: new Map(),

    init() {
        this.listEl = document.getElementById("device-list");
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
                    ${online ? "online" : "offline"}
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
