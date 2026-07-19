#include "pch.hpp"
#include "on/ConsoleMessage.hpp"
#include "tools/logger.hpp"

#include "worldlock_invite.hpp"

void worldlock_invite(ENetEvent& event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    if (hPipe["buttonClicked"] != "accept")
    {
        if (hPipe["buttonClicked"] == "decline")
        {
            on::ConsoleMessage(event.peer, "`oYou declined the World Lock invite.``");
            log_event("worldlock_decline", std::format("{}({})", pPeer->growid, pPeer->user_id),
                hPipe["world"], std::format("inviter={}", hPipe["inviter"]));
        }
        return;
    }

    const std::string world_name = hPipe["world"];
    if (world_name.empty()) return;

    auto world = std::ranges::find(worlds, world_name, &::world::name);
    if (world == worlds.end())
    {
        on::ConsoleMessage(event.peer, "`4That world is no longer loaded.``");
        return;
    }

    const int inviter_uid = atoi(hPipe["inviter_uid"].c_str());
    if (world->owner != inviter_uid)
    {
        on::ConsoleMessage(event.peer, "`4That invite is no longer valid.``");
        return;
    }

    if (pPeer->user_id == world->owner)
    {
        on::ConsoleMessage(event.peer, "`oYou already own this world.``");
        return;
    }

    if (std::ranges::find(world->access, pPeer->user_id) != world->access.end())
    {
        on::ConsoleMessage(event.peer, "`oYou already have access to this World Lock.``");
        return;
    }

    auto slot = std::ranges::find(world->access, 0);
    if (slot == world->access.end())
    {
        on::ConsoleMessage(event.peer, "`4This World Lock has no free access slots.``");
        return;
    }

    *slot = pPeer->user_id;
    world->mark_dirty();

    if (!pPeer->role && pPeer->recent_worlds.back() == world->name)
    {
        pPeer->prefix.front() = 'c';
        const std::string name = std::format("`{}{}``", pPeer->prefix, pPeer->growid);
        peers(world->name, PEER_SAME_WORLD, [pPeer, name](ENetPeer &peer)
        {
            send_varlist(&peer, { "OnNameChanged", name }, pPeer->netid);
        });
    }

    // refresh world-lock tile so access count/list updates for everyone in-world
    if (pPeer->recent_worlds.back() == world->name)
    {
        for (std::size_t i = 0; i < world->blocks.size(); ++i)
        {
            ::block &block = world->blocks[i];
            const ::item &item = id_to_item(block.fg);
            if (item.type == type::LOCK && !is_tile_lock(block.fg))
            {
                send_tile_update(event, {
                    .punch = ::pos{ static_cast<int>(i % 100), static_cast<int>(i / 100) }
                }, block, *world);
                break;
            }
        }
    }

    on::ConsoleMessage(event.peer, std::format("`2You now have access to `w{}``.``", world->name));
    peers(world->name, PEER_SAME_WORLD, [&](ENetPeer &peer)
    {
        ::peer *pOthers = static_cast<::peer*>(peer.data);
        if (pOthers && pOthers->user_id == inviter_uid)
            on::ConsoleMessage(&peer, std::format("`2{}`` accepted your World Lock invite.", pPeer->growid));
    });

    log_event("worldlock_accept", std::format("{}({})", pPeer->growid, pPeer->user_id),
        world->name, std::format("inviter_uid={}", inviter_uid));
}
