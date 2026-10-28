import os

from flask import Flask


def create_app(config=None):
    app = Flask(__name__)
    app.config["DATABASE_URL"] = os.environ.get("DATABASE_URL")
    app.config["API_TOKEN"] = os.environ.get("API_TOKEN")
    if config:
        app.config.update(config)

    @app.get("/api/health")
    def health():
        return {"status": "ok"}

    return app
