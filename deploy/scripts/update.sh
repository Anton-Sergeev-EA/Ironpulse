#!/usr/bin/env bash
# Updates a Docker Compose deployment of Ironpulse to the latest main and
# restarts it, rolling back to the previous commit if the new version does
# not become healthy.
#
#   deploy/scripts/update.sh            update if main has moved
#   deploy/scripts/update.sh --force    rebuild and restart even if not
#
# Run it by hand, or let the "Deploy" GitHub Actions workflow run it over
# SSH after CI passes (docs/auto-deploy.md). Settings come from the
# environment, with defaults matching a deployment behind a host nginx:
#
#   IRONPULSE_COMPOSE_FILES   compose files, space-separated
#                             (default: "docker-compose.yml docker-compose.prod.yml")
#   IRONPULSE_BRANCH          branch to follow (default: main)
#   IRONPULSE_HEALTH_TIMEOUT  seconds to wait for "healthy" (default: 180)
#   IRONPULSE_DEPLOY_LOG      log file (default: /var/log/ironpulse-deploy.log,
#                             or ./deploy.log if that is not writable)

set -Eeuo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "${script_dir}/../.." && pwd)"
compose_dir="${repo_dir}/deploy/docker"
branch="${IRONPULSE_BRANCH:-main}"
health_timeout="${IRONPULSE_HEALTH_TIMEOUT:-180}"
read -r -a compose_files <<<"${IRONPULSE_COMPOSE_FILES:-docker-compose.yml docker-compose.prod.yml}"
container="ironpulse-app"

force=false
# --force on the command line, or "force" sent by the Deploy workflow (with
# a forced-command SSH key the requested command arrives in this variable).
if [[ "${1:-}" == "--force" || "${SSH_ORIGINAL_COMMAND:-}" == "force" ]]; then
    force=true
fi


log() { printf '[%s] %s\n' "$(date -u '+%Y-%m-%d %H:%M:%S UTC')" "$*"; }

compose() {
    local args=()
    local f
    for f in "${compose_files[@]}"; do
        args+=(-f "${f}")
    done
    (cd "${compose_dir}" && docker compose "${args[@]}" "$@")
}

# Waits until the engine container reports healthy; fails on timeout or if
# it exits.
wait_healthy() {
    local deadline=$((SECONDS + health_timeout))
    local status
    while ((SECONDS < deadline)); do
        status="$(docker inspect --format '{{if .State.Health}}{{.State.Health.Status}}{{else}}{{.State.Status}}{{end}}' \
            "${container}" 2>/dev/null || echo missing)"
        case "${status}" in
        healthy) return 0 ;;
        exited | dead)
            log "${container} stopped (${status})"
            return 1
            ;;
        *) ;; # starting, unhealthy (may still recover), restarting, missing
        esac
        sleep 3
    done
    log "${container} is not healthy after ${health_timeout}s (last status: ${status:-unknown})"
    return 1
}

main() {
    # Only one deployment at a time: a second push during a build waits here.
    exec 9>"${repo_dir}/.git/ironpulse-deploy.lock"
    if ! flock -w 900 9; then
        log "Another deployment has been running for 15 minutes; giving up"
        exit 1
    fi

    cd "${repo_dir}"
    log "Checking ${branch} in ${repo_dir}"
    git fetch --quiet origin "${branch}"

    previous="$(git rev-parse HEAD)"
    target="$(git rev-parse "origin/${branch}")"

    if [[ "${previous}" == "${target}" && "${force}" == false ]]; then
        log "Already at $(git log -1 --format='%h %s' "${target}"); nothing to do"
        exit 0
    fi

    if [[ -n "$(git status --porcelain --untracked-files=no)" ]]; then
        log "Tracked files were changed on this server; refusing to overwrite them:"
        git status --short --untracked-files=no
        log "Inspect with 'git diff', then 'git stash' or 'git checkout -- .' and deploy again"
        exit 1
    fi

    if [[ "${previous}" != "${target}" ]]; then
        log "Updating $(git log -1 --format='%h' "${previous}") -> $(git log -1 --format='%h %s' "${target}")"
        if ! git merge --ff-only --quiet "origin/${branch}"; then
            log "${branch} cannot be fast-forwarded (history was rewritten on GitHub?); not deploying"
            exit 1
        fi
    fi

    log "Building and restarting"
    if compose up -d --build && wait_healthy; then
        log "Deployed $(git log -1 --format='%h %s')"
        # Old images are useless once the new one runs; build cache older than a
        # week is dropped too, so the disk does not fill up over many deploys.
        docker image prune -f >/dev/null || true
        docker builder prune -f --filter until=168h >/dev/null || true
        exit 0
    fi

    log "New version failed; last log lines of ${container}:"
    docker logs --tail 30 "${container}" 2>&1 || true

    if [[ "${previous}" == "$(git rev-parse HEAD)" ]]; then
        log "Nothing to roll back to (forced rebuild of the same commit)"
        exit 1
    fi

    log "Rolling back to $(git log -1 --format='%h %s' "${previous}")"
    git reset --hard --quiet "${previous}"
    if compose up -d --build && wait_healthy; then
        log "Rolled back; the previous version is running again"
    else
        log "Rollback did not become healthy either — check 'docker compose ps' and the logs"
    fi
    exit 1
}

log_file="${IRONPULSE_DEPLOY_LOG:-/var/log/ironpulse-deploy.log}"
if ! { : >>"${log_file}"; } 2>/dev/null; then
    log_file="${repo_dir}/deploy.log"
fi
# A plain pipe rather than exec > >(tee ...): the script then exits only
# after every line has reached both the log and the caller (e.g. the
# GitHub Actions log), so the final verdict is never cut off.
main "$@" 2>&1 | tee -a "${log_file}"
exit "${PIPESTATUS[0]}"
