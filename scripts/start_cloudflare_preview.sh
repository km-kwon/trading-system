#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

PROJECT_NAME="${PROJECT_NAME:-mini-ats-trading-console}"
PAGES_ENV="${PAGES_ENV:-preview}"
PAGES_BRANCH="${PAGES_BRANCH:-master}"
PAGES_ALIAS="${PAGES_ALIAS:-https://master.mini-ats-trading-console.pages.dev}"
HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-8080}"
BRIDGE_ORIGIN="http://${HOST}:${PORT}"
CLOUDFLARED_BIN="${CLOUDFLARED_BIN:-${HOME}/.local/bin/cloudflared}"
RUNTIME_DIR="${RUNTIME_DIR:-${ROOT_DIR}/.runtime/cloudflare-preview}"
BRIDGE_LOG="${RUNTIME_DIR}/bridge.log"
TUNNEL_LOG="${RUNTIME_DIR}/cloudflared.log"
BRIDGE_PID_FILE="${RUNTIME_DIR}/bridge.pid"
TUNNEL_PID_FILE="${RUNTIME_DIR}/cloudflared.pid"
TUNNEL_URL_FILE="${RUNTIME_DIR}/tunnel-url.txt"
DEPLOY_CWD="${RUNTIME_DIR}/deploy-cwd"
WAIT_SECONDS="${WAIT_SECONDS:-30}"
PAGES_WAIT_SECONDS="${PAGES_WAIT_SECONDS:-60}"

STOP_FIRST=1
UPDATE_SECRET=1
DEPLOY_AFTER_SECRET=1
VERIFY_PAGES=1
STOP_ONLY=0
DEPLOY_ONLY=0

usage() {
  cat <<EOF
Usage: ./scripts/start_cloudflare_preview.sh [options]

Starts the local Mini ATS bridge/engine, creates a Cloudflare quick tunnel,
updates the Cloudflare Pages preview secret, deploys from a temporary
Wrangler workspace, and verifies the preview API.

Options:
  --no-stop      Do not stop existing bridge/tunnel processes first.
  --no-secret    Do not update ATS_BRIDGE_ORIGIN in Cloudflare Pages.
  --no-deploy    Do not redeploy Cloudflare Pages after updating the secret.
  --no-verify    Skip Cloudflare Pages preview health verification.
  --deploy-only  Reuse the last quick tunnel URL and only update/deploy/verify.
  --stop-only    Stop existing bridge/tunnel processes and exit.
  -h, --help     Show this help.

Environment overrides:
  PROJECT_NAME, PAGES_ENV, PAGES_BRANCH, PAGES_ALIAS, HOST, PORT, CLOUDFLARED_BIN,
  RUNTIME_DIR, WAIT_SECONDS, PAGES_WAIT_SECONDS
EOF
}

log() {
  printf '[cloudflare-preview] %s\n' "$*" >&2
}

fail() {
  printf '[cloudflare-preview] ERROR: %s\n' "$*" >&2
  exit 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-stop)
      STOP_FIRST=0
      ;;
    --no-secret)
      UPDATE_SECRET=0
      ;;
    --no-deploy)
      DEPLOY_AFTER_SECRET=0
      ;;
    --no-verify)
      VERIFY_PAGES=0
      ;;
    --stop-only)
      STOP_ONLY=1
      ;;
    --deploy-only)
      DEPLOY_ONLY=1
      STOP_FIRST=0
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage >&2
      fail "unknown option: $1"
      ;;
  esac
  shift
done

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    fail "required command not found: $1"
  fi
}

stop_pid_file() {
  local pid_file="$1"
  local label="$2"

  if [[ ! -f "${pid_file}" ]]; then
    return
  fi

  local pid
  pid="$(cat "${pid_file}" 2>/dev/null || true)"
  rm -f "${pid_file}"

  if [[ -z "${pid}" ]]; then
    return
  fi

  if kill -0 "${pid}" >/dev/null 2>&1; then
    log "stopping ${label} pid ${pid}"
    kill "${pid}" >/dev/null 2>&1 || true
    for _ in {1..20}; do
      if ! kill -0 "${pid}" >/dev/null 2>&1; then
        return
      fi
      sleep 0.1
    done
    kill -9 "${pid}" >/dev/null 2>&1 || true
  fi
}

stop_existing() {
  mkdir -p "${RUNTIME_DIR}"
  stop_pid_file "${TUNNEL_PID_FILE}" "cloudflared"
  stop_pid_file "${BRIDGE_PID_FILE}" "bridge"

  pkill -f "cloudflared tunnel --url ${BRIDGE_ORIGIN}" >/dev/null 2>&1 || true
  pkill -f "python3 web/bridge.py" >/dev/null 2>&1 || true
  pkill -f "build/mini_ats --tcp" >/dev/null 2>&1 || true
}

wait_for_bridge() {
  local deadline=$((SECONDS + WAIT_SECONDS))
  local health=""

  while (( SECONDS < deadline )); do
    health="$(curl -fsS "${BRIDGE_ORIGIN}/api/health" 2>/dev/null || true)"
    if grep -Eq '"engineConnected"[[:space:]]*:[[:space:]]*true' <<<"${health}"; then
      log "local bridge is healthy: ${BRIDGE_ORIGIN}"
      return
    fi

    if [[ -f "${BRIDGE_PID_FILE}" ]]; then
      local bridge_pid
      bridge_pid="$(cat "${BRIDGE_PID_FILE}")"
      if ! kill -0 "${bridge_pid}" >/dev/null 2>&1; then
        tail -n 40 "${BRIDGE_LOG}" >&2 || true
        fail "bridge exited before becoming healthy"
      fi
    fi
    sleep 0.5
  done

  tail -n 40 "${BRIDGE_LOG}" >&2 || true
  fail "bridge did not become healthy within ${WAIT_SECONDS}s"
}

wait_for_tunnel_url() {
  local deadline=$((SECONDS + WAIT_SECONDS))
  local tunnel_url=""

  while (( SECONDS < deadline )); do
    tunnel_url="$(grep -Eo 'https://[a-z0-9-]+\.trycloudflare\.com' "${TUNNEL_LOG}" 2>/dev/null | tail -n 1 || true)"
    if [[ -n "${tunnel_url}" ]]; then
      printf '%s\n' "${tunnel_url}" > "${TUNNEL_URL_FILE}"
      log "quick tunnel URL: ${tunnel_url}"
      printf '%s\n' "${tunnel_url}"
      return
    fi

    if [[ -f "${TUNNEL_PID_FILE}" ]]; then
      local tunnel_pid
      tunnel_pid="$(cat "${TUNNEL_PID_FILE}")"
      if ! kill -0 "${tunnel_pid}" >/dev/null 2>&1; then
        tail -n 60 "${TUNNEL_LOG}" >&2 || true
        fail "cloudflared exited before printing a tunnel URL"
      fi
    fi
    sleep 0.5
  done

  tail -n 60 "${TUNNEL_LOG}" >&2 || true
  fail "cloudflared did not print a tunnel URL within ${WAIT_SECONDS}s"
}

update_pages_secret() {
  local tunnel_url="$1"
  log "updating Cloudflare Pages ${PAGES_ENV} secret ATS_BRIDGE_ORIGIN"
  printf '%s\n' "${tunnel_url}" \
    | npx wrangler pages secret put ATS_BRIDGE_ORIGIN \
        --project-name "${PROJECT_NAME}" \
        --env "${PAGES_ENV}"
}

write_pages_deploy_config() {
  local tunnel_url="$1"

  rm -rf "${DEPLOY_CWD}"
  mkdir -p "${DEPLOY_CWD}"
  cp -R "${ROOT_DIR}/web" "${DEPLOY_CWD}/web"
  cp -R "${ROOT_DIR}/functions" "${DEPLOY_CWD}/functions"

  cat > "${DEPLOY_CWD}/wrangler.toml" <<EOF
name = "${PROJECT_NAME}"
compatibility_date = "2026-06-29"
pages_build_output_dir = "./web"

[vars]
ATS_BRIDGE_PROXY_ORIGIN = "${tunnel_url}"

[env.preview.vars]
ATS_BRIDGE_PROXY_ORIGIN = "${tunnel_url}"

[env.production.vars]
ATS_BRIDGE_PROXY_ORIGIN = "${tunnel_url}"
EOF
}

deploy_pages() {
  local tunnel_url="$1"

  write_pages_deploy_config "${tunnel_url}"
  log "redeploying Cloudflare Pages branch ${PAGES_BRANCH} with the current bridge origin"
  (
    cd "${DEPLOY_CWD}"
    npx wrangler pages deploy web \
      --project-name "${PROJECT_NAME}" \
      --branch "${PAGES_BRANCH}" \
      --commit-dirty=true
  )
}

verify_pages() {
  local deadline=$((SECONDS + PAGES_WAIT_SECONDS))
  local health_url="${PAGES_ALIAS%/}/api/health"
  local health=""

  log "verifying Pages preview: ${health_url}"
  while (( SECONDS < deadline )); do
    health="$(curl -fsS "${health_url}" 2>/dev/null || true)"
    if grep -Eq '"engineConnected"[[:space:]]*:[[:space:]]*true' <<<"${health}"; then
      log "Pages preview is connected: ${PAGES_ALIAS}"
      return
    fi
    sleep 2
  done

  log "warning: Pages preview did not report engineConnected=true within ${PAGES_WAIT_SECONDS}s"
  log "last health response: ${health:-<empty>}"
}

if (( DEPLOY_ONLY == 1 && STOP_ONLY == 1 )); then
  fail "--deploy-only and --stop-only cannot be used together"
fi

if (( STOP_FIRST == 1 )); then
  stop_existing
fi

if (( STOP_ONLY == 1 )); then
  log "stopped local bridge, engine, and quick tunnel processes"
  exit 0
fi

require_command curl
require_command npx

if (( DEPLOY_ONLY == 1 )); then
  if [[ ! -s "${TUNNEL_URL_FILE}" ]]; then
    fail "no saved tunnel URL at ${TUNNEL_URL_FILE}; run without --deploy-only first"
  fi

  TUNNEL_URL="$(head -n 1 "${TUNNEL_URL_FILE}")"
  if (( UPDATE_SECRET == 1 )); then
    update_pages_secret "${TUNNEL_URL}"
    if (( DEPLOY_AFTER_SECRET == 1 )); then
      deploy_pages "${TUNNEL_URL}"
    fi
  else
    log "skipping Cloudflare secret update"
    if (( DEPLOY_AFTER_SECRET == 1 )); then
      deploy_pages "${TUNNEL_URL}"
    fi
  fi

  if (( VERIFY_PAGES == 1 )); then
    verify_pages
  fi

  cat <<EOF

Mini ATS Cloudflare preview deployment was refreshed.

Quick tunnel:
  ${TUNNEL_URL}

Pages preview:
  ${PAGES_ALIAS}

EOF
  exit 0
fi

if [[ ! -x "${CLOUDFLARED_BIN}" ]]; then
  fail "cloudflared not executable at ${CLOUDFLARED_BIN}; set CLOUDFLARED_BIN=/path/to/cloudflared"
fi

if [[ ! -x "${ROOT_DIR}/build/mini_ats" ]]; then
  fail "build/mini_ats not found; run: cmake -S . -B build && cmake --build build"
fi

mkdir -p "${RUNTIME_DIR}"
rm -f "${BRIDGE_LOG}" "${TUNNEL_LOG}" "${TUNNEL_URL_FILE}"

log "starting local bridge and engine"
nohup python3 web/bridge.py --host "${HOST}" --port "${PORT}" --start-engine \
  > "${BRIDGE_LOG}" 2>&1 &
printf '%s\n' "$!" > "${BRIDGE_PID_FILE}"

wait_for_bridge

log "starting Cloudflare quick tunnel"
nohup "${CLOUDFLARED_BIN}" tunnel --url "${BRIDGE_ORIGIN}" \
  > "${TUNNEL_LOG}" 2>&1 &
printf '%s\n' "$!" > "${TUNNEL_PID_FILE}"

TUNNEL_URL="$(wait_for_tunnel_url)"

if (( UPDATE_SECRET == 1 )); then
  update_pages_secret "${TUNNEL_URL}"
  if (( DEPLOY_AFTER_SECRET == 1 )); then
    deploy_pages "${TUNNEL_URL}"
  fi
else
  log "skipping Cloudflare secret update"
fi

if (( VERIFY_PAGES == 1 )); then
  verify_pages
fi

cat <<EOF

Mini ATS Cloudflare preview is ready.

Local bridge:
  ${BRIDGE_ORIGIN}

Quick tunnel:
  ${TUNNEL_URL}

Pages preview:
  ${PAGES_ALIAS}

Logs:
  ${BRIDGE_LOG}
  ${TUNNEL_LOG}

Stop later:
  ./scripts/start_cloudflare_preview.sh --stop-only
EOF
