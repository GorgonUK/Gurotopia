#include "pch.hpp"

#include "CountryState.hpp"

void on::CountryState(ENetEvent& event) 
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    // @note OnSpawn already carries country|; this refreshes the local flag / maxLevel.
    // Empty country hides the flag — callers should have set it from login.
    std::string state = pPeer->country.empty() ? "us" : pPeer->country;
    if (pPeer->level[0] == 125)
        state += "|maxLevel";
    
    send_varlist(event.peer, {
        "OnCountryState",
        state
    }, pPeer->netid);
}