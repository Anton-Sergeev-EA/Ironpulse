# Automatic deployment

After every push to `main` that passes CI, GitHub Actions logs in to the server
over SSH and runs [`deploy/scripts/update.sh`](../deploy/scripts/update.sh),
which pulls the new commit, rebuilds and restarts the containers, waits for
Ironpulse to report healthy and rolls back to the previous commit if it does
not.

```
git push ──▶ CI (build, tests, sanitizers, Docker build)
                │ success on main
                ▼
             Deploy workflow ──ssh──▶ VDS: update.sh
                                          git pull (fast-forward only)
                                          docker compose up -d --build
                                          wait for "healthy" ──fail──▶ roll back
```

Code that fails CI never reaches the server, and deployments run one at a
time.

## Security model

The workflow uses a key made only for deployment. On the server that key is
restricted with `command="…"` in `authorized_keys`: whatever the client asks
for, the server runs `update.sh` and nothing else — no shell, no port
forwarding, no file transfer. A leaked key can, at worst, redeploy what is
already on `main`. The server's host key is pinned in a secret, so the runner
refuses to talk to anything pretending to be your server.

## Setup (once)

The commands below assume the repository lives in `/root/ironpulse` on the
server and you log in as `root`; adjust the path and user if not.

**1. Get the script onto the server** (the first time only, by hand):

```bash
ssh root@SERVER 'cd /root/ironpulse && git pull'
```

**2. Create a deployment key on your own computer** — no passphrase, since
GitHub Actions has to use it unattended:

```bash
ssh-keygen -t ed25519 -N "" -C ironpulse-deploy -f ~/.ssh/ironpulse_deploy
```

**3. Authorise it on the server, restricted to the update script:**

```bash
ssh root@SERVER "printf '%s %s\n' 'command=\"/root/ironpulse/deploy/scripts/update.sh\",no-port-forwarding,no-X11-forwarding,no-agent-forwarding,no-pty' '$(cat ~/.ssh/ironpulse_deploy.pub)' >> ~/.ssh/authorized_keys"
```

Check it: this must print the update log ("Already at … nothing to do"), not
open a shell:

```bash
ssh -i ~/.ssh/ironpulse_deploy root@SERVER
```

**4. Add the secrets to GitHub** — repository → *Settings* → *Secrets and
variables* → *Actions* → *New repository secret*:

| Secret            | Value                                                                    |
|-------------------|--------------------------------------------------------------------------|
| `VDS_HOST`        | the server's IP address or host name                                     |
| `VDS_USER`        | `root` (optional, that is the default)                                   |
| `VDS_PORT`        | the SSH port, only if it is not 22                                       |
| `VDS_SSH_KEY`     | the whole private key: output of `cat ~/.ssh/ironpulse_deploy`           |
| `VDS_KNOWN_HOSTS` | output of `ssh-keyscan SERVER` (add `-p PORT` for a non-standard port)   |

With the GitHub CLI the same is:

```bash
gh secret set VDS_HOST --body "SERVER"
gh secret set VDS_SSH_KEY < ~/.ssh/ironpulse_deploy
ssh-keyscan SERVER 2>/dev/null | gh secret set VDS_KNOWN_HOSTS
```

**5. Try it:** *Actions* → *Deploy* → *Run workflow*. Tick *force* to rebuild
even when the server is already up to date.

Until `VDS_HOST` is set the Deploy workflow skips itself, so nothing happens in
forks or before setup.

## Everyday use

- Push to `main` (or merge a pull request). About ten minutes later — CI plus
  the build on the server — the new version is live.
- Each run's output is in the *Deploy* workflow on GitHub and appended to
  `/var/log/ironpulse-deploy.log` on the server.
- To deploy by hand: `/root/ironpulse/deploy/scripts/update.sh` on the server
  (`--force` to rebuild without new commits).

## When a deployment fails

| Message | Meaning and fix |
|---|---|
| `Rolled back; the previous version is running again` | The new version did not become healthy within 3 minutes. Its last log lines are printed above; fix the problem and push again. |
| `Tracked files were changed on this server` | Someone edited a file from the repository directly on the server. Look at `git diff` there, then keep the change in git or discard it with `git checkout -- .`. Your `.env` is not tracked and is never touched. |
| `cannot be fast-forwarded` | The history of `main` was rewritten on GitHub. On the server: `git fetch && git reset --hard origin/main`, then deploy again. |
| `Permission denied (publickey)` in the workflow | `VDS_SSH_KEY` does not match the key added in step 3, or `VDS_USER` is wrong. |
| `Host key verification failed` | `VDS_KNOWN_HOSTS` is missing or outdated (e.g. the server was reinstalled); run `ssh-keyscan` again and update the secret. |

## Settings

`update.sh` reads these environment variables; the defaults match the
deployment behind an existing nginx described in the README.

| Variable | Default |
|---|---|
| `IRONPULSE_COMPOSE_FILES` | `docker-compose.yml docker-compose.prod.yml` |
| `IRONPULSE_BRANCH` | `main` |
| `IRONPULSE_HEALTH_TIMEOUT` | `180` seconds |
| `IRONPULSE_DEPLOY_LOG` | `/var/log/ironpulse-deploy.log` |

To set them for automatic runs, put them in the forced command, e.g.
`command="IRONPULSE_COMPOSE_FILES=docker-compose.yml /root/ironpulse/deploy/scripts/update.sh"`.
