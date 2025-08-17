/* Dummy graph, just for ui at the min. I need to impliment this later*/
function main() {
    'use strict';

    var chart;

    function createChart(data, labels) {
        var ctx;
        ctx = document.getElementById('historicChart').getContext('2d');
        if (chart) {
            chart.destroy();
        }
        chart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: labels,
                datasets: [
                    {
                        label: 'Soil Moisture',
                        borderColor: '#3b82f6',
                        backgroundColor: '#3b82f688',
                        data: data.soil
                    },
                    {
                        label: 'Rainfall (mm)',
                        borderColor: '#6366f1',
                        backgroundColor: '#6366f188',
                        data: data.rain
                    },
                    {
                        label: 'Temperature (°C)',
                        borderColor: '#f59e0b',
                        backgroundColor: '#f59e0b88',
                        data: data.temp
                    },
                    {
                        label: 'Humidity (%)',
                        borderColor: '#10b981',
                        backgroundColor: '#10b98188',
                        data: data.hum
                    },
                    {
                        label: 'River Height (m)',
                        borderColor: '#8b5cf6',
                        backgroundColor: '#8b5cf688',
                        data: data.river
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        labels: {
                            color: '#f3f4f6',
                            usePointStyle: true
                        }
                    }
                },
                scales: {
                    x: {
                        ticks: {
                            color: '#e5e7eb'
                        }
                    },
                    y: {
                        ticks: {
                            color: '#e5e7eb'
                        }
                    }
                }
            }
        });
    }

    function updateGraph(range) {
        var dummyData, labels, i, days, months;

        days = new Array(30);
        for (i = 0; i < 30; i += 1) {
            days[i] = 'Day ' + (i + 1);
        }

        months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',
            'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];

        dummyData = {
            day: {
                soil: [40, 42, 41, 43, 40],
                rain: [2, 4, 1, 0, 3],
                temp: [25, 26, 26, 27, 26],
                hum: [60, 65, 63, 66, 64],
                river: [2.4, 2.5, 2.5, 2.6, 2.5]
            },
            week: {
                soil: [40, 42, 43, 45, 44, 43, 42],
                rain: [12, 10, 5, 7, 3, 0, 0],
                temp: [24, 25, 26, 27, 26, 25, 24],
                hum: [60, 63, 64, 66, 67, 65, 64],
                river: [2.3, 2.4, 2.5, 2.6, 2.5, 2.4, 2.3]
            },
            month: {
                soil: (new Array(30)).fill(40),
                rain: (new Array(30)).fill(2),
                temp: (new Array(30)).fill(26),
                hum: (new Array(30)).fill(65),
                river: (new Array(30)).fill(2.5)
            },
            year: {
                soil: (new Array(12)).fill(40),
                rain: (new Array(12)).fill(20),
                temp: (new Array(12)).fill(26),
                hum: (new Array(12)).fill(65),
                river: (new Array(12)).fill(2.5)
            }
        };

        labels = {
            day: ['08:00', '10:00', '12:00', '14:00', '16:00'],
            week: ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'],
            month: days,
            year: months
        };

        createChart(dummyData[range], labels[range]);
    }

    function renderSoilGauge(value) {
        var ctx, gauge;
        ctx = document.getElementById('soilGauge').getContext('2d');
        gauge = new Chart(ctx, {
            type: 'doughnut',
            data: {
                datasets: [{
                    data: [value, 100 - value],
                    backgroundColor: ['#3b82f6', '#1f2937'],
                    borderWidth: 0
                }]
            },
            options: {
                cutout: '80%',
                rotation: -90,
                circumference: 180,
                plugins: {
                    tooltip: {
                        enabled: false
                    },
                    legend: {
                        display: false
                    },
                    title: {
                        display: true,
                        text: value + '%',
                        position: 'bottom',
                        color: '#ffffff',
                        font: {
                            size: 20
                        }
                    }
                }
            }
        });
        return gauge;
    }

    document.addEventListener('DOMContentLoaded', function () {
        updateGraph('day');
        renderSoilGauge(40);
    });
}

main();