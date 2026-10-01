// Draws a sensor's configured low/high limits as dashed horizontal lines,
// so the distance between the live value and the alarm threshold is
// visible at a glance. Lines outside the visible range are skipped rather
// than stretching the axis (which would flatten the actual signal).
const LimitLinesPlugin = {
    id: "limitLines",
    afterDatasetsDraw(chart, _args, options) {
        const limits = options?.limits;
        if (!limits) {
            return;
        }
        const { ctx, chartArea, scales } = chart;
        const y = scales.y;
        ctx.save();
        ctx.setLineDash([6, 4]);
        ctx.lineWidth = 1;
        ctx.font = "11px system-ui, sans-serif";
        for (const [key, value] of [
            ["chart.limit_high", limits.high],
            ["chart.limit_low", limits.low],
        ]) {
            if (value === null || value === undefined || value < y.min || value > y.max) {
                continue;
            }
            const py = y.getPixelForValue(value);
            ctx.strokeStyle = "rgba(255, 84, 112, 0.8)";
            ctx.beginPath();
            ctx.moveTo(chartArea.left, py);
            ctx.lineTo(chartArea.right, py);
            ctx.stroke();
            ctx.fillStyle = "rgba(255, 84, 112, 0.9)";
            ctx.textAlign = "right";
            ctx.fillText(`${I18n.t(key)} ${I18n.formatNumber(value)}`, chartArea.right - 4, py - 4);
        }
        ctx.restore();
    },
};

// Manages one Chart.js line chart per sensor, appending live points as they
// arrive over the WebSocket connection (see ws-client.js).
const ChartsPanel = {
    charts: new Map(), // sensor id -> { chart, card }
    maxPoints: 120,
    downloadSeconds: 7 * 24 * 3600, // CSV export: up to a week (bounded by retention)

    init() {
        // X-axis labels are formatted times; re-format them (and the
        // current values) in the new locale when the viewer switches
        // language.
        I18n.onChange(() => {
            for (const [sensorId, entry] of this.charts.entries()) {
                const chart = entry.chart;
                chart.options.locale = I18n.intlLocale();
                chart.data.labels = chart.$timestamps.map((ts) => I18n.formatTime(ts));
                chart.update("none");
                this._renderValue(sensorId);
            }
        });
    },

    // Creates cards for every known sensor up front, in configuration
    // order, so the layout does not reshuffle as data arrives.
    createAll() {
        for (const sensorId of Sensors.byId.keys()) {
            this.ensureChart(sensorId);
        }
    },

    ensureChart(sensorId) {
        if (this.charts.has(sensorId)) {
            return this.charts.get(sensorId).chart;
        }

        const meta = Sensors.get(sensorId);
        const container = document.getElementById("charts-container");
        const card = document.createElement("article");
        card.className = "chart-card";
        card.innerHTML = `
            <header class="chart-head">
                <div class="chart-titles">
                    <h3>${escapeHtml(Sensors.name(sensorId))}</h3>
                    <div class="chart-sub">${escapeHtml(meta?.device_id ? `${meta.device_id} · ${sensorId}` : sensorId)}</div>
                </div>
                <div class="chart-value" aria-live="polite">—</div>
            </header>
            <canvas></canvas>
            <footer class="chart-foot">
                <button type="button" class="link-button" data-i18n="chart.download">${I18n.t("chart.download")}</button>
            </footer>
        `;
        card.querySelector("button").addEventListener("click", (event) => this._download(sensorId, event.currentTarget));
        container.appendChild(card);

        const ctx = card.querySelector("canvas").getContext("2d");
        const chart = new Chart(ctx, {
            type: "line",
            data: {
                labels: [],
                datasets: [
                    {
                        label: Sensors.name(sensorId),
                        data: [],
                        borderColor: "#4f8cff",
                        backgroundColor: "rgba(79, 140, 255, 0.1)",
                        tension: 0.3,
                        pointRadius: 0,
                        borderWidth: 2,
                    },
                ],
            },
            options: {
                locale: I18n.intlLocale(), // axis number formatting follows the chosen language
                animation: false,
                responsive: true,
                plugins: {
                    legend: { display: false },
                    limitLines: { limits: meta?.limits },
                },
                scales: {
                    x: {
                        ticks: { color: "#8891a7", maxTicksLimit: 5, maxRotation: 0, autoSkipPadding: 12 },
                        grid: { color: "#232c42" },
                    },
                    y: { ticks: { color: "#8891a7" }, grid: { color: "#232c42" } },
                },
            },
            plugins: [LimitLinesPlugin],
        });

        chart.$timestamps = [];
        this.charts.set(sensorId, { chart, card });
        if (meta?.latest) {
            this._renderValue(sensorId, meta.latest.value);
        }
        return chart;
    },

    pushPoint(sensorId, timestamp, value) {
        const chart = this.ensureChart(sensorId);
        chart.$timestamps.push(timestamp);
        chart.data.labels.push(I18n.formatTime(timestamp));
        chart.data.datasets[0].data.push(value);

        if (chart.data.labels.length > this.maxPoints) {
            chart.$timestamps.shift();
            chart.data.labels.shift();
            chart.data.datasets[0].data.shift();
        }

        chart.update("none");
        this._renderValue(sensorId, value);
    },

    _renderValue(sensorId, value) {
        const entry = this.charts.get(sensorId);
        if (!entry) {
            return;
        }
        if (value !== undefined) {
            entry.lastValue = value;
        }
        const el = entry.card.querySelector(".chart-value");
        el.textContent = Sensors.formatValue(sensorId, entry.lastValue);
        el.classList.toggle("breach", Sensors.breaches(sensorId, entry.lastValue));
    },

    async _download(sensorId, button) {
        button.disabled = true;
        try {
            await IronpulseApi.downloadCsv(sensorId, this.downloadSeconds);
        } catch (err) {
            console.error(`CSV export failed for ${sensorId}:`, err);
            if (err instanceof UnauthorizedError) {
                await AuthDialog.ask({ invalid: true });
            }
        } finally {
            button.disabled = false;
        }
    },
};

document.addEventListener("DOMContentLoaded", () => {
    ChartsPanel.init();
});
