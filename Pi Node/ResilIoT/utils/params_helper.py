import os
import json

THRESHOLDS_FILE = os.path.join(os.path.dirname(__file__), "..", "db", "thresholds.json")

def load_thresholds():
    try:
        with open(THRESHOLDS_FILE, "r") as f:
            return json.load(f)
    except FileNotFoundError:
        return {
            "Low": {"river": 2.0, "soil_min": 20, "soil_max": 80},
            "Mid": {"river": 2.2, "soil_min": 15, "soil_max": 85},
            "High": {"river": 2.5, "soil_min": 10, "soil_max": 90}
        }

def save_thresholds(thresholds):
    with open(THRESHOLDS_FILE, "w") as f:
        json.dump(thresholds, f, indent=2)
