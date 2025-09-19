import unittest
from unittest.mock import patch
import sqlite3
from datetime import datetime


# Unit test patch for login required in api.py and sqlite3.connect database to avoid HTTP 302 redirects.

patch_login = patch("routes.auth.login_required", lambda x: x)
patch_login.start()

# Shared in-memory sqlite connection for all tests
in_memory_conn = sqlite3.connect(":memory:")
in_memory_conn.row_factory = sqlite3.Row

# Patch routes.api.sqlite3.connect to always return the in-memory DB
patch_db = patch("routes.api.sqlite3.connect", return_value=in_memory_conn)
patch_db.start()


from app import create_app
import routes.api


class DashboardApiTestCase(unittest.TestCase):
    """ Tests run against a shared in-memory SQLite DB. """

    @classmethod
    def setUpClass(cls):
        """Run once for the test suite: create Flask client and DB schema."""
        cls.app = create_app()
        cls.app.config['TESTING'] = True
        cls.client = cls.app.test_client()

        # Use shared in-memory DB
        cls.conn = in_memory_conn
        cls.cursor = cls.conn.cursor()

        # Create tables
        cls._create_tables_static()

        # Patch thresholds to avoid reading external JSON
        fake_thresholds = {
            "Low": {"river_max": 100, "soil_min": 10, "soil_max": 50},
            "Mid": {"river_max": 200, "soil_min": 5, "soil_max": 60},
            "High": {"river_max": 300, "soil_min": 0, "soil_max": 70},
        }
        patch_thresholds = patch("routes.api.load_thresholds", return_value=fake_thresholds)
        patch_thresholds.start()
        cls._thresholds_patch = patch_thresholds

    @classmethod
    def tearDownClass(cls):
        """Close the shared DB and stop patches."""
        cls._thresholds_patch.stop()
        cls.conn.close()
        patch_login.stop()
        patch_db.stop()

    @classmethod
    def _create_tables_static(cls):
        """Create DB tables used by the API."""
        cls.cursor.execute("""
            CREATE TABLE IF NOT EXISTS sensor_readings (
                timestamp TEXT,
                sensor_id INTEGER,
                soil REAL,
                temp REAL,
                hum REAL,
                rain REAL,
                total_daily_rain REAL,
                river REAL,
                alert_level TEXT
            )
        """)
        cls.cursor.execute("""
            CREATE TABLE IF NOT EXISTS forecast (
                date TEXT,
                min_temp REAL,
                max_temp REAL,
                has_precip INTEGER,
                precip_prob REAL,
                precip_intensity TEXT
            )
        """)
        cls.conn.commit()

    # ----------------------
    # Tests
    # ----------------------

    def test_latest_empty_db(self):
        """
        GET /api/latest with no sensor data.
        Expected: returns 200 and defaults (soil=0, river=0).
        """
        response = self.client.get("/api/latest")
        self.assertEqual(response.status_code, 200)

        data = response.get_json()
        self.assertEqual(data["soil"], 0)
        self.assertEqual(data["river"], 0)

    def test_forecast_today_no_data(self):
        """
        GET /api/forecast/today with no forecast data.
        Expected: returns 404.
        """
        response = self.client.get("/api/forecast/today")
        self.assertEqual(response.status_code, 404)

    def test_alert_latest_high(self):
        """
        GET /api/alert/latest after inserting a high river reading.
        Expected: returns 200 with alert level "High".
        """
        # Insert high river reading
        self.cursor.execute(
            "INSERT INTO sensor_readings (timestamp, sensor_id, river) VALUES (?, ?, ?)",
            (datetime.now().strftime("%Y-%m-%d %H:%M:%S"), 3, 999)
        )
        self.conn.commit()

        response = self.client.get("/api/alert/latest")
        self.assertEqual(response.status_code, 200)

        data = response.get_json()
        self.assertEqual(data["level"], "High")


if __name__ == "__main__":
    unittest.main()
