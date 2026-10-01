// Sensor metadata (name, unit, device, limits) loaded from
// GET /api/v1/sensors, shared by the charts, alerts and devices panels.
const Sensors = {
    byId: new Map(),

    setAll(list) {
        this.byId.clear();
        for (const sensor of list) {
            this.byId.set(sensor.id, sensor);
        }
    },

    get(id) {
        return this.byId.get(id);
    },

    name(id) {
        return this.byId.get(id)?.name || id;
    },

    unit(id) {
        return this.byId.get(id)?.unit || "";
    },

    // "72.5 °C" in the viewer's language (decimal separator, digits).
    formatValue(id, value) {
        if (value === null || value === undefined || Number.isNaN(Number(value))) {
            return "—";
        }
        const unit = this.unit(id);
        const number = I18n.formatNumber(value);
        return unit ? `${number} ${unit}` : number;
    },

    // True when the value is outside the sensor's configured limits.
    breaches(id, value) {
        const limits = this.byId.get(id)?.limits;
        if (!limits || value === null || value === undefined) {
            return false;
        }
        return (limits.high !== null && value > limits.high) || (limits.low !== null && value < limits.low);
    },
};

// Minimal HTML escaping for the few places that build markup from data
// (sensor names come from the config file, but nothing on this page
// should trust strings it did not write itself).
function escapeHtml(text) {
    return String(text)
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;")
        .replaceAll("'", "&#39;");
}
