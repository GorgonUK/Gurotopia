#include "pch.hpp"
#include "on/ConsoleMessage.hpp"
#include "tools/create_dialog.hpp"

#include "quests.hpp"

const std::array<::quest_type, QUEST_GOAL_COUNT> quest_types
{{
    { "Break",   "blocks", 50, 150,  10 },
    { "Place",   "blocks", 50, 150,  10 },
    { "Harvest", "trees",  10,  30,  50 },
    { "Catch",   "fish",    5,  15, 150 }
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

    ::create_dialog dialog;
    dialog
        .set_default_color("`o")
        .add_label_with_icon("big", "`wDaily Quest``", 23)
        .add_spacer("small");

    const ::Quest &quest = pPeer->quest;
    if (!quest.active())
    {
        dialog
            .add_textbox("You don't have a quest right now. Take one on for a gem reward!")
            .add_spacer("small")
            .add_button("quest_new", "`wGet a Quest``");
    }
    else if (quest.complete())
    {
        dialog
            .add_textbox(std::format("`2{}`` - complete!", quest_describe(quest)))
            .add_spacer("small")
            .add_button("quest_claim", std::format("`wClaim `2{} Gems````", quest.reward_gems));
    }
    else
    {
        dialog
            .add_textbox(std::format("`w{}`` (`2{}``/`2{}``)", quest_describe(quest), quest.progress, quest.target))
            .add_textbox(std::format("Reward: `2{} Gems``", quest.reward_gems))
            .add_spacer("small")
            .add_button("quest_abandon", "`4Abandon Quest``");
    }
    dialog.add_quick_exit();

    send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("quest_menu", "Close", "") });
}
