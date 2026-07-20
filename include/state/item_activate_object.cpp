#include "pch.hpp"
#include "on/ConsoleMessage.hpp"

#include "item_activate_object.hpp"

namespace
{
    // Tell every peer in the world to drop the floating visual. The Growtopia client
    // keys visuals by the id it was given at spawn / in the pickup request — which
    // can differ from our server-side uid after world switches — so we may need both.
    void send_pickup_remove(ENetEvent& event, int netid, signed visual_id)
    {
        if (visual_id == 0) return;
        item_change_object(event, ::state{
            .netid = netid,
            .uid   = static_cast<int>(0xffffffff),
            .id    = visual_id
        });
    }

    void erase_object_by_uid(::world &world, u_int object_uid)
    {
        auto stale = std::ranges::find(world.objects, object_uid, &::object::uid);
        if (stale != world.objects.end())
            world.objects.erase(stale);
        world.mark_dirty();
    }
}

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

    // Fallback when the client uid desyncs after switching worlds: pick the drop
    // under the player's feet (same tile).
    if (object == world->objects.end())
    {
        const ::pos tile = pPeer->pos.by_32(true);
        object = std::ranges::find_if(world->objects, [&](const ::object &o) {
            return o.pos.by_32(true) == tile;
        });
    }

    if (object == world->objects.end()) return;

    // @note snapshot before any call that can reallocate world->objects
    const u_int   object_uid   = object->uid;
    const u_short object_id    = object->id;
    const u_short object_count = object->count;
    const ::pos   object_pos   = object->pos;
    // Prefer the id the client asked to pick up — that is what its visual is keyed by.
    const signed  client_visual_id = static_cast<signed>(state.id);

    auto clear_ground_visual = [&]()
    {
        send_pickup_remove(event, pPeer->netid, client_visual_id);
        const signed server_id = static_cast<signed>(object_uid);
        if (server_id != client_visual_id &&
            server_id != static_cast<signed>(static_cast<u_int>(client_visual_id) & 0x00ffffffu))
        {
            send_pickup_remove(event, pPeer->netid, server_id);
        }
        // Also try the masked low-24 form (client sometimes stores flags only locally).
        const signed masked = static_cast<signed>(static_cast<u_int>(client_visual_id) & 0x00ffffffu);
        if (masked != 0 && masked != client_visual_id && masked != server_id)
            send_pickup_remove(event, pPeer->netid, masked);
    };

    if (object_id == 112/*gem*/)
    {
        pPeer->credit_gems(event, object_count);
        clear_ground_visual();
        erase_object_by_uid(*world, object_uid);
        return;
    }

    const ::item &item = id_to_item(object_id);
    const u_short remaining = pPeer->emplace(::slot(object_id, object_count));
    const u_short collected = static_cast<u_short>(object_count - remaining);

    if (collected == 0)
    {
        on::ConsoleMessage(event.peer,
            "`oYour inventory is full (or that stack is maxed). Free a slot to pick this up.``");
        return; // leave the floating item in the world
    }

    // Clear the floating visual first so the client does not keep a ghost drop.
    if (remaining == 0)
        clear_ground_visual();

    // Tell this client their inventory gained `collected` (do not re-emplace).
    {
        ::state inv{.type = (collected << 24) | 0x000d, .id = object_id};
        send_data(*event.peer, compress_state(inv));
    }

    on::ConsoleMessage(event.peer, (item.rarity >= 999) ?
        std::format("Collected `w{} {}``.",                collected, item.raw_name) :
        std::format("Collected `w{} {}``. Rarity: `w{}``", collected, item.raw_name, item.rarity)
    );

    if (remaining == 0)
    {
        erase_object_by_uid(*world, object_uid);
    }
    else
    {
        // Keep the same uid; only shrink the stack. Never call add_object here —
        // that used to merge the remainder back onto this stack and double it.
        auto live = std::ranges::find(world->objects, object_uid, &::object::uid);
        if (live != world->objects.end())
        {
            live->count = remaining;
            // Update count on the visual the client already has (prefer client id).
            item_change_object(event, ::state{
                .netid = static_cast<int>(0xfffffffd),
                .uid   = client_visual_id != 0 ? client_visual_id : static_cast<int>(object_uid),
                .count = static_cast<float>(remaining),
                .id    = object_id,
                .pos   = object_pos
            });
        }
        world->mark_dirty();
    }
}
