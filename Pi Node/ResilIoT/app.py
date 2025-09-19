# app.py
from flask import Flask
from routes.auth import auth_bp
from routes.dashboard import dashboard_bp
from routes.api import api_bp
from routes.dynaminsert import dynaminsert_bp
from utils.db_helpers import init_user_db

def create_app(test_config=None):
    """
    Flask application factory.
    :param test_config: optional dict to override config (useful for testing)
    :return: Flask app
    """
    app = Flask(__name__)
    app.secret_key = 'supersecretkey'

    # Apply test config overrides
    if test_config:
        app.config.update(test_config)

    # Register blueprints
    app.register_blueprint(auth_bp)
    app.register_blueprint(dashboard_bp)
    app.register_blueprint(api_bp, url_prefix='/api')
    app.register_blueprint(dynaminsert_bp)

    # Initialize user database (skip in tests)
    if not app.config.get("TESTING", False):
        init_user_db()

    return app


# Only run when executing the file directly
if __name__ == "__main__":
    app = create_app()
    app.run(host="0.0.0.0", port=5000, debug=True)
