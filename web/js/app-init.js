// On page load, primes the dashboard with the engine's current state via
// REST — device list, recent history per sensor, and recent alerts —
// before the WebSocket connection (ws-client.js) takes over with live
// updates.
//
// Without this, panels stay empty until something *changes* after the
// page was opened: DevicesPanel only ever receives a WS device_status
// event when a device's online/offline state flips, and a chart only
// gets points as new readings arrive. A dashboard that was just opened
// against an engine that's already been running for a while would show
// nothing until the next state transition — this fixes that gap.
const AppInit = {
    async bootstrap() {
        await Promise.all([this._loadDevices(), this._loadAlerts()]);
    },

    async _loadDevices() {
        try {
            const devices = await IronpulseApi.getDevices();
            for (const device of devices) {
                DevicesPanel.setStatus(device.id, device.online);
            }
            // Prime each sensor's chart with recent history so it doesn't
            // start as a blank canvas that only fills in live.
            await Promise.all(devices.map((d) => this._loadSeries(d.id)));
        } catch (err) {
            console.error("Failed to load initial device list:", err);
        }
    },

    async _loadSeries(sensorId) {
        try {
            const samples = await IronpulseApi.getSeries(sensorId, 300);
            for (const sample of samples) {
                ChartsPanel.pushPoint(sensorId, sample.timestamp, sample.value);
            }
        } catch (err) {
            console.error(`Failed to load history for ${sensorId}:`, err);
        }
    },

    async _loadAlerts() {
        try {
            const alerts = await IronpulseApi.getAlerts(10);
            // The API returns newest-first; addAlert() prepends, so feed
            // it oldest-first to end up with newest-on-top in the DOM.
            for (const alert of alerts.slice().reverse()) {
                AlertsPanel.addAlert(alert);
            }
        } catch (err) {
            console.error("Failed to load initial alerts:", err);
        }
    },
};

document.addEventListener("DOMContentLoaded", () => {
    AppInit.bootstrap();
});
