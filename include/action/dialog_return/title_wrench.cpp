#include "pch.hpp"
#include "tools/create_dialog.hpp"
#include "on/NameChanged.hpp"
#include "on/CountryState.hpp"
#include "action/wrench.hpp"

#include "title_wrench.hpp"

void send_title_edit_dialog(ENetEvent &event)
{
    ::peer &pPeer = peer_of(event);
    ::create_dialog dialog;
    dialog
        .set_default_color("`o")
        .add_label("big", "Select Title:   ", "right")
        .add_checkbox("checkbox_anniversary_ten_year_title", "Old Timer", pPeer.title_old_timer)
        .add_spacer("small")
        .add_button("", "OK");
    send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("title_edit", "", "") });
}

void title_edit(ENetEvent &event, const ::hPipe &hPipe)
{
    ::peer &pPeer = peer_of(event);
    pPeer.title_old_timer = hPipe["checkbox_anniversary_ten_year_title"] == "1";
    on::NameChanged(event);
    on::CountryState(event);
}

void send_wrench_customization_dialog(ENetEvent &event)
{
    ::create_dialog dialog;
    dialog
        .add_label_with_icon("big", "`wWrench Customization``", 32)
        .add_spacer("small")
        .add_textbox("Select your custom wrench!")
        .add_spacer("small")
        .add_label("big", "Wrench Style:")
        .add_custom_margin(10, 0)
        .add_custom_button("wrench_reset", "icon:32;border:yellow;margin:0,0;")
        .reset_placement_x()
        .add_custom_margin(0, 89)
        .add_spacer("small")
        .add_label("big", "Decoration:")
        .reset_placement_x()
        .add_custom_margin(10, 0)
        .add_custom_button("wrenchforeground_reset", "icon:1398;border:yellow;margin:0,0;")
        .reset_placement_x()
        .add_custom_margin(0, 89)
        .add_spacer("small")
        .add_button("cancel", "Back")
        .add_spacer("small")
        .add_spacer("small");
    send_varlist(event.peer, {
        "OnDialogRequest",
        dialog.end_dialog_with_quick_exit("wrench_customization_select", "", "")
    });
}

void wrench_customization_select(ENetEvent &event, const ::hPipe &hPipe)
{
    ::peer &pPeer = peer_of(event);
    const std::string &clicked = hPipe["buttonClicked"];

    if (clicked == "cancel")
    {
        action::send_self_wrench_menu(event, pPeer.netid);
        return;
    }

    if (clicked == "wrench_reset")
        pPeer.wrench_icon_id = -1;
    else if (clicked == "wrenchforeground_reset")
        pPeer.wrench_foreground_id = -1;
    else
        return;

    on::NameChanged(event);
    send_wrench_customization_dialog(event);
}
