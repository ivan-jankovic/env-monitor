import os

from flask import Flask
from werkzeug.exceptions import HTTPException

from . import db, limits, readings


def create_app(config=None):
    app = Flask(__name__)
    app.config["DATABASE_URL"] = os.environ.get("DATABASE_URL")
    app.config["API_TOKEN"] = os.environ.get("API_TOKEN")
    app.config["MAX_CONTENT_LENGTH"] = 1024 * 1024  # a full batch is well under 100 KB
    if config:
        app.config.update(config)
    for key in ("DATABASE_URL", "API_TOKEN"):
        if not app.config[key]:
            raise RuntimeError(f"{key} is not set")

    db.init_app(app)
    app.register_blueprint(readings.bp)
    app.register_blueprint(limits.bp)

    @app.errorhandler(HTTPException)
    def http_error(e):
        # JSON instead of Flask's default HTML error pages
        return {"error": e.description}, e.code

    @app.get("/api/health")
    def health():
        return {"status": "ok"}

    return app
