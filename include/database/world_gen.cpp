#include "pch.hpp"
#include "tools/ransuu.hpp"

#include "world.hpp"

/*
    World terrain generation. Split out of world.cpp.
    generate_world = normal spawn; blast::thermonuclear = nuked (empty) world.
    ensure_main_door_bedrock repairs the door/bedrock invariant either builder relies on.
*/

void generate_world(::world &world, const std::string& name)
{
    ransuu ransuu;
    // @note real GT: 100 wide × 60 tall; main door at y=24, bedrock under it at y=25,
    // dirt/cave from y=25, lava near y=50–53, bottom 6 rows (y=54–59) are bedrock.
    constexpr int WIDTH = 100;
    constexpr int HEIGHT = 60;
    constexpr int DOOR_Y = 24;
    constexpr int SURFACE_Y = 25;
    constexpr int ROCK_Y = 26;
    constexpr int LAVA_Y = 50;
    constexpr int BEDROCK_Y = 54;

    const u_short main_door = static_cast<u_short>(ransuu[{2, WIDTH - 4}]);
    std::vector<::block> blocks(static_cast<std::size_t>(WIDTH * HEIGHT), ::block{0, 0});

    for (std::size_t i = 0ull; i < blocks.size(); ++i)
    {
        ::block &block = blocks[i];
        const int y = static_cast<int>(i / WIDTH);

        if (y >= SURFACE_Y)
        {
            block.bg = 14; // cave background
            if (y >= ROCK_Y && y < LAVA_Y && ransuu[{0, 38}] <= 1)
                block.fg = 10; // rock
            else if (y >= LAVA_Y && y < BEDROCK_Y && ransuu[{0, 8}] < 3)
                block.fg = 4; // lava
            else
                block.fg = (y >= BEDROCK_Y) ? 8 : 2; // bedrock / dirt
        }
    }

    // Place exit AFTER terrain fill. Never use cord(door, DOOR_Y + 1) — without
    // parenthesized macro args that used to put bedrock in the sky, not under the door.
    {
        const std::size_t door_i = static_cast<std::size_t>(DOOR_Y) * static_cast<std::size_t>(WIDTH) + main_door;
        const std::size_t pad_i = door_i + static_cast<std::size_t>(WIDTH);
        blocks[door_i].fg = 6;
        blocks[door_i].label = "EXIT";
        blocks[pad_i].fg = 8;
        blocks[pad_i].bg = 14;
    }

    world.blocks = std::move(blocks);
    world.name = std::move(name);
    world.objects.clear();
    // Keep the global drop-uid high-water so the client baseline does not reset to 0
    // when entering a brand-new world after one that already spawned drops.
    world.last_object_uid = g_object_uid.load();
    ensure_main_door_bedrock(world);
    world.mark_dirty();
}

void ensure_main_door_bedrock(::world &world)
{
    if (world.blocks.empty()) return;
    const std::size_t width = 100ull;
    if (world.blocks.size() % width != 0) return;

    constexpr int SURFACE_Y = 25;
    constexpr int BEDROCK_Y = 54;
    bool changed = false;

    // Remove sky bedrock left by the old cord(x, y+1) precedence bug (y < surface).
    for (std::size_t i = 0; i < world.blocks.size(); ++i)
    {
        if (world.blocks[i].fg != 8) continue;
        const int y = static_cast<int>(i / width);
        if (y >= SURFACE_Y) continue;
        world.blocks[i].fg = 0;
        changed = true;
    }

    // Exactly one bedrock under each main door. Extra surface-row bedrock (from the
    // brief 3-wide pad mistake) becomes normal dirt again.
    for (std::size_t i = 0; i < world.blocks.size(); ++i)
    {
        if (world.blocks[i].fg != 6) continue;
        const std::size_t under = i + width;
        if (under >= world.blocks.size()) continue;
        if (world.blocks[under].fg != 8 || world.blocks[under].bg == 0)
        {
            world.blocks[under].fg = 8;
            if (world.blocks[under].bg == 0)
                world.blocks[under].bg = 14;
            changed = true;
        }
    }
    for (std::size_t i = 0; i < world.blocks.size(); ++i)
    {
        if (world.blocks[i].fg != 8) continue;
        const int y = static_cast<int>(i / width);
        if (y < SURFACE_Y || y >= BEDROCK_Y) continue;
        if (i < width) continue;
        if (world.blocks[i - width].fg == 6) continue; // pad under main door — keep
        world.blocks[i].fg = 2;
        if (world.blocks[i].bg == 0)
            world.blocks[i].bg = 14;
        changed = true;
    }
    if (changed) world.mark_dirty();
}

bool door_mover(::world &world, const ::pos &pos)
{
    std::vector<::block> &blocks = world.blocks;

    if (blocks[cord(pos.x, pos.y)].fg != 0 ||
        blocks[cord(pos.x, (pos.y + 1))].fg != 0) return false;

    for (std::size_t i = 0ull; i < blocks.size(); ++i)
    {
        if (blocks[i].fg == 6/*Main Door*/)
        {
            blocks[i].fg = 0; // @note remove main door
            blocks[cord(i % 100, (i / 100 + 1))].fg = 0; // @note remove bedrock below
            break;
        }
    }
    blocks[cord(pos.x, pos.y)].fg = 6;
    blocks[cord(pos.x, (pos.y + 1))].fg = 8;
    blocks[cord(pos.x, (pos.y + 1))].bg = 14;
    world.mark_dirty();
    return true;
}

void blast::thermonuclear(::world &world, const std::string& name)
{
    ransuu ransuu;

    constexpr int WIDTH = 100;
    constexpr int HEIGHT = 60;
    constexpr int DOOR_Y = 24;
    constexpr int BEDROCK_Y = 54;

    const u_short main_door = static_cast<u_short>(ransuu[{2, WIDTH - 4}]);
    std::vector<::block> blocks(static_cast<std::size_t>(WIDTH * HEIGHT), ::block{0, 0});
    for (std::size_t i = 0ull; i < blocks.size(); ++i)
    {
        const int y = static_cast<int>(i / WIDTH);
        blocks[i].fg = (y >= BEDROCK_Y) ? 8 : 0;
    }
    {
        const std::size_t door_i = static_cast<std::size_t>(DOOR_Y) * static_cast<std::size_t>(WIDTH) + main_door;
        const std::size_t pad_i = door_i + static_cast<std::size_t>(WIDTH);
        blocks[door_i].fg = 6;
        blocks[pad_i].fg = 8;
    }
    world.blocks = std::move(blocks);
    world.name = std::move(name);
    world.objects.clear();
    world.last_object_uid = g_object_uid.load();
    ensure_main_door_bedrock(world);
    world.mark_dirty();
}
