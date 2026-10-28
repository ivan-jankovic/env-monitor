#!/usr/bin/env bash
# Run the server tests against a throwaway Postgres, then remove it.
# Usage: ./scripts/test.sh [pytest args], e.g. ./scripts/test.sh -k duplicates
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
compose=(docker compose -f "$root/docker-compose.test.yml")

# Runs on success, failure and Ctrl+C; the script still exits with pytest's code.
trap '"${compose[@]}" down >/dev/null 2>&1' EXIT

"${compose[@]}" up -d --wait
cd "$root/server"
uv run pytest "$@"
