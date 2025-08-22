from flask import Blueprint, render_template
from routes.auth import login_required
from utils.params_helper import load_thresholds

dynaminsert_bp = Blueprint("dynaminsert", __name__)

@dynaminsert_bp.route("/")
@login_required
def index():
    return render_template("index.html")

@dynaminsert_bp.route("/home.html")
@login_required
def home_fragment():
    return render_template("home.html")

@dynaminsert_bp.route("/params.html")
@login_required
def params_fragment():
    thresholds = load_thresholds()
    return render_template("params.html", thresholds=thresholds)

