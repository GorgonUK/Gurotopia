#include "pch.hpp"
#include "action/quit_to_exit.hpp"
#include "tools/logger.hpp"
#include "quit.hpp"

void action::quit(ENetEvent& event, const std::string& header) 
{
    action::quit_to_exit(event, "", true);
    
    if (event.peer == nullptr) return;
    if (event.peer->data != nullptr) 
    {
        ::peer *pPeer = static_cast<::peer*>(event.peer->data);
        log_event("logout", std::format("{}({})", pPeer->growid, pPeer->user_id),
            pPeer->recent_worlds.back().empty() ? "" : pPeer->recent_worlds.back(), "disconnect");
        pPeer->mysql_save_progress();
        delete pPeer;
        event.peer->data = nullptr;
    }
    enet_peer_reset(event.peer);
}