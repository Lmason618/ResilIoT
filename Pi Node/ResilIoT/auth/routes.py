from flask import Blueprint, render_template, request, redirect, url_for, session, flash
import bcrypt
from utils.db_helpers import get_user_conn

auth_bp = Blueprint('auth', __name__)

@auth_bp.route('/register', methods=['GET', 'POST'])
def register():
    error = None
    if request.method == 'POST':
        username = request.form['username'].strip()
        password = request.form['password']
        confirm = request.form['confirm']

        if not username or not password or not confirm:
            error = 'All fields are required.'
        elif password != confirm:
            error = 'Passwords do not match.'
        else:
            hashed_pw = bcrypt.hashpw(password.encode('utf-8'), bcrypt.gensalt())
            try:
                conn = get_user_conn()
                conn.execute("INSERT INTO users (username, password) VALUES (?, ?)",
                             (username, hashed_pw))
                conn.commit()
                conn.close()
                flash('Registration successful. Please log in.')
                return redirect(url_for('auth.login'))
            except Exception:
                error = 'Username already exists.'
    return render_template('register.html', error=error)

@auth_bp.route('/login', methods=['GET', 'POST'])
def login():
    error = None
    if request.method == 'POST':
        username = request.form['username'].strip()
        password = request.form['password']
        conn = get_user_conn()
        user = conn.execute("SELECT * FROM users WHERE username = ?", (username,)).fetchone()
        conn.close()

        if user and bcrypt.checkpw(password.encode('utf-8'), user['password']):
            session['user_id'] = user['id']
            session['username'] = user['username']
            return redirect(url_for('dashboard.index'))
        else:
            error = 'Invalid username or password.'
    return render_template('login.html', error=error)

@auth_bp.route('/logout')
def logout():
    session.clear()
    return redirect(url_for('auth.login'))

