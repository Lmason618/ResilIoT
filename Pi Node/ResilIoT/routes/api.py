from flask import Blueprint, jsonify
from routes.auth import login_required
from datetime import datetime, timedelta
import sqlite3
import traceback
from collections import defaultdict

api_bp = Blueprint('api', __name__)
DB_PATH = './db/sensor_data.db'

# Helpers
def get_conn():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

def safe_fetchone(conn, query, params=()):
    try:
        return conn.execute(query, params).fetchone()
    except Exception as e:
        print(f"[safe_fetchone] Error: {e}")
        return None

# /latest
@api_bp.route('/latest')
@login_required
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

# /forecast/today
@api_bp.route('/forecast/today')
@login_required
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


#/historic/<period_range>
@api_bp.route('/historic/<period_range>')
@login_required
def historic(period_range):
    try:
        now = datetime.now()
        conn = get_conn()
        try:
            if period_range == 'day':
                since = datetime(now.year, now.month, now.day)
            elif period_range == 'week':
                since = now - timedelta(days=6)
            elif period_range == 'month':
                since = now - timedelta(days=30)
            elif period_range == 'year':
                since = datetime(now.year, 1, 1)
            else:
                return jsonify({"error": "Invalid range"}), 400

            rows = conn.execute(
                "SELECT timestamp, soil, temp, hum, rain, total_daily_rain, river "
                "FROM sensor_readings "
                "WHERE timestamp >= ? "
                "ORDER BY timestamp ASC",
                (since.strftime('%Y-%m-%d %H:%M:%S'),)
            ).fetchall()
        finally:
            conn.close()

        merged = defaultdict(dict)
        for r in rows:
            ts = r['timestamp']
            merged[ts].update({k: v for k, v in dict(r).items() if v is not None})

        def get_group_key(dt):
            if period_range == 'day':
                return dt.strftime('%Y-%m-%d %H:00')
            elif period_range == 'week':
                return dt.strftime('%Y-%m-%d')  # group by day
            elif period_range == 'month':
                year, week, _ = dt.isocalendar()
                return f"{year}-W{week:02d}"
            elif period_range == 'year':
                year, week, _ = dt.isocalendar()
                return f"{year}-W{week:02d}"


        agg = defaultdict(list)
        for ts_str, values in merged.items():
            dt = datetime.strptime(ts_str, '%Y-%m-%d %H:%M:%S')
            key = get_group_key(dt)
            agg[key].append(values)

        all_periods = []
        if period_range == 'day':
            all_periods = [
                (datetime(now.year, now.month, now.day) + timedelta(hours=i)).strftime('%Y-%m-%d %H:00')
                for i in range(now.hour + 1)
            ]
        elif period_range == 'week':
            # Last 7 days (today + 6 previous days)
            for i in reversed(range(7)):
                dt = now - timedelta(days=i)
                all_periods.append(dt.strftime('%Y-%m-%d'))
        elif period_range == 'month':
            start = now - timedelta(days=28)
            for i in range(4):
                week_dt = start + timedelta(days=i*7)
                year, week, _ = week_dt.isocalendar()
                all_periods.append(f"{year}-W{week:02d}")
        elif period_range == 'year':
            start_of_year = datetime(now.year, 1, 1)
            current_week = now.isocalendar()[1]
            for i in range(current_week):
                week_dt = start_of_year + timedelta(weeks=i)
                year, week, _ = week_dt.isocalendar()
                all_periods.append(f"{year}-W{week:02d}")

        labels, soil_list, temp_list, hum_list, rain_total_list, rain_max_list, river_list = [], [], [], [], [], [], []

        for key in all_periods:
            group = agg.get(key, [])
            if group:
                avg_temp = sum(g.get('temp', 0) for g in group) / len(group)
                avg_hum = sum(g.get('hum', 0) for g in group) / len(group)
                avg_rain_total = sum(g.get('total_daily_rain', 0) for g in group) / len(group)
                avg_rain = sum(g.get('rain', 0) for g in group) / len(group)
                last_river = group[-1].get('river', 0)
                avg_soil = sum(g.get('soil', 0) for g in group) / len(group)
            else:
                avg_temp = avg_hum = avg_rain_total = avg_rain = last_river = avg_soil = "NA"

            labels.append(key)
            soil_list.append(avg_soil)
            temp_list.append(avg_temp)
            hum_list.append(avg_hum)
            rain_total_list.append(avg_rain_total)
            rain_max_list.append(avg_rain)
            river_list.append(last_river)

        data = {
            "labels": labels,
            "soil": soil_list,
            "temp": temp_list,
            "hum": hum_list,
            "rain_total": rain_total_list,
            "rain_max": rain_max_list,
            "river": river_list
        }

        return jsonify(data)

    except Exception as e:
        print(traceback.format_exc())
        return jsonify({"error": str(e)}), 500


# /alert/latest
@api_bp.route('/alert/latest')
@login_required
def latest_alert():
    try:
        # Get latest sensor values
        try:
            latest_data = latest().get_json(force=True)
        except Exception as e:
            print(f"[latest_alert] Error fetching latest data: {e}")
            latest_data = {}

        soil = latest_data.get("soil", 0)
        river = latest_data.get("river", 0)
        high_level_alert = latest_data.get("high_level_alert", 0)

        # Get today's forecast
        try:
            forecast_data = forecast_today().get_json(force=True)
        except Exception as e:
            print(f"[latest_alert] Error fetching forecast data: {e}")
            forecast_data = {}

        rain_intensity = (forecast_data.get("forecast", {}).get("precip_intensity") or "none").lower()

        # Alert Logic
        alert = "None"
        if river > 2.5 or high_level_alert == 1:
            alert = "High"
        if 20 <= soil <= 80:
            if rain_intensity in ["none", "light"]:
                alert = "None"
            elif rain_intensity in ["moderate", "heavy"]:
                alert = "Low"
        elif soil < 20 or soil > 80:
            if rain_intensity == "moderate" and (river > 2 or soil < 20 or soil > 80):
                alert = "Mid"
            if rain_intensity == "heavy" or high_level_alert == 1:
                alert = "High"

        return jsonify({"level": alert})

    except Exception as e:
        print(traceback.format_exc())
        return jsonify({"level": "No data", "error": str(e)}), 500