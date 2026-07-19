#include "pch.hpp"
#include "on/ConsoleMessage.hpp"
#include "tools/create_dialog.hpp"
#include "tools/logger.hpp"

#include "popup.hpp"

void popup(ENetEvent& event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    if (hPipe["buttonClicked"] == "my_worlds")
    {
        auto section = [](const auto& range) 
        {
            std::string result;
            for (const auto &name : range)
                if (!name.empty())
                    result.append(std::format("add_button|{0}|{0}|noflags|0|0|\n", name));
            return result;
        };
        send_varlist(event.peer, {
            "OnDialogRequest",
            std::format(
                "set_default_color|`o\n"
                "start_custom_tabs|\n"
                "add_custom_button|myWorldsUiTab_0|image:interface/large/btn_tabs2.rttex;image_size:228,92;frame:1,0;width:0.15;|\n"
                "add_custom_button|myWorldsUiTab_1|image:interface/large/btn_tabs2.rttex;image_size:228,92;frame:0,1;width:0.15;|\n"
                "add_custom_button|myWorldsUiTab_2|image:interface/large/btn_tabs2.rttex;image_size:228,92;frame:0,2;width:0.15;|\n"
                "end_custom_tabs|\n"
                "add_label|big|Locked Worlds|left|0|\n"
                "add_spacer|small|\n"
                "add_textbox|Place a World Lock in a world to lock it. Break your World Lock to unlock a world.|left|\n"
                "add_spacer|small|\n"
                "{}\n"
                "add_spacer|small|\n"
                "end_dialog|worlds_list||Back|\n"
                "add_quick_exit|\n",
                section(pPeer->my_worlds)
            )
        });
    }
    else if (hPipe["buttonClicked"] == "billboard_edit")
    {
        const ::item &item = id_to_item(pPeer->billboard.id);

        send_varlist(event.peer, {
            "OnDialogRequest",
            std::format(
                "set_default_color|`o\n"
                "add_label_with_icon|big|`wTrade Billboard``|left|8282|\n"
                "add_spacer|small|\n"
                "{}"
                "add_item_picker|billboard_item|`wSelect Billboard Item``|Choose an item to put on your billboard!|\n"
                "add_spacer|small|\n"
                "add_checkbox|billboard_toggle|`$Show Billboard``|{}\n"
                "add_checkbox|billboard_buying_toggle|`$Is Buying``|{}\n"
                "add_text_input|setprice|Price of item:|{}|5|\n"
                "add_checkbox|chk_peritem|World Locks per Item|{}\n"
                "add_checkbox|chk_perlock|Items per World Lock|{}\n"
                "add_spacer|small|\n"
                "end_dialog|billboard_edit|Close|Update|\n",
                (pPeer->billboard.id == 0) ? 
                    "" : 
                    std::format("add_label_with_icon|small|`w{}``|left|{}|\n", item.raw_name, pPeer->billboard.id),
                to_char(pPeer->billboard.show),
                to_char(pPeer->billboard.isBuying),
                pPeer->billboard.price,
                to_char(pPeer->billboard.perItem),
                to_char(!pPeer->billboard.perItem)
            )
        });
    }
    else if (hPipe["buttonClicked"] == "seed_diary_customization")
    {
        send_varlist(event.peer, { "OnDialogRequestRML", "show_seed_diary_ui" });
    }
    else if (hPipe["buttonClicked"] == "add_to_worldlock")
    {
        auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
        if (world == worlds.end() || world->owner != pPeer->user_id)
        {
            send_varlist(event.peer, { "OnTextOverlay", "Only the World Lock owner can invite players." });
            return;
        }

        const short netid = static_cast<short>(atoi(hPipe["netID"].c_str()));
        ENetPeer *target = nullptr;
        peers(world->name, PEER_SAME_WORLD, [netid, &target](ENetPeer &peer)
        {
            ::peer *pOthers = static_cast<::peer*>(peer.data);
            if (pOthers && pOthers->netid == netid)
                target = &peer;
        });

        if (target == nullptr)
        {
            send_varlist(event.peer, { "OnTextOverlay", "That player is no longer here." });
            return;
        }

        ::peer *pTarget = static_cast<::peer*>(target->data);
        if (pTarget->user_id == world->owner
            || std::ranges::find(world->access, pTarget->user_id) != world->access.end())
        {
            send_varlist(event.peer, { "OnTextOverlay", "They already have access." });
            return;
        }

        if (std::ranges::find(world->access, 0) == world->access.end())
        {
            send_varlist(event.peer, { "OnTextOverlay", "No free World Lock access slots." });
            return;
        }

        send_varlist(target, {
            "OnDialogRequest",
            ::create_dialog()
                .set_default_color("`o")
                .add_label_with_icon("big", "`wWorld Lock Invite``", 242)
                .add_spacer("small")
                .add_textbox(std::format("`2{}`` invited you to access `w{}``.``", pPeer->growid, world->name))
                .embed_data("world", world->name)
                .embed_data("inviter", pPeer->growid)
                .embed_data("inviter_uid", pPeer->user_id)
                .add_spacer("small")
                .add_button("accept", "`2Accept``")
                .add_button("decline", "`4Decline``")
                .end_dialog("worldlock_invite", "Close", "")
        });

        on::ConsoleMessage(event.peer, std::format("`oInvite sent to `2{}``.``", pTarget->growid));
        log_event("worldlock_invite", std::format("{}({})", pPeer->growid, pPeer->user_id),
            world->name, std::format("invitee={}({})", pTarget->growid, pTarget->user_id));
    }
}
