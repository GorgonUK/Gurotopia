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

    /* drop the cast line / unfreeze look for everyone in the world */
    void broadcast_stop(ENetEvent &event, const ::peer &peer)
    {
        state_visuals(*event.peer, ::state{
            .type = 0x00, // @note normal movement snapshot clears the cast pose
            .netid = peer.netid,
            .pos = peer.pos
        });
    }

    void broadcast_splash(ENetPeer &/*peer*/, const ::peer &pPeer)
    {
        const ::pos pixel = pPeer.fish_tile.by_32();
        peers(pPeer.recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer &p)
        {
            ENetEvent pev{ .peer = &p };
            send_particle_effect(pev, pixel, {0x00, 0x24}, 36); // @note splash particle (id 36)
            send_varlist(&p, { "OnTalkBubble", pPeer.netid, "`9*splash!*``", 0u });
        });
    }

    void award_catch(ENetEvent &event, ::peer &pPeer, ::world &world, const ::pos &tile, short bait_id)
    {
        // @note bait is only taken on a successful catch
        auto has_bait = [&](short id) {
            auto it = std::ranges::find(pPeer.slots, id, &::slot::id);
            return it != pPeer.slots.end() && it->count > 0;
        };
        if (!has_bait(bait_id))
            bait_id = bait_in_inventory(pPeer);
        if (bait_id == 0)
        {
            send_varlist(event.peer, {
                "OnTalkBubble", pPeer.netid,
                "`oYou need bait to keep that fish!``", 0u
            });
            return;
        }
        modify_item_inventory(event, ::slot(bait_id, -1));

        ransuu rng;
        int total_weight{};
        for (const ::fish &fish : fish_table) total_weight += fish.weight;

        int roll = rng[{1, total_weight}];
        for (const ::fish &fish : fish_table)
        {
            roll -= fish.weight;
            if (roll > 0) continue;

            add_drop(event, ::slot(fish.id, 1), tile.by_32(), world);
            pPeer.add_xp(event, fish.xp);
            send_particle_effect(event, tile.by_32(), {0x00, 0x40});

            const std::string message = std::format("`{}{}`` caught a `2{}``!",
                pPeer.prefix, pPeer.growid, id_to_item(fish.id).raw_name);
            peers(pPeer.recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer &p)
            {
                send_varlist(&p, { "OnTalkBubble", pPeer.netid, message, 0u });
            });
            on::ConsoleMessage(event.peer, message);

            achievement_progress(event, ACH_FISH_CAUGHT);
            quest_progress(event, QUEST_CATCH_FISH);
            break;
        }
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
        send_varlist(event.peer, {
            "OnTalkBubble", pPeer.netid,
            "`oWaiting for a bite... punch when you see the splash!``", 0u
        });
    }

    /* punch while already fishing — reel in if bite is up, else cancel (no bait spent) */
    void try_reel(ENetEvent &event, ::peer &pPeer, ::world &world)
    {
        if (!pPeer.fishing) return;

        if (pPeer.fish_bite && steady_clock::now() <= pPeer.fish_bite_until)
        {
            const ::pos tile = pPeer.fish_tile;
            const short bait = pPeer.fish_bait;
            clear_session(pPeer);
            broadcast_stop(event, pPeer);
            award_catch(event, pPeer, world, tile, bait);
            return;
        }

        // @note punched too early / after the window — stop, keep bait
        clear_session(pPeer);
        broadcast_stop(event, pPeer);
        send_varlist(event.peer, {
            "OnTalkBubble", pPeer.netid,
            "`oToo early! You reeled in empty-handed.``", 0u
        });
    }
}

void fishing_cancel(ENetEvent &event, const char *reason)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    if (!pPeer->fishing) return;

    clear_session(*pPeer);
    broadcast_stop(event, *pPeer);
    if (reason && *reason)
        send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, reason, 0u });
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
            fishing_cancel(ev, "`oYou put your rod away.``");
            continue;
        }

        if (pPeer->fish_bite)
        {
            if (now > pPeer->fish_bite_until)
            {
                // @note missed the window — fish swam off; another splash may come (bait kept)
                pPeer->fish_bite = false;
                pPeer->fish_next_check = now + seconds(rng[{2, 4}]);
                send_varlist(&peer, {
                    "OnTalkBubble", pPeer->netid,
                    "`oThe fish got away... keep waiting.``", 0u
                });
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

    // @note already casting — any punch tries to reel; other actions cancel
    if (pPeer->fishing)
    {
        if (punching)
        {
            try_reel(event, *pPeer, world);
            return true;
        }
        fishing_cancel(event, "`oYou stopped fishing.``");
        return false; // @note let the new action proceed
    }

    if (!punching && !placing_bait) return false;

    if (!(block.state[3] & S_WATER))
    {
        if (placing_bait)
        {
            send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "`oCast your bait onto water.``", 0u });
            return true;
        }
        return false;
    }
    if (!has_fishing_rod(*pPeer))
    {
        if (placing_bait)
        {
            send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "`oEquip a `2Fishing Rod`` first.``", 0u });
            return true;
        }
        return false;
    }

    short bait_id = placing_bait ? placed : bait_in_inventory(*pPeer);
    if (bait_id == 0)
    {
        // @note punch on water with rod but no bait — ignore (normal punch)
        if (placing_bait)
            send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "`oYou need a `2Wiggly Worm`` (or other bait) to fish.``", 0u });
        return placing_bait;
    }

    start_cast(event, *pPeer, bait_id, state.punch);
    return true;
}
