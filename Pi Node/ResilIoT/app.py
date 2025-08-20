from routes.auth import auth_bp
from routes.dashboard import dashboard_bp
from routes.api import api_bp

from utils.db_helpers import init_user_db

app = Flask(__name__)
app.secret_key = 'supersecretkey'

# Register bps
app.register_blueprint(auth_bp)
app.register_blueprint(dashboard_bp)
app.register_blueprint(api_bp, url_prefix='/api')

# Init user DB
init_user_db()

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
