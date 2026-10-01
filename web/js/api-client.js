// Thin wrapper around ironpulse's REST API (contract: docs/openapi.yaml).
//
// When the server is started with an api_token, every data endpoint needs
// it: the token is sent as "Authorization: Bearer <token>" and kept in
// localStorage so the viewer is asked only once per browser.
class UnauthorizedError extends Error {
    constructor() {
        super("unauthorized");
        this.name = "UnauthorizedError";
    }
}

const IronpulseApi = {
    baseUrl: "",
    tokenKey: "ironpulse.token",
    token: null,

    loadToken() {
        try {
            this.token = localStorage.getItem(this.tokenKey);
        } catch {
            this.token = null;
        }
    },

    saveToken(token) {
        this.token = token || null;
        try {
            if (token) {
                localStorage.setItem(this.tokenKey, token);
            } else {
                localStorage.removeItem(this.tokenKey);
            }
        } catch {
            // Storage unavailable: the token lives for this page only.
        }
    },

    async getConfig() {
        return this._get("/api/v1/config");
    },

    async getDevices() {
        return this._get("/api/v1/devices");
    },

    async getSensors() {
        return this._get("/api/v1/sensors");
    },

    async getSeries(sensorId, sinceSeconds = 300) {
        return this._get(`/api/v1/series/${encodeURIComponent(sensorId)}?since=${sinceSeconds}`);
    },

    async getAlerts(limit = 50) {
        return this._get(`/api/v1/alerts?limit=${limit}`);
    },

    // Downloads a sensor's retained history as CSV. Fetched with the
    // Authorization header and saved from a blob, so the token never ends
    // up in a URL, the browser history or a proxy log.
    async downloadCsv(sensorId, sinceSeconds) {
        const response = await this._fetch(
            `/api/v1/series/${encodeURIComponent(sensorId)}?since=${sinceSeconds}&format=csv`,
        );
        const blob = await response.blob();
        const url = URL.createObjectURL(blob);
        const link = document.createElement("a");
        link.href = url;
        link.download = `${sensorId}.csv`;
        document.body.appendChild(link);
        link.click();
        link.remove();
        setTimeout(() => URL.revokeObjectURL(url), 10000);
    },

    async _fetch(path) {
        const headers = this.token ? { Authorization: `Bearer ${this.token}` } : {};
        const response = await fetch(this.baseUrl + path, { headers });
        if (response.status === 401) {
            throw new UnauthorizedError();
        }
        if (!response.ok) {
            throw new Error(`API request failed: ${path} -> ${response.status}`);
        }
        return response;
    },

    async _get(path) {
        const response = await this._fetch(path);
        return response.json();
    },
};
