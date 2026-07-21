#include "pch.hpp"
#include "on/SetBux.hpp"

#include "openPiggyBank.hpp"

void action::openPiggyBank(ENetEvent &event, const std::string &header)
{
    (void)header;
    ::peer *pPeer = &peer_of(event);

    const int cap = pPeer->piggy_cap();
    const int banked = std::clamp(pPeer->piggy_gems, 0, cap);
    const bool full = (banked >= cap);

    // Official shows a green IAP "Buy {price}" button here; we repurpose the slot as a
    // Collect button: greyed (disabled IAP style) while filling, clickable once full.
    const std::string collect_row = full
        ? std::format(
            "add_button|piggy_collect|`2Collect {} Gems``|noflags|0|0|\n",
            on::format_gems_commas(cap)
        )
        : "add_iap_button|rt_grope_loyalty_bundle02|`oPiggy Bank``||label:Collect;disabled:1;|\n";

    send_varlist(event.peer, {
        "OnDialogRequest",
        std::format(
            "set_default_color|`o\n"
            "add_label_with_icon|big|`w Piggy Bank``|left|14540|\n"
            "add_image_button||interface/large/gui_shop_buybanner3.rttex|bannerlayout|flag_frames:4,10,0,9|flag_surfsize:512,200|\n"
            "add_spacer|small|\n"
            "add_textbox|You have banked: {} / {} Gems.|left|\n"
            "add_spacer|small|\n"
            "add_textured_progress_bar|interface/large/gui_event_bar2.rttex|0|4||{}|{}|relative|0.8|0.02|0.07|1000|64|0.007|barBG_2|\n"
            "add_spacer|small|\n"
            "add_custom_button|piggy_bank_bg_img_0|image:game/tiles_page1.rttex;image_size:32,32;frame:22,1;width:0.05;state:disabled;preset:listitem;margin_rself:0,0;anchor:barBG_2;top:0;left:-0.005;display:inline_free;|\n"
            "add_custom_button|piggy_bank_bg_img_150|image:game/tiles_page16.rttex;image_size:32,32;frame:22,26;width:0.07;state:disabled;preset:listitem;margin_rself:0.2,0;anchor:barBG_2;top:0;left:0.5;display:inline_free;|\n"
            "add_custom_button|piggy_bank_bg_img_300|image:game/tiles_page16.rttex;image_size:32,32;frame:23,26;width:0.07;state:disabled;preset:listitem;margin_rself:0.6,0;anchor:barBG_2;top:0;left:1;display:inline_free;|\n"
            "add_custom_label|0|target:barBG_2;top:1.35;left:0.01;size:small;|\n"
            "add_custom_label|{}|target:barBG_2;top:1.35;left:0.5;size:small;|\n"
            "add_custom_label|{}|target:barBG_2;top:1.35;left:1;size:small;|\n"
            "reset_placement_x|\n"
            "add_spacer|small|\n"
            "add_textbox|Complete Life Goals, Daily Bonuses and Daily Quests to add gems to your Piggy Bank!|left|\n"
            "add_textbox|- Life Goals - 65K Gems|left|\n"
            "add_textbox|- Daily Bonuses - 65K Gems|left|\n"
            "add_textbox|- Daily Quests - 90K Gems|left|\n"
            "add_spacer|small|\n"
            "reset_placement_x|\n"
            "{}"
            "add_spacer|small|\n"
            "add_spacer|small|\n"
            "end_dialog|piggy_bank||\n"
            "add_quick_exit|\n",
            on::format_gems_commas(banked),
            on::format_gems_commas(cap),
            banked, cap,
            on::format_gems_compact(cap / 2),
            on::format_gems_compact(cap),
            collect_row
        )
    });
}
