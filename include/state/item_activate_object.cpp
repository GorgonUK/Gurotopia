#include "pch.hpp"
#include "on/SetBux.hpp"
#include "on/ConsoleMessage.hpp"

#include "item_activate_object.hpp"

void item_activate_object(ENetEvent& event, state state) 
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
    if (world == worlds.end()) return;

    // @note the client packs flag bits into the high byte of the object id on
    // freshly-dropped items (e.g. 0xFA000001 for uid 1), so match tolerantly:
    // exact value, then low 24 bits, then the uid field as a last resort.
    auto by_uid = [&](u_int key) { return std::ranges::find(world->objects, key, &::object::uid); };

    auto object = by_uid(static_cast<u_int>(state.id));
    if (object == world->objects.end())
        object = by_uid(static_cast<u_int>(state.id) & 0x00ffffffu);
    if (object == world->objects.end() && state.uid != 0)
        object = by_uid(static_cast<u_int>(state.uid));

    if (object == world->objects.end()) return;

    // @note snapshot before any call that can reallocate world->objects and dangle 'object'
    const signed  object_uid   = object->uid;
    const u_short object_id    = object->id;
    const u_short object_count = object->count;
    const ::pos   object_pos   = object->pos;

    u_short remaining = 0;

    if (object_id != 112/*gem*/)
    {
        const ::item &item = id_to_item(object_id);

        remaining = pPeer->emplace(::slot(object_id, object_count)); // @return remains after reaching 200
        object->count = remaining; // @note update before add_object; iterator still valid here
        if (remaining > 0)
        {
            add_object(event, ::slot(object_id, remaining), object_pos, *world); // @note may reallocate world->objects -> 'object' dangles after this
        }
        u_short collected = object_count - remaining;
        if (collected ==/*unsigned*/ 0) return; // @todo

        on::ConsoleMessage(event.peer, (item.rarity >= 999) ?
            std::format("Collected `w{} {}``.",                collected, item.raw_name) :
            std::format("Collected `w{} {}``. Rarity: `w{}``", collected, item.raw_name, item.rarity)
        );
    }
    else 
    {
        pPeer->gems += object_count;
        pPeer->mark_dirty();
        on::SetBux(event);
    }
    remove_object(event, object_uid);

    if (remaining == 0)
    {
        // @note re-find because add_object/remove_object may have invalidated the earlier iterator
        auto stale = std::ranges::find(world->objects, object_uid, &::object::uid);
        if (stale != world->objects.end()) world->objects.erase(stale);
    }
}
