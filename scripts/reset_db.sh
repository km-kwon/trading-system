#!/usr/bin/env bash
set -euo pipefail

if ! command -v psql >/dev/null 2>&1; then
    echo "psql command not found. Install PostgreSQL client/server in WSL first." >&2
    echo "Example: sudo apt-get install -y postgresql postgresql-client" >&2
    exit 1
fi

DB_NAME="${MINI_ATS_DB_NAME:-mini_ats}"
DB_USER="${MINI_ATS_DB_USER:-$USER}"

echo "Resetting schema mini_ats in database '${DB_NAME}' as user '${DB_USER}'"
psql -U "${DB_USER}" -d "${DB_NAME}" -c "DROP SCHEMA IF EXISTS mini_ats CASCADE;"
psql -U "${DB_USER}" -d "${DB_NAME}" -f db/schema.sql
psql -U "${DB_USER}" -d "${DB_NAME}" -f db/seed.sql
echo "Database reset complete"
