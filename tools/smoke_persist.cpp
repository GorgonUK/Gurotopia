/* smoke test for persistence — not part of the game binary */
#include "pch.hpp"
#include "database/database.hpp"
#include "database/database_config.hpp"
#include "database/peer.hpp"
#include "database/world.hpp"
#include <cstdio>
#include <cstring>

int main()
{
    gDb_config = load_database_config();
    mysql_connect();

    // --- world roundtrip ---
    ::world w("SMOKETEST");
    generate_world(w, "SMOKETEST");
    w.blocks[cord(10, 20)].fg = 2;
    w.blocks[cord(10, 20)].tick = std::time(nullptr) - 120;
    w.blocks[cord(11, 20)].fg = 1008; // ATM provider
    w.blocks[cord(11, 20)].tick = std::time(nullptr) - 9999;
    w.objects.emplace_back(2, 5, ::pos{10.f, 20.f}, 1);
    w.last_object_uid = 1;
    w.owner = 1;
    w.mark_dirty();

    if (!world_save(w))
    {
        std::fprintf(stderr, "FAIL: world_save\n");
        return 1;
    }

    ::world loaded;
    if (!world_load(loaded, "SMOKETEST"))
    {
        std::fprintf(stderr, "FAIL: world_load\n");
        return 1;
    }

    if (loaded.blocks.size() != 6000 || loaded.blocks[cord(10, 20)].fg != 2)
    {
        std::fprintf(stderr, "FAIL: block mismatch\n");
        return 1;
    }
    if (block_elapsed_seconds(loaded.blocks[cord(10, 20)].tick) < 100)
    {
        std::fprintf(stderr, "FAIL: tick not offline-safe (elapsed=%d)\n",
            block_elapsed_seconds(loaded.blocks[cord(10, 20)].tick));
        return 1;
    }
    if (loaded.objects.size() != 1 || loaded.objects[0].id != 2 || loaded.owner != 1)
    {
        std::fprintf(stderr, "FAIL: objects/owner mismatch\n");
        return 1;
    }
    std::printf("OK world roundtrip (elapsed dirt tick=%d)\n",
        block_elapsed_seconds(loaded.blocks[cord(10, 20)].tick));

    // --- peer progress roundtrip ---
    ::peer p;
    p.user_id = 1;
    p.growid = "George";
    p.gems = 1234;
    p.level = {7, 42};
    p.slot_size = 26;
    p.slots = { {18, 1}, {32, 1}, {2, 50} };
    p.fav = {18, 32};
    p.clothing[0] = 12.0f;
    p.inventory_initialized = true;
    p.country = "us";

    if (!p.mysql_save_progress())
    {
        std::fprintf(stderr, "FAIL: peer save\n");
        return 1;
    }

    ::peer p2;
    p2.user_id = 1;
    p2.growid = "George";
    if (!p2.mysql_load_progress())
    {
        std::fprintf(stderr, "FAIL: peer load\n");
        return 1;
    }
    if (p2.gems != 1234 || p2.level.front() != 7 || p2.level.back() != 42 ||
        p2.slot_size != 26 || p2.slots.size() != 3 || !p2.inventory_initialized ||
        p2.fav.size() != 2 || p2.clothing[0] != 12.0f)
    {
        std::fprintf(stderr, "FAIL: peer fields mismatch gems=%d lvl=%u xp=%u slots=%zu\n",
            p2.gems, p2.level.front(), p2.level.back(), p2.slots.size());
        return 1;
    }
    std::printf("OK peer roundtrip (gems=%d level=%u slots=%zu)\n",
        p2.gems, p2.level.front(), p2.slots.size());

    mysql_query(db, "DELETE FROM world WHERE name='SMOKETEST'");
    mysql_close(db);
    std::puts("ALL PERSISTENCE SMOKE TESTS PASSED");
    return 0;
}
