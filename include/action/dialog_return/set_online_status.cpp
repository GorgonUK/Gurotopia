#include "pch.hpp"

#include "set_online_status.hpp"

void set_online_status(ENetEvent &event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    const bool online = atoi(hPipe["checkbox_status_online"].c_str()) != 0;
    const bool busy = atoi(hPipe["checkbox_status_busy"].c_str()) != 0;
    const bool away = atoi(hPipe["checkbox_status_away"].c_str()) != 0;

    // Prefer the checked box; default Online if none (official client is exclusive).
    if (busy)
        pPeer->online_status = 1;
    else if (away)
        pPeer->online_status = 2;
    else
        pPeer->online_status = 0; // Online (also when `online` alone)

    (void)online;
    pPeer->mark_dirty();
}
