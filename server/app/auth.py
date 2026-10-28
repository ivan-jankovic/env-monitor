import hmac
from functools import wraps

from flask import current_app, request


def require_token(view):
    """Reject the request unless it carries `Authorization: Bearer <API_TOKEN>`."""

    @wraps(view)
    def wrapped(*args, **kwargs):
        scheme, _, token = request.headers.get("Authorization", "").partition(" ")
        expected = current_app.config["API_TOKEN"]
        # compare_digest avoids leaking the token through response timing
        if scheme != "Bearer" or not hmac.compare_digest(token.encode(), expected.encode()):
            return (
                {"error": "missing or invalid bearer token"},
                401,
                {"WWW-Authenticate": "Bearer"},
            )
        return view(*args, **kwargs)

    return wrapped
