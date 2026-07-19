#include "pch.hpp"
#include "on/ConsoleMessage.hpp"
#include "on/SetBux.hpp"

#include "achievements.hpp"

const std::array<::achievement, ACH_COUNT> achievements
{{
    { "Demolition Expert",  "Break 500 blocks",        500, 1000 },
    { "Master Builder",     "Place 500 blocks",        500, 1000 },
    { "Green Thumb",        "Harvest 100 trees",       100, 1000 },
    { "World Traveler",     "Visit 50 worlds",          50,  500 },
    { "Master Angler",      "Catch 50 fish",            50, 1500 },
    { "Licensed Surgeon",   "Complete 10 surgeries",    10, 2000 },
    { "Quest Champion",     "Complete 10 daily quests", 10, 2000 }
}};

void achievement_progress(ENetEvent &event, ::ach ach, u_int add)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    u_int &progress = pPeer->ach_progress[ach];
    const ::achievement &achievement = achievements[ach];
    if (progress >= achievement.goal) return; // @note already earned

    progress = std::min(progress + add, achievement.goal);
    pPeer->mark_dirty();
    if (progress < achievement.goal) return;

    pPeer->gems += achievement.reward_gems;
    on::SetBux(event);

    const std::string message = std::format("`{}{}`` earned the achievement `#{}``!", pPeer->prefix, pPeer->growid, achievement.name);
    if (pPeer->netid != 0)
    {
        peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [pPeer, message](ENetPeer& peer) 
        {
            send_varlist(&peer, { "OnTalkBubble", pPeer->netid, message, 0u, 1u });
            on::ConsoleMessage(&peer, message);
        });
    }
    else on::ConsoleMessage(event.peer, message);
    on::ConsoleMessage(event.peer, std::format("`oYou received `w{} Gems`` for `#{}``!``", achievement.reward_gems, achievement.name));
}

u_char achievements_completed(const ::peer &peer)
{
    u_char completed{};
    for (u_char i = 0; i < ACH_COUNT; ++i)
        if (peer.ach_progress[i] >= achievements[i].goal) ++completed;

    return completed;
}
