# Gurotopia — agent instructions

Persistent base context for AI/coding agents working in this repo. Prefer this over rediscovering setup from chat history.

## What this is

**Gurotopia** is a Growtopia private server (GTPS) in C++20. One process serves:

| Port | Role |
|------|------|
| TCP `443` | HTTPS `POST /growtopia/server_data.php` (self-signed cert in `resources/ctx/`) |
| UDP `17091` | ENet game + login (`OnSendToServer` reconnects to the same host) |

MariaDB stores peers/worlds. Docker Compose runs `db` + `server`.

## Architecture (code layout)

- **Action pool** — [include/action/__action.cpp](include/action/__action.cpp): string → handler (`action|join_request`, `action|wrench`, …).
- **State pool** — [include/state/__states.cpp](include/state/__states.cpp): game-packet type → handler (movement `0x00`, tile_change `0x03`, …).
- **Dialog-return pool** — [include/action/dialog_return/__dialog_return.cpp](include/action/dialog_return/__dialog_return.cpp): `dialog_name` → handler.
- **Dialog builder** — [include/tools/create_dialog.cpp](include/tools/create_dialog.cpp).
- **Models** — [include/database/peer.hpp](include/database/peer.hpp), [include/database/world.hpp](include/database/world.hpp).
- **Broadcast** — `peers(world, PEER_SAME_WORLD, …)` in [include/database/peer.cpp](include/database/peer.cpp). Always null-check / skip `peer.data == nullptr` (ENet can mark CONNECTED before `_connect` assigns data).

One file per handler; register in the matching pool. Keep hot paths (especially movement) cheap.

## Build / run

**Local (Windows MSYS2 / Linux):** see [README.md](README.md). `make -j$(nproc)` → `./main.out`.

**Docker (preferred for deploy):**

```bash
# runtime files (gitignored) must exist under deploy/
docker compose up -d --build
```

Runtime mounts: `deploy/{database.cfg,server.cfg,server_data.php,items.dat}`, `resources/`, and writable `logs/` for action logs.

## VPS deploy workflow

Prod VPS details and SSH key live in the local **gitignored** [handoff/](handoff/) bundle (do not commit secrets).

Typical loop after editing C++:

```powershell
scp -i "$env:USERPROFILE\.ssh\gurotopia_vps" .\path\to\file.cpp `
  root@VPS_IP:/opt/gurotopia/path/to/file.cpp
ssh -i "$env:USERPROFILE\.ssh\gurotopia_vps" root@VPS_IP `
  "cd /opt/gurotopia && docker compose up -d --build server"
```

Or `git push` then on VPS: `cd /opt/gurotopia && git pull && docker compose up -d --build server`.

Checks:

```bash
docker compose logs server --tail 50
docker inspect gurotopia-server-1 --format '{{.RestartCount}} {{.State.OOMKilled}}'
```

Rising `RestartCount` ⇒ crash loop. Action logs: `./logs/gurotopia-YYYY-MM-DD.log` on the host (bind-mounted).

## Conventions

- **Implement → deploy → test → keep only if it works.** Do not commit/push a feature until the checklist passes on a real client; revert failing work in the tree.
- Strip trailing `\r` when parsing text configs (Windows CRLF on Linux).
- Never log or do heavy I/O inside movement / per-tick packet paths (VPS is often **1 CPU core**).
- Prefer discrete event logs (login, world join, place/break, drop/trash/buy, chat).
- Author commits as repo owner when asked (`GorgonUK` / configured email). Never update git config permanently unless requested.
- Do not commit `deploy/*.cfg`, `items.dat`, `handoff/`, or credentials.

## Client connect (phone)

Shadowrocket (or hosts) must map `www.growtopia1.com` / `www.growtopia2.com` to the server IP. Use a clean config **without** `update-url` (subscriptions override `[Host]`). Login page: `loginurl` in `server_data.php` (e.g. GTLogin on Vercel).

## Known pitfalls

1. HTTPS handler must drain full request (headers + body) before close, or TCP RST drops the response → client hangs on “Getting server address”.
2. `peers()` must skip null `peer.data`.
3. Bound-check pipe-parsed packets (wrench, etc.) — OOB `operator[]` segfaults the process.
4. `world.access` is `std::array<int,20>`; access already gates build in `tile_change` and blue name in `join_request`.
