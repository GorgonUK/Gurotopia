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
- ✅ split `database/world.cpp` (1320 → 495 lines) into cohesive units:
  - `world_serialize.cpp` — blob (de)serialization + MySQL load/save/autosave
  - `tile_lock.cpp` — Small/Big/Huge/Builder lock rules
  - `world_gen.cpp` — terrain generation + `blast::thermonuclear`
  - `world.cpp` (remainder) — world lifecycle, packet-send helpers, object/drop ops,
    `send_tile_update`, particles. Pure moves; declarations unchanged in `world.hpp`.
- 🔜 split `state/tile_change.cpp` (1001 lines) into `handle_punch/wrench/place` + per-type tables

## Tier 3 — structural (⚠️ build + in-client verification required)
These are the highest-value remaining refactors, but each is best executed **with a compiler and
a GTProxy client in the loop** — they either touch byte-exact wire format or fan out across many
files. Doing them "blind" risks a silent runtime desync that CI's compile check cannot catch.
Precise steps below so they can be picked up on a machine that can build.

- 🔜 ⚠️ **Shared tile-extra serializer** (highest payoff, highest risk). `world.cpp::send_tile_update`
  (the `switch(item.type)` that appends per-tile "extra" bytes) and the equivalent cascade in
  `action/join_request.cpp` are two hand-kept copies of the same binary layout.
  Steps: (1) diff the two switches to confirm they are byte-identical *today* (they may have
  drifted). (2) Extract `serialize_tile_extra(ByteWriter&, const world&, const block&, pos)` into
  a new `database/tile_extra.cpp` (declare in `world.hpp`). (3) Point both call sites at it.
  Verify each of the 11 tile types (lock/door/sign/mailbox/bulletin/donation/xenonite/spirit-board/
  seed/provider/display/vending/magplant) renders correctly on join AND on single-tile update.
- 🔜 **`peer` component split** — group the non-persisted runtime fields into nested structs:
  `FishingSession { active, bite, tile, bait, next_check, bite_until }` and
  `TradeSession { with_netid, offer, ready, confirmed }`. Touched by ~5 files each
  (`state/fishing.cpp`, `state/movement.cpp`, `state/item_activate.cpp`, `action/quit_to_exit.cpp`,
  `action/dialog_return/trade.cpp`). Compile-verifiable but broad field renames.
- 🔜 ⚠️ **Split `state/tile_change.cpp`** (~960-line function) into `handle_punch` / `handle_wrench` /
  `handle_place` (maps 1:1 to the `state.id == 18 / 32 / else` branch), then per-`type` handler
  tables mirroring `state_pool`. Extract remaining inline wrench dialogs (lock/door/sign/entrance)
  to helpers like the display/vending ones already are.
- 🔜 slim `pch.hpp`; move `host` / `state` out of `peer.hpp` into a `net.hpp` / `protocol/state.hpp`.
- 🔜 `create_dialog`: add `add_checkbox` / `add_text_box_input` / custom-tabs; migrate the raw-string
  dialogs (`popup`, `paginated_personal_notebook`, `buy`, owner branch of `vending`).
- 🔜 consolidate config loaders (`https/server_data.cpp`, `database/server_config.cpp`,
  `database/database_config.cpp`) behind one key/value loader; unify the 3 base64 impls
  (`tools/string.cpp`, `tools/crypt.cpp`) — security-sensitive, round-trip a known hash.

## Discoverability (for Cursor / new clones)
- ✅ `ARCHITECTURE.md` — module map + "reuse these helpers" index
- ✅ `.cursor/rules/code-organization.mdc` — steers the assistant to shared helpers + pool registration
- ✅ `AGENTS.md` — points at `ARCHITECTURE.md` and the split `world*.cpp` layout
