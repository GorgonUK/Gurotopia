#include "pch.hpp"
#include "tools/create_dialog.hpp"

#include "display_edit.hpp"

static bool is_world_editor(::peer *pPeer, const ::world &world)
{
    if (!world.owner || pPeer->role) return true;
    if (pPeer->user_id == world.owner) return true;
    return std::ranges::find(world.access, pPeer->user_id) != world.access.end();
}

void display_edit_dialog(ENetEvent &event, const ::state &state, ::world &world, const ::item &item)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    auto display = std::ranges::find(world.displays, state.punch, &::display::pos);
    const u_int shown = (display != world.displays.end()) ? display->id : 0;

    ::create_dialog dialog;
    dialog
        .set_default_color("`o")
        .add_label_with_icon("big", std::format("`w{}``", item.raw_name), item.id)
        .add_spacer("small")
        .embed_data("tilex", state.punch.x)
        .embed_data("tiley", state.punch.y);

    if (shown == 0)
    {
        dialog.add_textbox("This Display Block is empty. Punch it with an item to put one inside.");
    }
    else
    {
        const ::item &shown_item = id_to_item(static_cast<u_short>(shown));
        dialog
            .add_label_with_icon("small", std::format("`w{}``", shown_item.raw_name), shown)
            .add_spacer("small");
        if (is_world_editor(pPeer, world))
            dialog.add_button("retrieve", "`wRetrieve item``");
    }

    dialog.add_spacer("small");
    send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("display_edit", "Close", "") });
}

void display_edit(ENetEvent &event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
    if (world == worlds.end()) return;

    const ::pos pos{ atoi(hPipe["tilex"].c_str()), atoi(hPipe["tiley"].c_str()) };
    ::block &block = world->blocks[cord(pos.x_int(), pos.y_int())];
    if (id_to_item(block.fg).type != type::DISPLAY_BLOCK) return;

    if (hPipe["buttonClicked"] != "retrieve") return;
    if (!is_world_editor(pPeer, *world)) return;

    auto display = std::ranges::find(world->displays, pos, &::display::pos);
    if (display == world->displays.end() || display->id == 0) return;

    const u_short id = static_cast<u_short>(display->id);
    display->id = 0;
    world->mark_dirty();

    modify_item_inventory(event, ::slot(id, 1));
    send_tile_update(event, ::state{ .punch = pos }, block, *world);
}
