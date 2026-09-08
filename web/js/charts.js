// Manages one Chart.js line chart per sensor, appending live points as they
// arrive over the WebSocket connection (see ws-client.js).
const ChartsPanel = {
    charts: new Map(),
    maxPoints: 120,

    ensureChart(sensorId) {
        if (this.charts.has(sensorId)) {
            return this.charts.get(sensorId);
        }

        const container = document.getElementById("charts-container");
        const card = document.createElement("div");
        card.className = "chart-card";
        card.innerHTML = `<h3>${sensorId}</h3><canvas></canvas>`;
        container.appendChild(card);

        const ctx = card.querySelector("canvas").getContext("2d");
        const chart = new Chart(ctx, {
            type: "line",
            data: {
                labels: [],
                datasets: [
                    {
                        label: sensorId,
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
                animation: false,
                responsive: true,
                plugins: { legend: { display: false } },
                scales: {
                    x: { ticks: { color: "#8891a7" }, grid: { color: "#232c42" } },
                    y: { ticks: { color: "#8891a7" }, grid: { color: "#232c42" } },
                },
            },
        });

        this.charts.set(sensorId, chart);
        return chart;
    },

    pushPoint(sensorId, timestamp, value) {
        const chart = this.ensureChart(sensorId);
        const label = new Date(timestamp).toLocaleTimeString();

        chart.data.labels.push(label);
        chart.data.datasets[0].data.push(value);

        if (chart.data.labels.length > this.maxPoints) {
            chart.data.labels.shift();
            chart.data.datasets[0].data.shift();
        }

        chart.update("none");
    },
};
