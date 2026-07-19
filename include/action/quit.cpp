#include "pch.hpp"
#include "action/quit_to_exit.hpp"
#include "quit.hpp"

void action::quit(ENetEvent& event, const std::string& header) 
{
    action::quit_to_exit(event, "", true);
    
    if (event.peer == nullptr) return;
    if (event.peer->data != nullptr) 
    {
        ::peer *pPeer = static_cast<::peer*>(event.peer->data);
        pPeer->mysql_save_progress();
        delete pPeer;
        event.peer->data = nullptr;
    }
    enet_peer_reset(event.peer);
}