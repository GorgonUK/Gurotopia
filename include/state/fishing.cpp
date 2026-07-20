#include "pch.hpp"
#include "on/ConsoleMessage.hpp"
#include "tools/ransuu.hpp"
#include "database/achievements.hpp"
#include "database/quests.hpp"

#include "fishing.hpp"

using namespace std::chrono;

namespace
{
    constexpr short FISHING_ROD = 2912;
    constexpr short WIGGLY_WORM = 2914;

    /* baits accepted for casting (expand as more baits are supported) */
    constexpr std::array<short, 1ull> baits{ WIGGLY_WORM };

    constexpr auto BITE_WINDOW = milliseconds(2500);
    constexpr int SPLASH_CHANCE = 45; // @note percent per check after the wait

    struct fish
    {
        short id{};
        u_char weight{}; // @note roll share; higher = more common
        u_short xp{};
    };
    /* rarest last so a single roll can walk the table */
    constexpr std::array<::fish, 9ull> fish_table
    {{
        { 3036/*Sunfish*/,      25,  2 },
        { 3028/*Perch*/,        25,  2 },
        { 3038/*Bass*/,         15,  5 },
        { 3026/*Catfish*/,      12,  5 },
        { 3030/*Dogfish*/,      10,  8 },
        { 3032/*Gar*/,           6, 10 },
        { 3034/*Mahi Mahi*/,     4, 15 },
        { 3022/*Electric Eel*/,  2, 25 },
        { 3024/*Whale*/,         1, 50 }
    }};

    bool is_bait(short id)
    {
        return std::ranges::find(baits, id) != baits.end();
    }

    bool has_fishing_rod(const ::peer &peer)
    {
        return static_cast<short>(peer.clothing[hand]) == FISHING_ROD;
    }

    short bait_in_inventory(const ::peer &peer)
    {
        for (short bait : baits)
        {
            auto it = std::ranges::find(peer.slots, bait, &::slot::id);
            if (it != peer.slots.end() && it->count > 0)
                return bait;
        }
        return 0;
    }

    void clear_session(::peer &peer)
    {
        peer.fishing = false;
        peer.fish_bite = false;
        peer.fish_tile = {};
        peer.fish_bait = 0;
        peer.fish_next_check = {};
        peer.fish_bite_until = {};
    }

    /* cast visual — packet type 0x1F (fishing line) at the water tile */
    void broadcast_cast(ENetEvent &event, const ::peer &peer, const ::pos &tile)
    {
        state_visuals(*event.peer, ::state{
            .type = 0x1F,
            .netid = peer.netid,
            .punch = tile
        });
    }

    /* clear fishing line for everyone — GTOS uses SET_CHARACTER_STATE + OnSetPos */
    void broadcast_stop(ENetEvent &event, ::peer &peer)
    {
        // @note restore normal character state (undoes the cast freeze / line attach)
        state_visuals(*event.peer, ::state{
            .type = 0x14 | ((0x808000 + peer.punch_effect) << 8),
            .netid = peer.netid,
            .count = 125.0f,
            .id = peer.state,
            .pos = ::pos{ 1200.0f, 200.0f },
            .speed = ::pos{ 250.0f, 1000.0f },
            .punch = ::pos{ peer.hair_color, 0x00000000 }
        });

        // @note snap position — client drops the fishing line that was following the player
        const CL_Vec2f at{ peer.pos.x, peer.pos.y };
        peers(peer.recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer &p)
        {
            send_varlist(&p, { "OnSetPos", at }, peer.netid);
        });
    }

    void broadcast_splash(ENetPeer &/*peer*/, const ::peer &pPeer)
    {
        const ::pos pixel = pPeer.fish_tile.by_32();
        peers(pPeer.recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer &p)
        {
            ENetEvent pev{ .peer = &p };
            send_particle_effect(pev, pixel, {0x00, 0x24}, 36); // @note splash particle (id 36)
        });
    }

    std::size_t used_inventory_slots(const ::peer &peer)
    {
        return static_cast<std::size_t>(std::ranges::count_if(peer.slots, [](const ::slot &s) {
            return s.count > 0;
        }));
    }

    bool inventory_can_take(const ::peer &peer, short item_id)
    {
        auto existing = std::ranges::find(peer.slots, item_id, &::slot::id);
        if (existing != peer.slots.end() && existing->count > 0 && existing->count < 200)
            return true;
        return used_inventory_slots(peer) < static_cast<std::size_t>(peer.slot_size);
    }

    void award_catch(ENetEvent &event, ::peer &pPeer, ::world &world, short bait_id)
    {
        auto has_bait = [&](short id) {
            auto it = std::ranges::find(pPeer.slots, id, &::slot::id);
            return it != pPeer.slots.end() && it->count > 0;
        };
        if (!has_bait(bait_id))
            bait_id = bait_in_inventory(pPeer);
        if (bait_id == 0)
            return; // @note no bait left — silent fail, no catch bubble

        modify_item_inventory(event, ::slot(bait_id, -1));

        ransuu rng;
        int total_weight{};
        for (const ::fish &fish : fish_table) total_weight += fish.weight;

        int roll = rng[{1, total_weight}];
        for (const ::fish &fish : fish_table)
        {
            roll -= fish.weight;
            if (roll > 0) continue;

            if (inventory_can_take(pPeer, fish.id))
                modify_item_inventory(event, ::slot(fish.id, 1));
            else
                add_drop(event, ::slot(fish.id, 1), pPeer.pos, world);

            pPeer.add_xp(event, fish.xp);
            send_particle_effect(event, pPeer.pos, {0x00, 0x40});

            const std::string message = std::format("You caught a `2{}``!",
                id_to_item(fish.id).raw_name);
            send_varlist(event.peer, { "OnTalkBubble", pPeer.netid, message, 0u });
            on::ConsoleMessage(event.peer, message);

            achievement_progress(event, ACH_FISH_CAUGHT);
            quest_progress(event, QUEST_CATCH_FISH);
            break;
        }
    }

    /* standing on the water tile or any neighbouring tile (Chebyshev distance <= 1) */
    bool in_cast_range(const ::peer &peer, const ::pos &water_tile)
    {
        const ::pos stand = peer.pos.by_32(true);
        return std::abs(stand.x_int() - water_tile.x_int()) <= 1
            && std::abs(stand.y_int() - water_tile.y_int()) <= 1;
    }

    void start_cast(ENetEvent &event, ::peer &pPeer, short bait_id, const ::pos &tile)
    {
        ransuu rng;
        // @note do NOT consume bait here — only on a successful catch

        pPeer.fishing = true;
        pPeer.fish_bite = false;
        pPeer.fish_tile = tile;
        pPeer.fish_bait = bait_id;
        // @note first splash opportunity after 2–5s, then ~1s retries
        pPeer.fish_next_check = steady_clock::now() + seconds(rng[{2, 5}]);
        pPeer.fish_bite_until = {};

        broadcast_cast(event, pPeer, tile);
    }

    /* punch while already fishing — reel in if bite is up, else cancel (no bait spent) */
    void try_reel(ENetEvent &event, ::peer &pPeer, ::world &world)
    {
        if (!pPeer.fishing) return;

        if (pPeer.fish_bite && steady_clock::now() <= pPeer.fish_bite_until)
        {
            const short bait = pPeer.fish_bait;
            clear_session(pPeer);
            broadcast_stop(event, pPeer);
            award_catch(event, pPeer, world, bait);
            return;
        }

        // @note punched too early / after the window — stop silently, keep bait
        clear_session(pPeer);
        broadcast_stop(event, pPeer);
    }
}

void fishing_cancel(ENetEvent &event, const char *reason)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    if (!pPeer->fishing) return;

    clear_session(*pPeer);
    broadcast_stop(event, *pPeer);
    (void)reason; // @note bubbles only on successful catch
}

void fishing_tick()
{
    ransuu rng;
    const auto now = steady_clock::now();

    for (ENetPeer &peer : std::span(host->peers, host->peerCount))
    {
        if (peer.state != ENET_PEER_STATE_CONNECTED || peer.data == nullptr) continue;
        ::peer *pPeer = static_cast<::peer*>(peer.data);
        if (!pPeer->fishing) continue;

        // @note rod unequipped mid-cast (e.g. inventory race) — stop
        if (!has_fishing_rod(*pPeer))
        {
            ENetEvent ev{ .peer = &peer };
            fishing_cancel(ev);
            continue;
        }

        if (pPeer->fish_bite)
        {
            if (now > pPeer->fish_bite_until)
            {
                // @note missed the window — fish swam off; another splash may come (bait kept)
                pPeer->fish_bite = false;
                pPeer->fish_next_check = now + seconds(rng[{2, 4}]);
            }
            continue;
        }

        if (now < pPeer->fish_next_check) continue;

        if (rng[{1, 100}] <= SPLASH_CHANCE)
        {
            pPeer->fish_bite = true;
            pPeer->fish_bite_until = now + BITE_WINDOW;
            broadcast_splash(peer, *pPeer);
        }
        else
            pPeer->fish_next_check = now + milliseconds(rng[{800, 1600}]);
    }
}

bool try_fishing(ENetEvent &event, const ::state &state, ::block &block, ::world &world)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    const short placed = static_cast<short>(state.id);
    const bool punching = (placed == 18);
    const bool placing_bait = is_bait(placed);

    // @note already casting — any click (fist or selected item) tries to reel; swallow so nothing is placed
    if (pPeer->fishing)
    {
        try_reel(event, *pPeer, world);
        return true;
    }

    if (!punching && !placing_bait) return false;

    if (!(block.state[3] & S_WATER))
        return placing_bait; // @note swallow bait place on land (no bubble)

    if (!has_fishing_rod(*pPeer))
        return placing_bait; // @note swallow bait place without rod

    // @note must stand on the water or a tile next to it (not 2+ blocks away)
    if (!in_cast_range(*pPeer, state.punch))
        return true;

    short bait_id = placing_bait ? placed : bait_in_inventory(*pPeer);
    if (bait_id == 0)
        return placing_bait; // @note punch with no bait — ignore; bait place with none — swallow

    start_cast(event, *pPeer, bait_id, state.punch);
    return true;
}
