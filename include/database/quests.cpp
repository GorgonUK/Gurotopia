#include "pch.hpp"
#include "on/ConsoleMessage.hpp"
#include "tools/create_dialog.hpp"

#include "quests.hpp"

const std::array<::quest_type, QUEST_GOAL_COUNT> quest_types
{{
    { "Break",   "blocks", 50, 150,  10, 2 },    // Dirt
    { "Place",   "blocks", 50, 150,  10, 2 },    // Dirt
    { "Harvest", "trees",  10,  30,  50, 3 },    // Dirt Seed
    { "Catch",   "fish",    5,  15, 150, 3036 }  // Sunfish
}};

std::string quest_describe(const ::Quest &quest)
{
    if (!quest.active()) return "";

    const ::quest_type &type = quest_types[quest.goal];
    return std::format("{} {} {}", type.verb, quest.target, type.noun);
}

void quest_progress(ENetEvent &event, ::quest_goal goal, u_int add)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    ::Quest &quest = pPeer->quest;
    if (quest.goal != goal || quest.complete()) return;

    quest.progress = std::min(quest.progress + add, quest.target);
    pPeer->mark_dirty();
    if (quest.complete())
    {
        send_varlist(event.peer, { 
            "OnTalkBubble", 
            pPeer->netid, 
            std::format("`2Quest complete:`` {}! Use `$/quest`` to claim your reward.", quest_describe(quest)), 
            0u, 1u 
        });
    }
}

void quest_dialog(ENetEvent &event)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    std::string daily_body;
    const ::Quest &quest = pPeer->quest;
    if (!quest.active())
    {
        daily_body =
            "add_textbox|You don't have a quest right now. Take one on for a gem reward!|left|\n"
            "add_spacer|small|\n"
            "add_button|quest_new|`wGet a Quest``|noflags|0|0|\n";
    }
    else if (quest.complete())
    {
        const u_short icon = quest_types[quest.goal].icon;
        daily_body = std::format(
            "add_label_with_icon|small|`2{}`` - complete!|left|{}|\n"
            "add_spacer|small|\n"
            "add_button|quest_claim|`wClaim `2{} Gems````|noflags|0|0|\n",
            quest_describe(quest),
            icon,
            quest.reward_gems
        );
    }
    else
    {
        const u_short icon = quest_types[quest.goal].icon;
        daily_body = std::format(
            "add_label_with_icon|small|`w{}`` (`2{}``/`2{}``)|left|{}|\n"
            "add_textbox|Reward: `2{} Gems``|left|\n"
            "add_spacer|small|\n"
            "add_button|quest_abandon|`4Abandon Quest``|noflags|0|0|\n",
            quest_describe(quest),
            quest.progress,
            quest.target,
            icon,
            quest.reward_gems
        );
    }

    send_varlist(event.peer, {
        "OnDialogRequest",
        std::format(
            "set_default_color|`o\n"
            "add_label_with_icon|small|{}'s Goals|left|982|\n"
            "add_spacer|small|\n"
            "add_popup_name|GoalsList|\n"
            "add_textbox|`9Life Goals``|left|\n"
            "add_smalltext|`9Complete daily goals to earn gems for your Piggy Bank.``|left|\n"
            "add_spacer|small|\n"
            "add_textbox|`9Daily Quest``|left|\n"
            "{}"
            "add_spacer|small|\n"
            "add_textbox|`9Role Quests|left|\n"
            "add_button|rolesmenu|View Role Quests|noflags|0|0|\n"
            "add_spacer|small|\n"
            "end_dialog|goalslist||Back|\n"
            "add_quick_exit|\n",
            pPeer->growid,
            daily_body
        )
    });
}
