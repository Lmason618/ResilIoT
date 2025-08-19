import sqlite3
from datetime import datetime, timedelta
import random

DB_PATH = './sensor_data.db'

# Connect to database
conn = sqlite3.connect(DB_PATH)
cursor = conn.cursor()

# Create table
cursor.execute("""
CREATE TABLE IF NOT EXISTS sensor_readings (
    timestamp TEXT,
    soil REAL,
    temp REAL,
    hum REAL,
    rain REAL,
    river REAL,
    rate_of_rise REAL,
    high_level_alert INTEGER,
    sensor_id INTEGER,
    total_daily_rain REAL,
    PRIMARY KEY (timestamp, sensor_id)
)
""")

# Clear old data
cursor.execute("DELETE FROM sensor_readings")

# Start from Jan 1 2025 00:00
start = datetime(2025, 1, 1, 0, 0)
now = datetime.now()

print(f"Generating hourly test data from {start} to {now}...")

current = start
while current <= now:
    # Sensor 2: soil, temp, hum, rain per min, total_daily_rain
    soil = round(random.uniform(20, 80), 1)
    temp = round(random.uniform(5, 35), 1)
    hum = round(random.uniform(30, 90), 1)
    rain = round(random.uniform(0, 2), 2)
    total_daily_rain = round(random.uniform(0, 20), 2)

    cursor.execute("""
        INSERT INTO sensor_readings (timestamp, soil, temp, hum, rain, total_daily_rain, sensor_id)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    """, (current.strftime('%Y-%m-%d %H:%M:%S'), soil, temp, hum, rain, total_daily_rain, 2))

    # Sensor 3: river height, rate_of_rise, high_level_alert
    river = round(random.uniform(2.0, 3.5), 2)
    rate_of_rise = round(random.uniform(-0.05, 0.05), 3)
    high_level_alert = random.choice([0, 1])

    cursor.execute("""
        INSERT INTO sensor_readings (timestamp, river, rate_of_rise, high_level_alert, sensor_id)
        VALUES (?, ?, ?, ?, ?)
    """, (current.strftime('%Y-%m-%d %H:%M:%S'), river, rate_of_rise, high_level_alert, 3))

    current += timedelta(hours=1)

conn.commit()
conn.close()
print("Test data inserted.")
