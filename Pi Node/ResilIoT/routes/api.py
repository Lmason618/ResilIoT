from flask import Flask, jsonify
import sqlite3
from datetime import datetime, date, timedelta

app = Flask(__name__)
DB_PATH = './db/sensor_data.db'


def get_db_connection():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


@app.route('/api/latest')
def latest():
     try:
         conn = get_conn()
         try:
             soil_row = safe_fetchone(conn,
                 "SELECT * FROM sensor_readings WHERE sensor_id=2 ORDER BY timestamp DESC LIMIT 1"
             )
             river_row = safe_fetchone(conn,
                 "SELECT * FROM sensor_readings WHERE sensor_id=3 ORDER BY timestamp DESC LIMIT 1"
             )
         finally:
             conn.close()

         data = {
             "soil": soil_row["soil"] if soil_row and soil_row["soil"] is not None else 0,
             "temp": soil_row["temp"] if soil_row and soil_row["temp"] is not None else 0,
             "hum": soil_row["hum"] if soil_row and soil_row["hum"] is not None else 0,
             "rain": soil_row["rain"] if soil_row and soil_row["rain"] is not None else 0,
             "total_rain": soil_row["total_daily_rain"] if soil_row and soil_row["total_daily_rain"] is not None else 0,
             "river": river_row["river"] if river_row and river_row["river"] is not None else 0,
             "alert_level": soil_row["alert_level"] if soil_row and "alert_level" in soil_row.keys() else "normal"
         }
         return jsonify(data)

     except Exception as e:
         print(traceback.format_exc())
         return jsonify({"error": str(e)}), 500



@app.route('/api/forecast/today')
def forecast_today():
    try:
        today_str = datetime.now().strftime('%Y-%m-%d')
        conn = get_conn()
        try:
            forecast_row = safe_fetchone(conn,
                "SELECT * FROM forecast WHERE date = ?",
                (today_str,)
            )
        finally:
            conn.close()

        if not forecast_row:
            return jsonify({"forecast": {}, "rain_intensity": "None", "error": "No forecast found"}), 404

        data = {
            "forecast": {
                "min_temp": forecast_row["min_temp"],
                "max_temp": forecast_row["max_temp"],
                "has_precip": forecast_row["has_precip"],
                "precip_prob": forecast_row["precip_prob"],
                "precip_intensity": forecast_row["precip_intensity"]
            },
        }
        return jsonify(data)

    except Exception as e:
        print(traceback.format_exc())
        return jsonify({"forecast": {}, "rain_intensity": "None", "error": str(e)}), 500



if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)