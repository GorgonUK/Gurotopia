# Gurotopia — architecture & reusable helpers

A map of where things live and, more importantly, **the shared helpers to reuse instead of
re-deriving them**. If you (or an AI assistant) are about to write a `static_cast<::peer*>`,
hand-format a dialog string, hardcode a packet-type byte, or copy a loop that already exists —
check this file first. Adding a duplicate is the main way this codebase rots.

> New handlers are dispatched from "pools" (hash maps keyed by message type/name). Unknown
> keys are silently ignored, so **every new handler must be registered in its pool** — see the
> table below.

## Module map

| Path | Responsibility | Dispatch pool / entry |
|------|----------------|-----------------------|
| `main.cpp` | ENet host + service loop, autosave tick | — |
| `include/pch.hpp` | precompiled header (enet, openssl, std, Variant, `packet`, domain headers) | — |
| `include/proton/` | `Variant`/`VariantList` (VarList serialization), `Math/` vec2/vec3 | — |
| `include/proton/packet.hpp` | **protocol constants** (`packet::net_message`, `packet::tank`) | — |
| `include/event_type/` | ENet connect / disconnect / receive | `event_pool` (`__event_type.cpp`), `receive.cpp` |
| `include/action/` | text `action\|…` handlers, login | `action_pool` (`__action.cpp`) |
| `include/action/dialog_return/` | `action\|dialog_return` sub-handlers | `dialog_return_pool` (`__dialog_return.cpp`) |
| `include/state/` | binary tank packets (type 4) | `state_pool` (`__states.cpp`) |
| `include/on/` | server→client VariantList calls (`On*`) | called directly (`on::…`) |
| `include/commands/` | `/…` chat commands | `cmd_pool` (`__command.cpp`) |
| `include/https/` | `server_data.php` bootstrap, local TLS | `https::listener` |
| `include/automate/` | holiday / time-driven state | — |
| `include/tools/` | string/`hPipe`, `create_dialog`, crypt, `ransuu`, ip_tracker | — |
| `include/database/` | domain model + persistence (see below) | — |

### `include/database/` (split for cohesion)

| File | Holds |
|------|-------|
| `peer.hpp` / `peer.cpp` | `peer` class, `peers()`, inventory (`emplace`), XP/gems, the `state` packet type, `get_state`/`compress_state`, `send_inventory_state` |
| `world.hpp` | `world` + block/door/vending/… structs, **all** world free-function declarations, grid constants |
| `world.cpp` | world lifecycle (`worlds`, `current_world`, uid), packet-send helpers, object/drop ops, `send_tile_update`, particles |
| `world_serialize.cpp` | `ByteWriter`/`ByteReader`, `encode/decode_world`, MySQL `world_load`/`world_save`/autosave |
| `tile_lock.cpp` | Small/Big/Huge/Builder lock claim + edit-permission rules |
| `world_gen.cpp` | `generate_world`, `ensure_main_door_bedrock`, `door_mover`, `blast::thermonuclear` |
| `items.cpp` | `items.dat` decode, `id_to_item`, `type::` categories |
| `database.cpp` | MySQL connection, `hStmt`, `make_bind_in/out(_blob)` |

## Reuse these — do not reinvent

**Dispatch:** register new handlers in the matching pool; don't add ad-hoc `if (action == …)` chains.

**Peer / world access** (`database/peer.hpp`, `database/world.hpp`):
- `peer_of(event)` / `peer_of(peer)` → the `::peer&` — **not** `static_cast<::peer*>(x->data)`
- `current_world(peer)` / `current_world(event)` → `::world*` — not a hand-rolled `std::ranges::find(worlds, …)`
- `peer_by_netid(world, netid)` / `peer_by_uid(world, uid)` → find another peer in a world

**Inventory & drops:**
- `inventory_count(peer, id)` / `inventory_has(peer, id, n)` (peer.hpp) — read the backpack
- `modify_item_inventory(event, {id, ±count})` (world.hpp) — add/remove + client visual
- `give_to_backpack(event, id, amount) → remaining` (world.hpp) — drain a stack into the backpack in ≤200 chunks
- `add_object` / `add_drop` / `merge_object` / `remove_object` / `despawn_object` (world.hpp) — world drops
- `spill_drops(event, id, count, at, world)` (world.hpp) — spill a stack onto the ground in ≤200 chunks

**World grid** (`world.hpp`): `WORLD_WIDTH` / `WORLD_HEIGHT` / `WORLD_BLOCK_COUNT`, `cord(x, y)`, `in_bounds(x, y)` / `in_bounds(pos)` — never hardcode `100` / `60`.

**Protocol constants** (`proton/packet.hpp`): `packet::GAME_MESSAGE`, `packet::GAME_PACKET`, `packet::SEND_TILE_UPDATE_DATA`, … — never a bare `0x05` with a `// PACKET_*` comment.

**Dialogs** (`tools/create_dialog.hpp`): build with the fluent `create_dialog` builder, then `send_varlist(peer, { "OnDialogRequest", dlg.end_dialog(...) })`. Don't hand-concatenate `add_button|…\n` strings. If the builder lacks a widget, **add the method** rather than dropping to a raw string.

**Parsing** (`tools/string.hpp`): `hPipe pipe{header}; pipe["key"]` for `key|value` payloads; `readch(str, '|')` to split. Prefer key lookup over positional `[3]`/`[4]` indexing.

**Packet bytes:** `compress_state(state)` / `get_state(bytes)` (peer.hpp) for tank packets; `ByteWriter`/`ByteReader` (world_serialize.cpp) for the world blob.

**Send helpers:** `send_varlist` (proton), `send_action` / `send_data` / `send_tile_update` / `send_particle_effect` (world.hpp), and the `on::*` layer for named client calls.

## Verifying changes
No unit tests. CI (`.github/workflows/make.yml`) builds on Ubuntu (GCC) + Windows (MSYS2 UCRT64)
on every push. Anything that changes serialized bytes (tank packets, world blob, map data, dialog
strings) must additionally be exercised against a real client through GTProxy
(`GTProxy/run/config.gurotopia.json`) before merge.

See `REFACTOR.md` for the modularity refactor status and remaining staged work.
