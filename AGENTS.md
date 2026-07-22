# Gurotopia - Agent Guidelines

## Project Overview

Gurotopia is a Growtopia private server. Protocol work is driven by captures from the official client via GTProxy.

**Capture → implement workflow:** `c:\VS_Repos\captures\`  
**Implement agent prompt:** `c:\VS_Repos\captures\agents\IMPLEMENT.md`

## Protocol layers

Traffic is dispatched by raw message type, then by string/hex maps (no typed packet registry like GTProxy).

| Layer | Role | Entry |
|-------|------|-------|
| ENet events | Connect / disconnect / receive | `include/event_type/` |
| Text actions | Client → server `action\|…`, login | `include/action/` → `action_pool` in `__action.cpp` |
| Dialog returns | Nested under `action\|dialog_return` | `include/action/dialog_return/` → `__dialog_return.cpp` |
| Tank / game packets | Binary `::state` (type 4) | `include/state/` → `state_pool` in `__states.cpp` |
| Server → client calls | VariantList (`On*`, dialogs) | `include/on/` + `send_varlist` |
| Chat commands | `/…` via `action\|input` | `include/commands/` |
| HTTPS bootstrap | `server_data.php` | `include/https/` |

**Receive path** (`include/event_type/receive.cpp`):
- Byte `2`/`3` → pipe-delimited text → `action_pool`
- Byte `4` → `get_state()` → `state_pool[state.type]`

Unknown action keys / tank types are **silently ignored** — always register new handlers.

## Implementing from a capture

1. Open the feature folder under `c:\VS_Repos\captures\` (`brief.md` + `sequence.md`).
2. Classify each message (action / dialog / tank / variant).
3. Add or update the handler; register in the matching pool.
4. Reply with the same shape as official (`send_varlist`, `create_dialog`, `send_action`, tank helpers).
5. Verify through GTProxy using `GTProxy/run/config.gurotopia.json`.

### Handler patterns

- Text: `readch` / `hPipe` for pipe key-values.
- Tank: `::state` fields in `include/database/peer.hpp` (`type`, `netid`, `id`, `pos`, …).
- Dialogs: `create_dialog` → `OnDialogRequest`.
- World/map: often hand-built bytes (see `join_request.cpp`) — match captures carefully.

## Login / HTTPS notes

- Local TLS on 443; `POST /growtopia/server_data.php`.
- Custom `loginurl` + base64 `ltoken` (not full Ubisoft OAuth).
- Game UDP port from `server_data` (often `17091`).
- Examples: `server_data.php.example`, `deploy/server_data.php.example`.

## Current inbound tank coverage

Registered in `__states.cpp`: `0x00`, `0x03`, `0x07`, `0x0a`, `0x0b`, `0x15`, `0x1a`.  
New tank types from captures need a new handler + pool entry.

## Code style

Match existing files in the same folder (includes, naming, `pch.hpp`). Prefer minimal diffs focused on the feature brief.

**Before adding code, check `ARCHITECTURE.md`** for the module map and the reusable-helper index —
reuse `peer_of`, `current_world`, `peer_by_netid/uid`, `inventory_count/has`, `give_to_backpack`,
`spill_drops`, `in_bounds`, the `packet::` protocol constants, and the `create_dialog` builder rather
than re-deriving them. `database/world*.cpp` is split by concern (runtime ops / serialize / tile_lock /
world_gen) — put new world code in the matching file. Refactor status lives in `REFACTOR.md`.

## Cursor Cloud specific instructions

Ubuntu 24.04 + GCC 13. Build deps (`build-essential libssl-dev libmariadb-dev pkg-config`) and
`mariadb-server` are installed by the startup update script; ENet is a vendored static lib in-repo.

- **Build / lint / test:** `make -j$(nproc)` → produces `main.out` at the repo root. This is the only
  automated check — there is **no unit-test suite and no runnable local linter** (static analysis is
  hosted Codacy; CI just builds on push). Standard build/run commands live in `README.md`.
- **Run:** `sudo ./main.out` from the repo root. It **must** run as root (binds TCP 443) and from the
  repo root (it resolves `items.dat`, `resources/`, and the cfgs relative to `cwd`). Stop with Ctrl+C
  (graceful — releases 443 + 17091). Ports: TCP 443 (HTTPS `server_data.php` bootstrap, in-process
  thread) and UDP 17091 (ENet game traffic).
- **MariaDB:** no systemd here — start it with `sudo mariadbd --user=mysql` (datadir `/var/lib/mysql`
  is already initialized). Dev DB user is `gurotopia` / `change_me` (ALL privileges). The server
  auto-creates the `gurotopia` database and all tables on connect, so no manual migrations.
- **Required runtime files (all gitignored, must exist or the server won't run):** `database.cfg`,
  `server.cfg`, `server_data.php`, and `items.dat`.
  - **`items.dat` is proprietary Growtopia data and is NOT in the repo.** `decode_items()` calls
    `std::filesystem::file_size("items.dat")` with no guard, so a missing file throws and aborts the
    whole process. A compatible file (parser supports up to version `0x1a`) must be supplied. During
    setup one was fetched from the public `StileDevs/grow-items` repo (`assets/items.dat`, v0x1a).
  - **Config gotcha:** cfgs are parsed **positionally by pipe index**, not by key. Do **not** copy the
    commented `database.cfg.example` / `server.cfg.example` verbatim — a comment line without a `|`
    shifts every field and breaks parsing. Write bare `key|value` lines only. (`server_data.php` keeps
    its `#maint|...` line precisely because that line contains a `|` and is counted in the layout.)
- **Quick "hello world" smoke test** (server running): the client bootstrap request
  `curl -sk -X POST https://127.0.0.1/growtopia/server_data.php --data protocol=216` returns the
  pipe-delimited `server_data` payload (HTTP 200) over TLS.
- **Logs:** stdout is block-buffered through pipes; prefix with `stdbuf -oL -eL` to watch startup lines
  (`connected to MariaDB…`, `listening on…`, `items.dat parsed successfully!`) live.
- Full client login/gameplay (ENet 17091) needs the real Growtopia client or GTProxy pointed at this
  host (hosts-file override) plus an external GTLogin deployment for auth — not reproducible headless.
