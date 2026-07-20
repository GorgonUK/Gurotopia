#include "pch.hpp"
#include "on/ConsoleMessage.hpp"
#include "action/quit_to_exit.hpp"
#include "database/world_ban.hpp"
#include "database/achievements.hpp"
#include "tools/create_dialog.hpp"
#include "database/quests.hpp"
#include "surgery.hpp"
#include "trade.hpp"
#include "paginated_personal_notebook.hpp"

#include "popup.hpp"

/* 
* @brief find a peer in pPeer's world by netid. moderation targets (pull/kick/ban) are 
* only valid when the clicker owns the world and the target is a regular player.
* @return nullptr when not found or not allowed.
*/
static ENetPeer *find_moderation_target(ENetEvent &event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
    if (world == worlds.end() || world->owner != pPeer->user_id) return nullptr;

    const short netid = atoi(hPipe["netID"].c_str());
    ENetPeer *target{};
    peers(world->name, PEER_SAME_WORLD, [netid, &target](ENetPeer &peer)
    {
        ::peer *pOthers = static_cast<::peer*>(peer.data);
        if (pOthers->netid == netid) target = &peer;
    });
    if (target == nullptr) return nullptr;

    ::peer *pTarget = static_cast<::peer*>(target->data);
    if (pTarget->user_id == world->owner || pTarget->role) return nullptr;

    return target;
}

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
    else if (hPipe["buttonClicked"] == "set_online_status")
    {
        const bool is_online = pPeer->online_status == 0;
        const bool is_busy = pPeer->online_status == 1;
        const bool is_away = pPeer->online_status == 2;
        send_varlist(event.peer, {
            "OnDialogRequest",
            std::format(
                "set_default_color|`o\n"
                "add_label_with_icon|big| `wOnline Status`` |left|1366|\n"
                "add_spacer|small|\n"
                "add_checkbox|checkbox_status_online|Online|{}\n"
                "add_checkbox|checkbox_status_busy|Busy|{}\n"
                "add_checkbox|checkbox_status_away|Away|{}\n"
                "add_button||Ok|noflags|0|0|\n"
                "end_dialog|set_online_status||\n"
                "add_quick_exit|\n",
                to_char(is_online),
                to_char(is_busy),
                to_char(is_away)
            )
        });
    }
    else if (hPipe["buttonClicked"] == "notebook_edit")
    {
        send_notebook_view(event, 0);
    }
    else if (hPipe["buttonClicked"] == "goals")
    {
        quest_dialog(event); // @note same as /quest
    }
    else if (hPipe["buttonClicked"] == "alist")
    {
        ::create_dialog dialog;
        dialog
            .set_default_color("`o")
            .add_label_with_icon("small", std::format("{}'s Achievements", pPeer->growid), 982)
            .add_spacer("small");
        for (u_char i = 0; i < ACH_COUNT; ++i)
        {
            const ::achievement &achievement = achievements[i];
            const u_int progress = pPeer->ach_progress[i];
            if (progress >= achievement.goal)
            {
                dialog.add_achieve(
                    std::string{ achievement.name },
                    std::format("Earned for {}.", achievement.description),
                    achievement.icon
                );
            }
            else if (progress == 0)
            {
                dialog.add_achieve(std::string{ achievement.name }, "Not achieved!", 127);
            }
            else
            {
                const u_int pct = (achievement.goal == 0) ? 0 : (progress * 100u) / achievement.goal;
                dialog.add_achieve(
                    std::string{ achievement.name },
                    std::format("Progress: {}%", pct),
                    127
                );
            }
        }
        dialog
            .add_spacer("small")
            .add_quick_exit();
        send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("popup", "", "Close") });
    }
    else if (hPipe["buttonClicked"] == "trade")
    {
        const short netid = atoi(hPipe["netID"].c_str());
        ENetPeer *target{};
        peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [netid, &target](ENetPeer &peer)
        {
            if (static_cast<::peer*>(peer.data)->netid == netid) target = &peer;
        });
        if (target == nullptr || target == event.peer) return;
        trade_dialog(event, target);
    }
    else if (hPipe["buttonClicked"] == "surgery_start")
    {
        const short netid = atoi(hPipe["netID"].c_str());
        ENetPeer *target{};
        peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [netid, &target](ENetPeer &peer)
        {
            if (static_cast<::peer*>(peer.data)->netid == netid) target = &peer;
        });
        if (target == nullptr || target == event.peer) return;

        surgery_start(event, target);
    }
    else if (hPipe["buttonClicked"] == "pull_player")
    {
        ENetPeer *target = find_moderation_target(event, hPipe);
        if (target == nullptr) return;
        ::peer *pTarget = static_cast<::peer*>(target->data);

        send_varlist(target, {
            "OnSetPos", 
            CL_Vec2f{pPeer->pos.x, pPeer->pos.y}
        }, pTarget->netid);
        pTarget->pos = pPeer->pos;

        const std::string message = std::format("`{}{}`` pulls `{}{}``.", pPeer->prefix, pPeer->growid, pTarget->prefix, pTarget->growid);
        peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [message](ENetPeer& peer) 
        {
            on::ConsoleMessage(&peer, message);
        });
    }
    else if (hPipe["buttonClicked"] == "kick_player")
    {
        ENetPeer *target = find_moderation_target(event, hPipe);
        if (target == nullptr) return;
        ::peer *pTarget = static_cast<::peer*>(target->data);

        /* @note same visuals as action::respawn, but aimed at the target */
        send_varlist(target, { "OnSetFreezeState", 2 }, pTarget->netid);
        send_varlist(target, { "OnKilled" }, pTarget->netid);
        send_varlist(target, {
            "OnSetPos", 
            CL_Vec2f{pTarget->rest_pos.x, pTarget->rest_pos.y}
        }, pTarget->netid, 1900);
        send_varlist(target, { "OnSetFreezeState" }, pTarget->netid, 1900);

        const std::string message = std::format("`{}{}`` kicks `{}{}``.", pPeer->prefix, pPeer->growid, pTarget->prefix, pTarget->growid);
        peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [message](ENetPeer& peer) 
        {
            on::ConsoleMessage(&peer, message);
        });
    }
    else if (hPipe["buttonClicked"] == "worldban_player")
    {
        ENetPeer *target = find_moderation_target(event, hPipe);
        if (target == nullptr) return;
        ::peer *pTarget = static_cast<::peer*>(target->data);

        world_ban_add(pPeer->recent_worlds.back(), pTarget->user_id);

        const std::string message = std::format("`{}{}`` world bans `{}{}``.", pPeer->prefix, pPeer->growid, pTarget->prefix, pTarget->growid);
        peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [message](ENetPeer& peer) 
        {
            on::ConsoleMessage(&peer, message);
        });

        on::ConsoleMessage(target, "`oYou've been `4banned`` from the world for 10 minutes!``");
        ENetEvent fake{ .peer = target };
        action::quit_to_exit(fake, "", false); // @note back to world select
    }
}