#include "pch.hpp"
#include "on/ConsoleMessage.hpp"
#include "tools/ransuu.hpp"
#include "database/achievements.hpp"
#include "database/quests.hpp"

#include "fishing.hpp"

using namespace std::chrono;

namespace
{
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
}

bool try_fishing(ENetEvent &event, const ::state &state, ::block &block, ::world &world)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    if (pPeer->clothing[hand] != 2912/*Fishing Rod*/) return false;
    if (!(block.state[3] & S_WATER)) return false;

    if (steady_clock::now() - pPeer->last_cast < seconds(3))
    {
        send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "`oThe fish aren't biting yet...``", 0u });
        return true;
    }
    pPeer->last_cast = steady_clock::now();

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
