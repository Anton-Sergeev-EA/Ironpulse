// On page load, primes the dashboard with the engine's current state via
// REST — sensor metadata, device list, recent history per sensor, and
// recent alerts — before the WebSocket connection (ws-client.js) takes
// over with live updates.
//
// Without this, panels stay empty until something *changes* after the
// page was opened: DevicesPanel only ever receives a WS device_status
// event when a device's online/offline state flips, and a chart only
// gets points as new readings arrive. A dashboard that was just opened
// against an engine that's already been running for a while would show
// nothing until the next state transition — this fixes that gap.
//
// If the engine requires an API token, the viewer is asked for it first
// (auth-dialog.js) and everything below runs with it.
const AppInit = {
    config: {},

    async bootstrap() {
        IronpulseApi.loadToken();
        try {
            this.config = await IronpulseApi.getConfig();
        } catch (err) {
            console.error("Failed to load /api/v1/config:", err);
        }
        this._showVersion(this.config.version);

        if (this.config.auth_required && !IronpulseApi.token) {
            await AuthDialog.ask();
        }

        for (;;) {
            try {
                await this._loadAll();
                break;
            } catch (err) {
                if (!(err instanceof UnauthorizedError)) {
                    console.error("Failed to load initial state:", err);
                    break;
                }
                // Stored token was revoked or rotated: ask again.
                await AuthDialog.ask({ invalid: true });
            }
        }

        WsClient.connect(this.config);
    },

    async _loadAll() {
        const [sensors, devices] = await Promise.all([IronpulseApi.getSensors(), IronpulseApi.getDevices()]);
        Sensors.setAll(sensors);
        DevicesPanel.setAll(devices);
        ChartsPanel.createAll();
        await Promise.all([...sensors.map((s) => this._loadSeries(s.id)), this._loadAlerts()]);
    },

    async _loadSeries(sensorId) {
        try {
            const samples = await IronpulseApi.getSeries(sensorId, 300);
            for (const sample of samples) {
                ChartsPanel.pushPoint(sensorId, sample.timestamp, sample.value);
            }
        } catch (err) {
            if (err instanceof UnauthorizedError) {
                throw err;
            }
            console.error(`Failed to load history for ${sensorId}:`, err);
        }
    },

    async _loadAlerts() {
        const alerts = await IronpulseApi.getAlerts(20);
        // The API returns newest-first; addAlert() prepends, so feed
        // it oldest-first to end up with newest-on-top in the DOM.
        for (const alert of alerts.slice().reverse()) {
            AlertsPanel.addAlert(alert);
        }
    },

    _showVersion(version) {
        const el = document.getElementById("app-version");
        if (el && version) {
            el.textContent = `v${version}`;
        }
    },
};

document.addEventListener("DOMContentLoaded", () => {
    AppInit.bootstrap();
});
