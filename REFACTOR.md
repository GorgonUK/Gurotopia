# Modularity Refactor — tracker

Branch: `refactor/modularity`. Goal: make the codebase easier to maintain and extend by
extracting shared helpers/modules and breaking up oversized units, **without changing behaviour**.

No build runs in the authoring environment. CI (`.github/workflows/make.yml`) compiles on
Ubuntu (GCC) + Windows (MSYS2 UCRT64) on every push, so **compile errors are caught by CI**.
There are no unit tests, so anything that changes serialized bytes must be verified against a
real client through GTProxy before merge.

Legend: ✅ done · 🔜 planned · ⚠️ needs in-client (GTProxy) verification before merge

## Tier 1 — safe dedup / constants (CI-verifiable, behaviour-preserving)
- ✅ `proton/packet.hpp` — one home for protocol (net-message / tank-packet) constants; wired the
  central dispatch (`receive.cpp`, `__states.cpp`) and the documented send sites
- ✅ pool registration: drop redundant `std::bind` identity wrappers (all 5 pools)
- 🔜 `tools/byte_stream.hpp` — promote `ByteWriter`/`ByteReader` out of `world.cpp`'s anon namespace
- ✅ access helpers: `peer_of(event)`, `current_world(...)`, `peer_by_netid/uid`, `inventory_count/has`
  (added + `world.cpp` migrated; handler call sites migrate opportunistically)
- ✅ world grid constants `WORLD_WIDTH/HEIGHT` + `in_bounds()` (migrated in `world.cpp`)
- ✅ ⚠️ `spill_drops()` + `give_to_backpack()` — collapsed the 200-count drain loops
  (7 copies across `magplant`/`vending`/`tile_change`); touches inventory → verify in-client
- 🔜 consolidate the 3 config loaders + 3 base64 implementations

## Tier 2 — mechanical file splits (CI-verifiable moves)
- 🔜 split `database/world.cpp` (1320 lines) into cohesive units
- 🔜 split `state/tile_change.cpp` (1001 lines) into `handle_punch/wrench/place` + per-type tables

## Tier 3 — structural (⚠️ build + in-client verification required)
- 🔜 ⚠️ shared tile-extra serializer used by both `world.cpp` and `join_request.cpp`
- 🔜 `peer` component split — start with non-persisted `FishingSession` / `TradeSession`
- 🔜 slim `pch.hpp`; move `host` / `state` out of `peer.hpp`
- 🔜 `dialog builder`: add missing widgets, migrate raw-string dialogs

## Discoverability (for Cursor / new clones)
- 🔜 `ARCHITECTURE.md` — module map + "reuse these helpers" index
- 🔜 `.cursor/rules/` — point the assistant at the shared helpers before writing new code
- 🔜 refresh `AGENTS.md` with the new module layout
