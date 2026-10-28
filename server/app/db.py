import psycopg
from flask import current_app, g


def get_db():
    """One connection per request, opened on first use."""
    if "db" not in g:
        g.db = psycopg.connect(current_app.config["DATABASE_URL"], autocommit=True)
    return g.db


def close_db(exc=None):
    conn = g.pop("db", None)
    if conn is not None:
        conn.close()


def init_app(app):
    app.teardown_appcontext(close_db)
