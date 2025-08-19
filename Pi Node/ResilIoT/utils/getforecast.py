import requests
import sqlite3
from datetime import date

DB_PATH = 'db/sensor_data.db'

def fetch_and_store_forecast():
    url = (
        "https://api.open-meteo.com/v1/forecast"
        "?latitude=51.108"
        "&longitude=4.161"
        "&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max,rain_sum"
        "&timezone=GMT"
        "&forecast_days=1"
    )
    resp = requests.get(url)
    resp.raise_for_status()
    data = resp.json()

    daily = data["daily"]

    min_temp = daily["temperature_2m_min"][0]
    max_temp = daily["temperature_2m_max"][0]
    precip_prob = daily.get("precipitation_probability_max", [0])[0]
    rain_sum = daily.get("rain_sum", [0])[0]

    # Define has_precip
    has_precip = int((precip_prob > 0) or (rain_sum > 0))

    # Map rain_sum into a text category for precip_intensity
    if not has_precip:
        precip_intensity = "NA"
    elif rain_sum < 2.5:
        precip_intensity = "Light"
    elif rain_sum < 7.5:
        precip_intensity = "Moderate"
    else:
        precip_intensity = "Heavy"

    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    # Keep schema the same
    c.execute('''
        CREATE TABLE IF NOT EXISTS forecast (
            date TEXT PRIMARY KEY,
            min_temp REAL,
            max_temp REAL,
            has_precip INTEGER,
            precip_prob INTEGER,
            precip_intensity TEXT
        )
    ''')
    c.execute('''
        INSERT OR REPLACE INTO forecast (date, min_temp, max_temp, has_precip, precip_prob, precip_intensity)
        VALUES (?, ?, ?, ?, ?, ?)
    ''', (date.today().isoformat(), min_temp, max_temp, has_precip, precip_prob, precip_intensity))
    conn.commit()
    conn.close()
    print(f"Forecast stored for {date.today().isoformat()}")

if __name__ == "__main__":
    fetch_and_store_forecast()
