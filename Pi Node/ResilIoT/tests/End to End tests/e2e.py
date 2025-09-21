import pytest
import sqlite3
from datetime import datetime
from unittest.mock import patch

from app import create_app


@pytest.fixture(scope="module")
def test_client():
    """Spin up Flask test client with isolated in-memory DB."""
    app = create_app()
    app.config["TESTING"] = True

    # Shared in-memory DB
    conn = sqlite3.connect(":memory:")
    conn.row_factory = sqlite3.Row
    cursor = conn.cursor()

    # Create sensor_readings + forecast tables
    cursor.execute("""
        CREATE TABLE sensor_readings (
            timestamp TEXT,
            soil REAL,
            temp REAL,
            hum REAL,
            rain REAL,
            total_daily_rain REAL,
            river REAL,
            rate_of_rise REAL,
            high_level_alert INTEGER,
            sensor_id INTEGER
        )
    """)
    cursor.execute("""
        CREATE TABLE forecast (
            date TEXT,
            min_temp REAL,
            max_temp REAL,
            has_precip INTEGER,
            precip_prob REAL,
            precip_intensity TEXT
        )
    """)
    conn.commit()

    # Monkeypatch api.get_conn() uses the in-memory DB
    import routes.api
    routes.api.get_conn = lambda: conn

    # Monkeypatch thresholds to control alert logic
    fake_thresholds = {
        "Low": {"river_max": 100, "soil_min": 10, "soil_max": 50},
        "Mid": {"river_max": 200, "soil_min": 5, "soil_max": 60, "rain_thresh": 3},
        "High": {"river_max": 300, "soil_min": 0, "soil_max": 70, "rain_thresh": 5},
    }
    patch_thresholds = patch("routes.api.load_thresholds", return_value=fake_thresholds)
    patch_thresholds.start()

    with app.test_client() as client:
        yield client, conn

    patch_thresholds.stop()


def test_full_sensor_to_alert_flow(test_client):
    client, conn = test_client
    cursor = conn.cursor()

    # Step 1: simulate river sensor, writes to DB
    cursor.execute(
        "INSERT INTO sensor_readings (timestamp, river, sensor_id) VALUES (?, ?, ?)",
        (datetime.now().strftime("%Y-%m-%d %H:%M:%S"), 350, 3)
    )
    conn.commit()

    # Step 2: call /api/latest endpoint
    res_latest = client.get("/api/latest")
    assert res_latest.status_code == 200
    data_latest = res_latest.get_json()
    assert data_latest["river"] == 350

    # Step 3: call /api/alert/latest endpoint
    res_alert = client.get("/api/alert/latest")
    assert res_alert.status_code == 200
    alert = res_alert.get_json()
    assert alert["level"] == "High"
