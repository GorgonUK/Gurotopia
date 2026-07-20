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
