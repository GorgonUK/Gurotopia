#include "pch.hpp"
#include "on/RequestWorldSelectMenu.hpp"
#include "on/RequestGazette.hpp"
#include "on/ConsoleMessage.hpp"
#include "on/SetBux.hpp"
#include "on/FtueButtonDataSet.hpp"
#include "tools/create_dialog.hpp"
#include "automate/holiday.hpp"

#include "enter_game.hpp"

void action::enter_game(ENetEvent& event, const std::string& header) 
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    // Capture before starter-kit grant — FTUE is only for the first authenticated entry.
    const bool first_login = !pPeer->inventory_initialized;

    if (first_login) // @note brand-new account: grant starter kit once
    {
        pPeer->emplace({18, 1}); // @note Fist
        pPeer->emplace({32, 1}); // @note Wrench
        pPeer->emplace({9640, 1}); // @note My First World Lock
        pPeer->inventory_initialized = true;
        pPeer->mark_dirty();
    }
    pPeer->prefix = (pPeer->role == MODERATOR) ? "#@" : (pPeer->role == DEVELOPER) ? "8@" : pPeer->prefix;
    on::ConsoleMessage(event.peer, 
        std::format("Welcome back, `{}{}````. No friends are online.", 
            pPeer->prefix, pPeer->growid
        )
    );
    on::ConsoleMessage(event.peer, holiday_greeting().second);
    on::ConsoleMessage(event.peer, "`5Personal Settings active:`` `#Can customize profile``");
    
    send_inventory_state(event);
    on::SetBux(event);
    send_varlist(event.peer, { "SetHasGrowID", 1, pPeer->growid.c_str(), "" });
    {
        std::tm time = localtime();

        send_varlist(event.peer, {
            "OnTodaysDate",
            time.tm_mon + 1,
            time.tm_mday,
            0u, // @todo
            0u // @todo
        });
    } // @note delete time

    on::RequestWorldSelectMenu(event);
    on::RequestGazette(event);

    if (first_login)
        on::FtueButtonDataSet(event);

    send_data(*event.peer, compress_state(::state{
        .type = 0x16 // @noote PACKET_PING_REQUEST
    }));
    /* for v5.47+ client */
    send_varlist(event.peer, {
        "OnSetFeatureEnableFlags",
        "EA8DEAcGAgEOBQgKCQ0MEQQ=" // @todo Dw0JEQQMEAMPAgYBDgUICg==
    });
}
