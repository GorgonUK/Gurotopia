#include "pch.hpp"
#include "tools/create_dialog.hpp"
#include "database/quests.hpp"

#include "battlepass.hpp"

namespace
{
    void add_pass_tabs(::create_dialog &dialog, int selected)
    {
        // selected: 0=rewards, 1=tasks, 2=leaderboard
        dialog
            .start_custom_tabs()
            .add_custom_button("tab_rewards", std::format(
                "image:interface/large/btn_passtabs.rttex;image_size:228,92;frame:{};width:0.15;min_width:60;",
                selected == 0 ? "1,0" : "0,0"
            ))
            .add_custom_button("tab_tasks", std::format(
                "image:interface/large/btn_passtabs.rttex;image_size:228,92;frame:{};width:0.15;min_width:60;",
                selected == 1 ? "1,1" : "0,1"
            ))
            .add_custom_button("tab_perks", std::format(
                "image:interface/large/btn_passtabs.rttex;image_size:228,92;frame:{};width:0.15;min_width:60;",
                selected == 2 ? "1,2" : "0,2"
            ))
            .end_custom_tabs()
            .add_dialog_width_anchor(850);
    }
}

void send_battlepass_tasks(ENetEvent &event)
{
    ::create_dialog dialog;
    dialog.set_default_color("`o");
    add_pass_tabs(dialog, 1);
    dialog
        .add_label_with_icon("big", "Grow Pass Tasks", 9222)
        .add_spacer("small")
        .add_smalltext("All daily activities will reset soon.")
        .add_label_with_icon("small", "`wDaily Bonuses``", 14402)
        .add_spacer("small")
        .add_label_with_icon("small", "`wLife Goals``", 3902)
        .add_button("life_goals", "Life Goals")
        .add_label_with_icon("small", "`wDaily Quest``", 3902)
        .add_button("daily_quests", "Daily Quest")
        .add_label_with_icon("small", "`wBi-Weekly Quest``", 3902)
        .add_button("biweekly_quests", "Bi-Weekly Quest")
        .add_label_with_icon("small", "`wRole Quests``", 14404)
        .add_button("role_quests", "Role Quest")
        .add_quick_exit();
    send_varlist(event.peer, {
        "OnDialogRequest",
        dialog.end_dialog("battlepass_tasks", "", "Close")
    });
}

void send_battlepass_rewards(ENetEvent &event)
{
    ::create_dialog dialog;
    dialog.set_default_color("`o");
    add_pass_tabs(dialog, 0);
    dialog
        .add_image_button(
            "growpass_buy",
            "interface/large/gui_shop_grow_pass_buy.rttex",
            "bannerlayout",
            "OPENSTORE",
            "main/rt_grope_battlepass_bundle01",
            "-5.0"
        )
        .add_spacer("small")
        .add_custom_textbox("`oGrow Pass Points: 0/5400``", "medium")
        .add_spacer("small")
        .add_label_with_icon("big", "Royal Grow Pass Perks", 9222)
        .add_image_button("purchasePass", "interface/large/gui_growpass_perks_regular.rttex", "bannerlayout", "")
        .add_spacer("small")
        .add_quick_exit();
    send_varlist(event.peer, {
        "OnDialogRequest",
        dialog.end_dialog("battlepass_rewards", "", "Close")
    });
}

void send_battlepass_leaderboard(ENetEvent &event)
{
    ::create_dialog dialog;
    dialog
        .set_default_color("`o")
        .disable_resize();
    add_pass_tabs(dialog, 2);
    dialog
        .add_label_with_icon("big", "Royal Grow Pass Leaderboard", 14406)
        .add_custom_textbox(
            "Top 3 players will be `9Grow Royals this month`` and earn an Exclusive Title for the next month.",
            "medium"
        )
        .add_custom_textbox("`5(Score updates every 20 seconds)``", "small")
        .add_label("small", "Your Rank: 0")
        .add_label("small", "Your Points: 0")
        .add_smalltext("1. `odinrah`` - 33,710")
        .add_quick_exit();
    send_varlist(event.peer, {
        "OnDialogRequest",
        dialog.end_dialog("battlepass_leaderboard", "", "")
    });
}

void battlepass(ENetEvent &event, const ::hPipe &hPipe)
{
    const std::string &clicked = hPipe["buttonClicked"];
    if (clicked == "tab_rewards")
    {
        send_battlepass_rewards(event);
        return;
    }
    if (clicked == "tab_tasks")
    {
        send_battlepass_tasks(event);
        return;
    }
    if (clicked == "tab_perks")
    {
        send_battlepass_leaderboard(event);
        return;
    }
    if (clicked == "life_goals" || clicked == "daily_quests" ||
        clicked == "biweekly_quests" || clicked == "role_quests")
    {
        quest_dialog(event);
        return;
    }
}
