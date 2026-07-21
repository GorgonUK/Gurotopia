#include "pch.hpp"
#include "NameChanged.hpp"

void on::NameChanged(ENetEvent& event) 
{
    ::peer &pPeer = peer_of(event);

    const std::string title_texture =
        pPeer.title_old_timer ? "game/tiles_page7.rttex" : "";
    const std::string title_coords =
        pPeer.title_old_timer ? "8,6" : "";

    send_varlist(event.peer, {
        "OnNameChanged",
        std::format("`{}{}``", pPeer.prefix, pPeer.growid),
        std::format(
            "{{\"PlayerWorldID\":{},\"TitleTexture\":\"{}\",\"TitleTextureCoordinates\":\"{}\","
            "\"WrenchCustomization\":{{\"WrenchForegroundCanRotate\":{},\"WrenchForegroundID\":{},"
            "\"WrenchIconID\":{}}}}}",
            pPeer.netid,
            title_texture,
            title_coords,
            pPeer.wrench_foreground_can_rotate ? "true" : "false",
            pPeer.wrench_foreground_id,
            pPeer.wrench_icon_id
        )
    }, pPeer.netid);
}
