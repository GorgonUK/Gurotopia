#include "pch.hpp"
#include "tools/create_dialog.hpp"

#include "magplant.hpp"

static bool is_world_editor(::peer *pPeer, const ::world &world)
{
    if (!world.owner || pPeer->role) return true;
    if (pPeer->user_id == world.owner) return true;
    return std::ranges::find(world.access, pPeer->user_id) != world.access.end();
}

static short inventory_count(::peer *pPeer, short id)
{
    auto it = std::ranges::find(pPeer->slots, id, &::slot::id);
    return (it != pPeer->slots.end()) ? it->count : 0;
}

static ::magplant &ensure_magplant(::world &world, const ::pos &pos)
{
    auto it = std::ranges::find(world.magplants, pos, &::magplant::pos);
    if (it != world.magplants.end()) return *it;
    return world.magplants.emplace_back(pos);
}

void magplant_dialog(ENetEvent &event, const ::state &state, ::world &world, const ::item &item)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    if (!is_world_editor(pPeer, world))
    {
        send_varlist(event.peer, { "OnTextOverlay", "Only the world owner can use this Magplant." });
        return;
    }

    ::magplant &mag = ensure_magplant(world, state.punch);

    ::create_dialog dialog;
    dialog
        .set_default_color("`o")
        .add_label_with_icon("big", std::format("`w{}``", item.raw_name), item.id)
        .add_spacer("small")
        .embed_data("tilex", state.punch.x)
        .embed_data("tiley", state.punch.y);

    if (mag.id == 0)
    {
        dialog
            .add_textbox("This Magplant has no item filter set.")
            .add_item_picker("filteritem", "`wChoose item to collect``", "Pick an item the Magplant should suck up.")
            .add_spacer("small");
        send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("magplant_edit", "Close", "") });
        return;
    }

    const ::item &stock = id_to_item(mag.id);
    dialog
        .add_label_with_icon("small",
            std::format("Collecting `2{}`` — {}/{}", stock.raw_name, mag.count, ::magplant::CAPACITY), mag.id)
        .add_spacer("small")
        .add_textbox(mag.enabled ? "`2Collecting is ON``" : "`4Collecting is OFF``")
        .add_button("toggle", mag.enabled ? "Turn collecting OFF" : "Turn collecting ON")
        .add_button("retrieve", "Retrieve items to backpack")
        .add_button("clearfilter", "Clear filter (empties stock)")
        .add_item_picker("filteritem", "`wChange filter item``", "Pick a new item to collect.")
        .add_spacer("small");

    send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("magplant_edit", "Close", "") });
}

void magplant_edit(ENetEvent &event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
    if (world == worlds.end()) return;
    if (!is_world_editor(pPeer, *world)) return;

    const ::pos pos{ atoi(hPipe["tilex"].c_str()), atoi(hPipe["tiley"].c_str()) };
    ::block &block = world->blocks[cord(pos.x_int(), pos.y_int())];
    if (id_to_item(block.fg).type != type::MAGPLANT) return;

    ::magplant &mag = ensure_magplant(*world, pos);
    const std::string &clicked = hPipe["buttonClicked"];

    auto refresh = [&]{
        world->mark_dirty();
        send_tile_update(event, ::state{ .punch = pos }, block, *world);
    };

    if (!hPipe["filteritem"].empty())
    {
        const short id = static_cast<short>(atoi(hPipe["filteritem"].c_str()));
        if (id <= 0 || id == 18 || id == 32) return;
        const ::item &stock = id_to_item(id);
        if (stock.cat & CAT_UNTRADEABLE) return;
        if (stock.type == type::LOCK) return;

        // changing filter dumps old stock into backpack when possible
        if (mag.id != 0 && mag.id != static_cast<u_short>(id) && mag.count > 0)
        {
            while (mag.count > 0)
            {
                short give = std::min(mag.count, static_cast<u_short>(200));
                u_short excess = modify_item_inventory(event, ::slot(static_cast<short>(mag.id), give));
                short actually = static_cast<short>(give - excess);
                if (actually <= 0) break;
                mag.count = static_cast<u_short>(mag.count - actually);
                if (excess > 0) break;
            }
            if (mag.count > 0) return; // backpack full — keep old filter
        }

        mag.id = static_cast<u_short>(id);
        if (mag.count == 0) mag.enabled = true;
        refresh();
        return;
    }

    if (clicked == "toggle" && mag.id != 0)
    {
        mag.enabled = !mag.enabled;
        refresh();
        return;
    }

    if (clicked == "retrieve" && mag.id != 0 && mag.count > 0)
    {
        while (mag.count > 0)
        {
            short give = std::min(mag.count, static_cast<u_short>(200));
            u_short excess = modify_item_inventory(event, ::slot(static_cast<short>(mag.id), give));
            short actually = static_cast<short>(give - excess);
            if (actually <= 0) break;
            mag.count = static_cast<u_short>(mag.count - actually);
            if (excess > 0) break;
        }
        refresh();
        return;
    }

    if (clicked == "clearfilter")
    {
        if (mag.count > 0)
        {
            while (mag.count > 0)
            {
                short give = std::min(mag.count, static_cast<u_short>(200));
                u_short excess = modify_item_inventory(event, ::slot(static_cast<short>(mag.id), give));
                short actually = static_cast<short>(give - excess);
                if (actually <= 0) break;
                mag.count = static_cast<u_short>(mag.count - actually);
                if (excess > 0) break;
            }
            if (mag.count > 0) { refresh(); return; }
        }
        mag.id = 0;
        mag.enabled = false;
        refresh();
    }
}
