#include "pch.hpp"
#include "on/SetClothing.hpp"
#include "commands/punch.hpp"
#include "on/ConsoleMessage.hpp"
#include "action/dialog_return/magplant.hpp"
#include "fishing.hpp"

#include "item_activate.hpp"

void item_activate(ENetEvent& event, state state)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    const ::item &item = id_to_item(state.id);
    if (item.cloth_type != clothing::none) 
    {
        float &current_cloth = pPeer->clothing[item.cloth_type]; // @note ID of the current clothing being changed

        current_cloth = (current_cloth == state.id) ? 0 : state.id;

        // @note putting the fishing rod away ends an active cast
        if (pPeer->fishing && item.cloth_type == hand && static_cast<short>(current_cloth) != 2912)
            fishing_cancel(event, "`oYou put your rod away.``");

        pPeer->update_effects();
        pPeer->mark_dirty();
        
        /* @note this is so we can add the latest punch effect (if any) */
        u_short punch_id = get_punch_id(static_cast<u_int>(current_cloth));
        if (punch_id != 0)
            pPeer->punch_effect = punch_id;

        send_varlist(event.peer, { "OnEquipNewItem", state.id }, pPeer->netid);
        on::SetClothing(*event.peer); // @todo
    }
    else if (item.type == type::MAGPLANT_REMOTE)
    {
        // @note open the first Magplant in this world the peer can edit
        auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
        if (world == worlds.end() || world->magplants.empty())
        {
            send_varlist(event.peer, { "OnTextOverlay", "No Magplant found in this world." });
            return;
        }

        const bool can_edit = !world->owner || pPeer->role || pPeer->user_id == world->owner ||
            std::ranges::find(world->access, pPeer->user_id) != world->access.end();
        if (!can_edit)
        {
            send_varlist(event.peer, { "OnTextOverlay", "Only the world owner can use this Magplant Remote." });
            return;
        }

        ::magplant &mag = world->magplants.front();
        ::block &block = world->blocks[cord(mag.pos.x_int(), mag.pos.y_int())];
        const ::item &mag_item = id_to_item(block.fg);
        magplant_dialog(event, ::state{ .punch = mag.pos }, *world, mag_item);
    }
    else 
    {
        const auto item = std::ranges::find(pPeer->slots, state.id, &::slot::id);
        if (item == pPeer->slots.end()) return;
        
        if (item->id == 242 && item->count >= 100) 
        {
            const u_short nokori = modify_item_inventory(event, {1796, 1});
            if (nokori == 0) 
            {
                modify_item_inventory(event, {item->id, -100});
                const std::string compressed = "You compressed 100 `2World Lock`` into a `2Diamond Lock``!";
                send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, compressed, 0u, 1u });
                on::ConsoleMessage(event.peer, compressed);
            }
        }
        else if (item->id == 1796 && item->count >= 1)
        {
            const u_short nokori = modify_item_inventory(event, {242, 100});
            short hyaku = 100 - nokori;
            if (hyaku == 100) 
            {
                modify_item_inventory(event, {item->id, -1});
                const std::string shattered = "You shattered a `2Diamond Lock`` into 100 `2World Lock``!";
                send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, shattered, 0u, 1u });
                on::ConsoleMessage(event.peer, shattered);
            }
            else modify_item_inventory(event, ::slot(242, -hyaku)); // @note return wls if can't hold 100
        }
        else if (item->id == 1486 && item->count >= 100) 
        {
            const u_short nokori = modify_item_inventory(event, {6802, 1});
            if (nokori == 0) 
                modify_item_inventory(event, {item->id, -100});
        }
        else if (item->id == 6802 && item->count >= 1)
        {
            const u_short nokori = modify_item_inventory(event, {1486, 100});
            short hyaku = 100 - nokori;
            if (hyaku == 100) 
                modify_item_inventory(event, {item->id, -1});
            else 
                modify_item_inventory(event, ::slot(1486, -hyaku)); // @note return growtoken if can't hold 100
        }
    }
}
