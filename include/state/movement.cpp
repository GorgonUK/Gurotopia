#include "pch.hpp"
#include "action/respawn.hpp"
#include "fishing.hpp"

#include "movement.hpp"

void movement(ENetEvent& event, state state) 
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    // @note moving cancels an active cast (bait already spent)
    if (pPeer->fishing)
    {
        const ::pos tile = state.pos.by_32(true);
        // @note allow tiny jitter on the same tile; leaving it cancels
        if (tile != pPeer->pos.by_32(true))
            fishing_cancel(event, "`oSit perfectly while fishing!``");
    }
    
    pPeer->pos = state.pos;
    pPeer->facing_left = state.peer_state & peer_state::S_MOVE_LEFT;

    /* add fireproof only take away 1 hp instead of 2 */
    if (state.peer_state & peer_state::S_LAVA_HIT) pPeer->pain_hp -= 2;
    if (pPeer->pain_hp <= 0) action::respawn(event, ""), pPeer->pain_hp = 10;
    
    state.netid = pPeer->netid;
    state_visuals(*event.peer, std::move(state)); // finished.
}
