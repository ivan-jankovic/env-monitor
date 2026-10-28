import os
from pathlib import Path

import psycopg
import pytest
from psycopg import sql

from app import create_app

TOKEN = "test-token"
# A separate database in the Compose Postgres, so tests never touch real data.
TEST_DATABASE_URL = os.environ.get(
    "TEST_DATABASE_URL", "postgresql://monitor:changeme@localhost:5432/monitor_test"
)
INIT_SQL = Path(__file__).parent.parent / "db" / "init.sql"


@pytest.fixture
def app():
    return create_app(
        {"TESTING": True, "API_TOKEN": TOKEN, "DATABASE_URL": TEST_DATABASE_URL}
    )


@pytest.fixture
def client(app):
    return app.test_client()


@pytest.fixture
def auth():
    return {"Authorization": f"Bearer {TOKEN}"}


@pytest.fixture(scope="session")
def test_database():
    """Create the test database once if it does not exist yet."""
    dbname = psycopg.conninfo.conninfo_to_dict(TEST_DATABASE_URL)["dbname"]
    admin_url = psycopg.conninfo.make_conninfo(TEST_DATABASE_URL, dbname="postgres")
    try:
        conn = psycopg.connect(admin_url, autocommit=True, connect_timeout=3)
    except psycopg.OperationalError as e:
        pytest.fail(f"cannot reach Postgres, run `docker compose up -d db` first: {e}")
    with conn:
        exists = conn.execute(
            "SELECT 1 FROM pg_database WHERE datname = %s", (dbname,)
        ).fetchone()
        if not exists:
            conn.execute(sql.SQL("CREATE DATABASE {}").format(sql.Identifier(dbname)))


@pytest.fixture
def db(test_database):
    """A fresh schema from init.sql for every test."""
    with psycopg.connect(TEST_DATABASE_URL, autocommit=True) as conn:
        conn.execute("DROP SCHEMA public CASCADE; CREATE SCHEMA public;")
        conn.execute(INIT_SQL.read_text())
        yield conn
