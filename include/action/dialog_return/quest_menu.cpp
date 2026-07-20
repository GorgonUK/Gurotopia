#include "pch.hpp"
#include "on/ConsoleMessage.hpp"
#include "tools/ransuu.hpp"
#include "database/quests.hpp"
#include "database/achievements.hpp"

#include "quest_menu.hpp"

void quest_menu(ENetEvent &event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    ::Quest &quest = pPeer->quest;

    if (hPipe["buttonClicked"] == "quest_new")
    {
        if (quest.active()) return;

        ransuu ransuu;
        const u_int goal = ransuu[{0, QUEST_GOAL_COUNT - 1}];
        const ::quest_type &type = quest_types[goal];

        quest.goal = goal;
        quest.progress = 0;
        quest.target = ransuu[{(int)type.min, (int)type.max}];
        quest.reward_gems = quest.target * type.gems_each;
        pPeer->mark_dirty();

        on::ConsoleMessage(event.peer, std::format("`oNew quest: `w{}`` for `2{} Gems``!``", quest_describe(quest), quest.reward_gems));
        quest_dialog(event);
    }
    else if (hPipe["buttonClicked"] == "quest_claim")
    {
        if (!quest.complete()) return;

        pPeer->credit_gems(event, static_cast<int>(quest.reward_gems));
        pPeer->add_xp(event, 50);
        on::ConsoleMessage(event.peer, std::format("`oQuest complete! You received `2{} Gems``.``", quest.reward_gems));

        quest = ::Quest{};
        pPeer->mark_dirty();
        achievement_progress(event, ACH_QUESTS_DONE);
        quest_dialog(event);
    }
    else if (hPipe["buttonClicked"] == "quest_abandon")
    {
        quest = ::Quest{};
        pPeer->mark_dirty();
        on::ConsoleMessage(event.peer, "`oQuest abandoned.``");
        quest_dialog(event);
    }
    else if (hPipe["buttonClicked"] == "rolesmenu")
    {
        on::ConsoleMessage(event.peer, "`oRole Quests are not available yet.``");
        quest_dialog(event);
    }
}
