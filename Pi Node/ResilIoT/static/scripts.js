// Get refs to HTML elements
const soilEl = document.getElementById('soil');
const tempEl = document.getElementById('temp');
const humEl = document.getElementById('hum');
const rainEl = document.getElementById('rain');
const riverEl = document.getElementById('river');
const rainSince9El = document.getElementById('rain-since-9');
const ctx = document.getElementById('sensorChart').getContext('2d');
// Weather Forecast
const forecastMinEl = document.getElementById('forecast-min');
const forecastMaxEl = document.getElementById('forecast-max');
const forecastRainProbEl = document.getElementById('forecast-rain-prob');
const forecastRainIntensityEl = document.getElementById('forecast-rain-intensity');

// Alert
const alertLevelEl = document.getElementById('alert-level');
// Gauge elements
const soilGaugeEl = document.getElementById('soilGauge');
const riverGaugeEl = document.getElementById('riverGauge');


let sensorChart = null;
let currentRange = 'day';

//  Gauges
const soilOpts = {
    angle: 0.15,
    lineWidth: 0.44,
    pointer: { length: 0.9, strokeWidth: 0.035 },
    limitMax: false,
    limitMin: false,
    colorStart: '#ff0000',
    colorStop: '#00ff00',
    strokeColor: '#E0E0E0',
    generateGradient: true,
    highDpiSupport: true,
    staticZones: [
        { strokeStyle: "red", min: 0, max: 15 },
        { strokeStyle: "green", min: 16, max: 80 },
        { strokeStyle: "red", min: 81, max: 100 }
    ]
};
const riverOpts = {
    angle: 0.15,
    lineWidth: 0.44,
    pointer: { length: 0.9, strokeWidth: 0.035 },
    limitMax: false,
    limitMin: false,
    strokeColor: '#E0E0E0',
    generateGradient: true,
    staticZones: [
        { strokeStyle: "green", min: 0, max: 1.2 },
        { strokeStyle: "yellow", min: 1.21, max: 1.5 },
        { strokeStyle: "orange", min: 1.51, max: 1.8 },
        { strokeStyle: "red", min: 1.81, max: 5 }
    ],
    highDpiSupport: true
};
const soilGauge = new Gauge(soilGaugeEl).setOptions(soilOpts);
soilGauge.maxValue = 100;
soilGauge.setMinValue(0);
soilGauge.set(0);

const riverGauge = new Gauge(riverGaugeEl).setOptions(riverOpts);
riverGauge.maxValue = 5;
riverGauge.setMinValue(0);
riverGauge.set(0);

// Update gauges and DOM
function updateGauges(soil, river) {
    soilEl.textContent = soil !== undefined ? `${soil}%` : '-';
    riverEl.textContent = river !== undefined ? `${river} m` : '-';

    if (soil !== undefined) soilGauge.set(soil);
    if (river !== undefined) riverGauge.set(river);
}

// Get last sens readings
async function fetchLatest() {
    try {
        const res = await fetch('/api/latest');
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();

        updateGauges(data.soil, data.river);
        tempEl.textContent = data.temp !== undefined ? data.temp + '°C' : '-';
        humEl.textContent = data.hum !== undefined ? data.hum + '%' : '-';
        rainEl.textContent = data.rain ?? '-';
        rainSince9El.textContent = data.total_rain ?? '-';
    } catch (err) {
        console.error("Error fetching latest readings:", err);
        soilEl.textContent = tempEl.textContent = humEl.textContent =
            rainEl.textContent = riverEl.textContent = 'Error';
    }
}
async function fetchLocalForecast() {
    try {
        const res = await fetch('/api/forecast/today');
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        const forecast = data.forecast || {}; // fallback to empty object
        // Check if data exists for today
        if (!data || Object.keys(data).length === 0) {
            forecastMinEl.textContent = 'No current forecast';
            forecastMaxEl.textContent = 'No current forecast';
            forecastRainProbEl.textContent = 'No current forecast';
            forecastRainIntensityEl.textContent = 'No current forecast';
            return;
        }

        forecastMinEl.textContent = `Min temp: ${forecast.min_temp ?? '-'}`;
        forecastMaxEl.textContent = `Max temp: ${forecast.max_temp ?? '-'}`;
        forecastRainProbEl.textContent = `Probability of rain: ${forecast.precip_prob != null ? forecast.precip_prob + '%' : '-'}`;
        forecastRainIntensityEl.textContent = `Intensity of rain: ${forecast.precip_intensity ?? '-'}`;
    } catch (err) {
        console.error("Error fetching local forecast:", err);
        forecastMinEl.textContent = forecastMaxEl.textContent =
            forecastRainProbEl.textContent = forecastRainIntensityEl.textContent = 'Error';
    }
}


// Fetch data and update chart
async function updateChart(range = currentRange) {
    try {
        const res = await fetch(`/api/historic/${range}`);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();

        const datasets = [
            { label: 'Soil (%)', data: data.soil, borderColor: 'green', fill: false },
            { label: 'Temp (°C)', data: data.temp, borderColor: 'red', fill: false },
            { label: 'Humidity (%)', data: data.hum, borderColor: 'blue', fill: false },
            { label: 'Rain Max (mm/min)', data: data.rain_max, borderColor: 'aqua', fill: false },
            { label: 'Rain Total (mm)', data: data.rain_total, borderColor: 'skyblue', fill: false },
            { label: 'River Height (m)', data: data.river, borderColor: 'purple', fill: false }
        ];

        const options = {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                x: { type: 'category', ticks: { autoSkip: true, maxTicksLimit: 12 } },
            },
            interaction: { mode: 'index', intersect: false },
            plugins: { legend: { display: true } }
        };

        if (!sensorChart) {
            sensorChart = new Chart(ctx, {
                type: 'line',
                data: {
                    labels: data.labels,
                    datasets
                },
                options
            });
        } else {
            sensorChart.data.labels = data.labels;
            sensorChart.data.datasets.forEach((ds, i) => ds.data = datasets[i].data);
            sensorChart.update();
        }
    } catch (err) {
        console.error("Error updating chart:", err);
        if (sensorChart) {
            sensorChart.data.labels = [];
            sensorChart.data.datasets.forEach(ds => ds.data = []);
            sensorChart.update();
        }
    }
}

// Fetch alert from api.py
async function fetchLocalAlert() {
    try {
        const res = await fetch('/api/alert/latest');
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();

        alertLevelEl.textContent = data.level ?? 'No data';
    } catch (err) {
        console.error("Error fetching local alert:", err);
        alertLevelEl.textContent = 'Error';
    }
}

// Update all alerts/forecast
async function updateAllAlerts() {
    await fetchLatest();
    await fetchLocalForecast();
    await fetchLocalAlert();
}

// Set button listeners
document.querySelectorAll('.range-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        currentRange = btn.dataset.range;
        updateChart(currentRange);
    });
});

fetchLatest();
updateChart();
updateAllAlerts();


// Refresh 60s
setInterval(() => {
    fetchLatest();
    updateChart(currentRange);
    updateAllAlerts();
}, 60000);
