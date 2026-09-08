// Thin wrapper around ironpulse's REST API.
// Endpoints match docs/api.md / include/ironpulse/api/http_server.hpp.
const IronpulseApi = {
    baseUrl: "",

    async getDevices() {
        return this._get("/api/v1/devices");
    },

    async getSeries(sensorId, sinceSeconds = 300) {
        return this._get(`/api/v1/series/${encodeURIComponent(sensorId)}?since=${sinceSeconds}`);
    },

    async getAlerts(limit = 50) {
        return this._get(`/api/v1/alerts?limit=${limit}`);
    },

    async _get(path) {
        const response = await fetch(this.baseUrl + path);
        if (!response.ok) {
            throw new Error(`API request failed: ${path} -> ${response.status}`);
        }
        return response.json();
    },
};
