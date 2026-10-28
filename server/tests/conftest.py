import os
from pathlib import Path

import psycopg
import pytest

from app import create_app

TOKEN = "test-token"
# The throwaway Postgres from docker-compose.test.yml, never the real database.
TEST_DATABASE_URL = os.environ.get(
    "TEST_DATABASE_URL",
    "postgresql://monitor_test:monitor_test@localhost:5433/monitor_test",
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


@pytest.fixture
def db():
    """A fresh schema from init.sql for every test."""
    try:
        conn = psycopg.connect(TEST_DATABASE_URL, autocommit=True, connect_timeout=3)
    except psycopg.OperationalError as e:
        pytest.fail(
            "cannot reach the test database, run"
            f" `docker compose -f docker-compose.test.yml up -d --wait` first: {e}"
        )
    with conn:
        conn.execute("DROP SCHEMA public CASCADE; CREATE SCHEMA public;")
        conn.execute(INIT_SQL.read_text())
        yield conn
