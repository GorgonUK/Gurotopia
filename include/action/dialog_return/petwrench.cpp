#include "pch.hpp"
#include "tools/create_dialog.hpp"
#include "action/join_request.hpp"

#include "petwrench.hpp"

namespace
{
    void add_gym_button(::create_dialog &dialog, std::string_view id, std::string_view label, int icon, short margin_x)
    {
        dialog
            .add_custom_margin(margin_x, 0)
            .add_custom_button(std::string{ id }, std::format("icon:{};border:grey;width:0.23;", icon))
            .add_custom_label(std::string{ label }, std::format("target:{};top:1.2;left:0.5;size:tiny;", id));
    }
}

void send_petwrench_tab(ENetEvent &event, int tab)
{
    ::peer &pPeer = peer_of(event);
    ::create_dialog dialog;
    dialog
        .set_default_color("`o")
        .start_custom_tabs()
        .add_custom_button("Tab_0", std::format(
            "image:interface/large/btn_petbattles01.rttex;image_size:228,92;frame:{};width:0.15;min_width:60;",
            tab == 0 ? "1, 0" : "0, 0"
        ))
        .add_custom_button("Tab_1", std::format(
            "image:interface/large/btn_petbattles01.rttex;image_size:228,92;frame:{};width:0.15;min_width:60;",
            tab == 1 ? "1, 1" : "0, 1"
        ))
        .end_custom_tabs()
        .embed_data("netID", pPeer.netid);

    if (tab == 0)
    {
        dialog
            .add_label_with_icon("big", "`wPet Battle``", 3676)
            .add_spacer("small")
            .add_textbox("Badges Earned: `50/6``")
            .add_spacer("small");
        add_gym_button(dialog, "gym_TerraForge", "TerraForge", 15264, 32);
        add_gym_button(dialog, "gym_Zephyrwing", "Zephyrwing", 15266, 64);
        add_gym_button(dialog, "gym_Abyssal Tide", "Abyssal Tide", 15268, 64);
        add_gym_button(dialog, "gym_Emberflare", "Emberflare", 15270, 64);
        add_gym_button(dialog, "gym_Stormcrag", "Stormcrag", 15272, 64);
        add_gym_button(dialog, "gym_Pyrodelve", "Pyrodelve", 15274, 64);
        dialog
            .add_custom_margin(64, 0)
            .reset_placement_x()
            .add_custom_margin(32, 125)
            .reset_placement_x()
            .add_smalltext("`5Click on an icon to travel to the Gym``")
            .add_spacer("small")
            .add_custom_margin(0, 16)
            .add_custom_button(
                "battle_pet_tournament",
                "textLabel:Travel to Master Peng;middle_colour:3389566975;border_colour:3389566975;display:block;state:disabled;opacity:0.3;"
            )
            .reset_placement_x()
            .add_custom_margin(0, 16)
            .add_spacer("small")
            .add_smalltext("Rumor has it that the legendary Master Battler…")
            .add_spacer("small")
            .add_smalltext("Conquer all Gym Leaders…")
            .add_spacer("small");
    }
    else
    {
        dialog
            .add_label_with_icon(
                "big",
                std::format("`w`w{}``'s Battle Leash``", pPeer.growid),
                3552
            )
            .add_spacer("small")
            .add_textbox("Leash pets from your cages to see them and battle.")
            .add_spacer("small");
    }

    send_varlist(event.peer, {
        "OnDialogRequest",
        dialog.end_dialog_with_quick_exit("petwrench", "Okay", "")
    });
}

void petwrench(ENetEvent &event, const ::hPipe &hPipe)
{
    const std::string &clicked = hPipe["buttonClicked"];
    if (clicked == "Tab_1")
    {
        send_petwrench_tab(event, 1);
        return;
    }
    if (clicked == "Tab_0" || clicked.empty())
    {
        send_petwrench_tab(event, 0);
        return;
    }
    if (clicked == "gym_TerraForge")
    {
        action::join_request(event, "", "TERRAFORGE_1");
        return;
    }
}
