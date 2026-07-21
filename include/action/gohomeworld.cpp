#include "pch.hpp"
#include "on/ConsoleMessage.hpp"
#include "join_request.hpp"

#include "gohomeworld.hpp"

void action::gohomeworld(ENetEvent &event, const std::string &header)
{
    (void)header;
    ::peer &peer = peer_of(event);

    // World-select Home button: join the saved home world, else START.
    std::string dest = peer.home_world.empty() ? "START" : peer.home_world;
    std::for_each(dest.begin(), dest.end(), [](char &c) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    });

    send_varlist(event.peer, { "OnSetFreezeState", 1 });
    on::ConsoleMessage(event.peer, std::format(
        "Magically warping to home world `5{}``...", dest
    ));
    action::join_request(event, "", dest);
}
