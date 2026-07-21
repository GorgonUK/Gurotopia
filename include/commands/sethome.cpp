#include "pch.hpp"
#include "on/ConsoleMessage.hpp"

#include "sethome.hpp"

void sethome(ENetEvent &event, const std::string_view text)
{
    (void)text;
    ::peer &peer = peer_of(event);
    ::world *world = current_world(peer);
    if (world == nullptr || world->name.empty())
    {
        on::ConsoleMessage(event.peer, "`4You must be in a world to set it as home.``");
        return;
    }

    if (!peer.home_world.empty() && peer.home_world == world->name)
    {
        peer.home_world.clear();
        peer.mark_dirty();
        on::ConsoleMessage(event.peer, std::format(
            "`w{}`` has been removed as your home world!", world->name
        ));
        return;
    }

    peer.home_world = world->name;
    peer.mark_dirty();
    on::ConsoleMessage(event.peer, std::format(
        "`w{}`` has been set as your home world!", world->name
    ));
}
