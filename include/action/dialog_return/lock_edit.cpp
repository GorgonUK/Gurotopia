#include "pch.hpp"
#include "on/ConsoleMessage.hpp"

#include "lock_edit.hpp"

void lock_edit(ENetEvent& event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
    if (world == worlds.end()) return;

    ::pos pos{};
    pos.x = atoi(hPipe["tilex"].c_str());
    pos.y = atoi(hPipe["tiley"].c_str());
    
    world->is_public = atoi(hPipe["checkbox_public"].c_str());
    if (atoi(hPipe["checkbox_disable_music"].c_str()) != 0)
        world->lock_state |= DISABLE_MUSIC;
    else world->lock_state &= ~DISABLE_MUSIC;
    world->minimum_entry_level = atoi(hPipe["minimum_entry_level"].c_str());

    ::block &block = world->blocks[cord(pos.x, pos.y)];

    if (world->is_public) 
         block.state[2] |= S_PUBLIC;
    else block.state[2] &= ~S_PUBLIC;
    on::ConsoleMessage(event.peer, std::format("`2{}`` has set the `$World Lock`` to {}``", pPeer->growid, (block.state[2] & S_PUBLIC) ? "`$PUBLIC" : "`4PRIVATE"));

    send_tile_update(event, {
        .id = block.fg,
        .punch = pos
    }, block, *world);
}

void tile_lock_edit(ENetEvent &event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
    if (world == worlds.end()) return;

    const ::pos pos{ atoi(hPipe["tilex"].c_str()), atoi(hPipe["tiley"].c_str()) };
    auto tl = std::ranges::find(world->tile_locks, pos, &::tile_lock::pos);
    if (tl == world->tile_locks.end()) return;
    if (!pPeer->role && pPeer->user_id != tl->owner) return;

    tl->is_public = atoi(hPipe["checkbox_public"].c_str()) != 0;
    world->mark_dirty();

    ::block &block = world->blocks[cord(pos.x_int(), pos.y_int())];
    if (tl->is_public) block.state[2] |= S_PUBLIC;
    else block.state[2] &= ~S_PUBLIC;

    on::ConsoleMessage(event.peer, std::format(
        "`2{}`` set their `$Area Lock`` to {}``",
        pPeer->growid, tl->is_public ? "`$PUBLIC" : "`4PRIVATE"));
    send_tile_update(event, ::state{ .punch = pos }, block, *world);
}