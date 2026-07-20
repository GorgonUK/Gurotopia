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

    /* first bait stack in inventory, or 0 */
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
}

bool try_fishing(ENetEvent &event, const ::state &state, ::block &block, ::world &world)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    const short placed = static_cast<short>(state.id);
    const bool punching = (placed == 18);
    const bool placing_bait = is_bait(placed);

    // @note real GT casts by using bait on water; we also allow fist-punch on water
    // while holding a rod with bait in inventory (same outcome, consumes 1 bait).
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
        send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "`oYou need a `2Wiggly Worm`` (or other bait) to fish.``", 0u });
        return true;
    }

    if (steady_clock::now() - pPeer->last_cast < seconds(3))
    {
        send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "`oThe fish aren't biting yet...``", 0u });
        return true;
    }
    pPeer->last_cast = steady_clock::now();

    modify_item_inventory(event, ::slot(bait_id, -1));

    ransuu ransuu;
    if (ransuu[{0, 2}] == 0) // @note 1 in 3 casts comes up empty
    {
        send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "`oNot even a nibble.``", 0u });
        return true;
    }

    int total_weight{};
    for (const ::fish &fish : fish_table) total_weight += fish.weight;

    int roll = ransuu[{1, total_weight}];
    for (const ::fish &fish : fish_table)
    {
        roll -= fish.weight;
        if (roll > 0) continue;

        add_drop(event, ::slot(fish.id, 1), state.punch.by_32(), world);
        pPeer->add_xp(event, fish.xp);
        send_particle_effect(event, state.punch.by_32(), {0x00, 0x40});

        const std::string message = std::format("`{}{}`` caught a `2{}``!", pPeer->prefix, pPeer->growid, id_to_item(fish.id).raw_name);
        peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [pPeer, message](ENetPeer& peer)
        {
            send_varlist(&peer, { "OnTalkBubble", pPeer->netid, message, 0u });
        });
        on::ConsoleMessage(event.peer, message);

        achievement_progress(event, ACH_FISH_CAUGHT);
        quest_progress(event, QUEST_CATCH_FISH);
        break;
    }
    return true;
}
