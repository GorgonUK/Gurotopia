#include "pch.hpp"
#include "tools/create_dialog.hpp"
#include "on/ConsoleMessage.hpp"

#include "vending.hpp"

/* only the World Lock owner (or staff) may stock / price / withdraw */
static bool is_vending_owner(::peer *pPeer, const ::world &world)
{
    if (!world.owner) return false; // @note unlocked worlds have no vending owner
    if (pPeer->role) return true;
    return pPeer->user_id == world.owner;
}

static short inventory_count(::peer *pPeer, short id)
{
    auto it = std::ranges::find(pPeer->slots, id, &::slot::id);
    return (it != pPeer->slots.end()) ? it->count : 0;
}

static ::vending &ensure_vending(::world &world, const ::pos &pos)
{
    auto it = std::ranges::find(world.vendings, pos, &::vending::pos);
    if (it != world.vendings.end()) return *it;
    return world.vendings.emplace_back(pos);
}

void vending_dialog(ENetEvent &event, const ::state &state, ::world &world, const ::item &item)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    const bool owner = is_vending_owner(pPeer, world);
    ::vending &vend = ensure_vending(world, state.punch);

    if (!owner)
    {
        ::create_dialog dialog;
        dialog
            .set_default_color("`o")
            .add_label_with_icon("big", std::format("`w{}``", item.raw_name), item.id)
            .add_spacer("small")
            .embed_data("tilex", state.punch.x)
            .embed_data("tiley", state.punch.y);

        if (vend.id == 0 || vend.price == 0 || vend.count == 0)
        {
            dialog.add_textbox("This machine is out of order.");
            send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("vending", "Close", "") });
            return;
        }

        const ::item &stock = id_to_item(vend.id);
        const bool per_lock = vend.price < 0;
        const int abs_price = std::abs(vend.price);
        dialog
            .add_label_with_icon("small",
                std::format("The machine contains a total of {} `2{}``.", vend.count, stock.raw_name), vend.id)
            .add_spacer("small")
            .add_textbox("For a cost of:")
            .add_label_with_icon("small",
                std::format("{} x `8World Lock``", per_lock ? 1 : abs_price), 242)
            .add_spacer("small")
            .add_textbox("You will get:")
            .add_label_with_icon("small",
                std::format("{} x `2{}``", per_lock ? abs_price : 1, stock.raw_name), vend.id)
            .add_spacer("small")
            .add_textbox(std::format("You have {} World Locks.", inventory_count(pPeer, 242)))
            .add_text_input("buycount", "How many would you like to buy?", 0, 3)
            .embed_data("expectprice", vend.price)
            .embed_data("expectitem", vend.id);

        send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("vending", "Close", "Buy") });
        return;
    }

    if (vend.id == 0 || vend.count == 0)
    {
        // @note sold out / never stocked — no item line, no "Empty"; withdraw locks + restock only
        if (vend.count == 0 && vend.id != 0)
        {
            vend.id = 0;
            vend.price = 0;
            world.mark_dirty();
            send_tile_update(event, ::state{ .punch = state.punch },
                world.blocks[cord(state.punch.x_int(), state.punch.y_int())], world);
        }

        ::create_dialog dialog;
        dialog
            .set_default_color("`o")
            .add_label_with_icon("big", std::format("`w{}``", item.raw_name), item.id)
            .add_spacer("small")
            .embed_data("tilex", state.punch.x)
            .embed_data("tiley", state.punch.y)
            .add_textbox("This machine is empty.");

        if (vend.earned > 0)
        {
            dialog
                .add_spacer("small")
                .add_smalltext(std::format("You have earned {} World Locks.", vend.earned))
                .add_button("withdraw", "Withdraw World Locks");
        }

        dialog
            .add_spacer("small")
            .add_item_picker("stockitem", "`wPut an item in``", "Choose an item to put in the machine!")
            .add_spacer("small");

        send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("vending", "Close", "") });
        return;
    }

    const ::item &stock = id_to_item(vend.id);
    const short have = inventory_count(pPeer, static_cast<short>(vend.id));
    const bool per_lock = vend.price < 0;
    const int abs_price = std::abs(vend.price);

    std::string s = std::format(
        "set_default_color|`o\n"
        "add_label_with_icon|big|`w{}``|left|{}|\n"
        "add_spacer|small|\n"
        "embed_data|tilex|{}\n"
        "embed_data|tiley|{}\n"
        "add_label_with_icon|small|The machine contains a total of {} `2{}``. |left|{}|\n"
        "add_spacer|small|\n",
        item.raw_name, item.id, state.punch.x, state.punch.y,
        vend.count, stock.raw_name, vend.id
    );

    if (vend.price == 0)
        s += "add_textbox|Not currently for sale!|left|\n";
    else
        s += std::format(
            "add_textbox|For a cost of:|left|\n"
            "add_label_with_icon|small|{} x `8World Lock``|left|242|\n"
            "add_spacer|small|\n"
            "add_textbox|You will get:|left|\n"
            "add_label_with_icon|small|{} x `2{}``|left|{}|\n",
            per_lock ? 1 : abs_price,
            per_lock ? abs_price : 1, stock.raw_name, vend.id
        );

    if (have > 0 && vend.count < 9999)
        s += std::format(
            "add_smalltext|You have {} {} in your backpack.|\n"
            "add_button|addstocks|Add them to the machine|noflags|0|0|\n",
            have, stock.raw_name
        );

    s += "add_button|pullstocks|Empty the machine|noflags|0|0|\n";

    if (vend.earned > 0)
        s += std::format(
            "add_smalltext|You have earned {} World Locks.|left|\n"
            "add_button|withdraw|Withdraw World Locks|noflags|0|0|\n",
            vend.earned
        );

    s += std::format(
        "add_spacer|small|\n"
        "add_smalltext|`5(Machine will not sell when price is set to 0)``|left|\n"
        "add_text_input|setprice|Price|{}|5|\n"
        "add_checkbox|chk_peritem|World Locks per Item|{}\n"
        "add_checkbox|chk_perlock|Items per World Lock|{}\n"
        "add_spacer|small|\n"
        "end_dialog|vending|Close|Update|\n",
        abs_price,
        vend.price >= 0 ? 1 : 0,
        vend.price < 0 ? 1 : 0
    );

    send_varlist(event.peer, { "OnDialogRequest", s });
}

void vending_edit(ENetEvent &event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
    if (world == worlds.end()) return;

    const ::pos pos{ atoi(hPipe["tilex"].c_str()), atoi(hPipe["tiley"].c_str()) };
    ::block &block = world->blocks[cord(pos.x_int(), pos.y_int())];
    if (id_to_item(block.fg).type != type::VENDING_MACHINE) return;

    ::vending &vend = ensure_vending(*world, pos);
    const bool owner = is_vending_owner(pPeer, *world);
    const std::string &clicked = hPipe["buttonClicked"];

    auto refresh = [&]{
        world->mark_dirty();
        send_tile_update(event, ::state{ .punch = pos }, block, *world);
    };

    // @note stock / price / withdraw — owner only (access list may only buy)
    if (owner && !hPipe["stockitem"].empty())
    {
        const short id = static_cast<short>(atoi(hPipe["stockitem"].c_str()));
        if (id <= 0 || id == 18 || id == 32) return;
        const ::item &stock = id_to_item(id);
        if (stock.cat & CAT_UNTRADEABLE) return;
        if (inventory_count(pPeer, id) <= 0) return;

        if (vend.id == 0)
        {
            vend.id = static_cast<u_short>(id);
            vend.count = 0;
            vend.price = 0;
        }
        else if (vend.id != static_cast<u_short>(id))
            return;

        short have = inventory_count(pPeer, id);
        short add = std::min(have, static_cast<short>(9999 - vend.count));
        if (add <= 0) return;
        modify_item_inventory(event, ::slot(id, -add));
        vend.count = static_cast<u_short>(vend.count + add);
        refresh();
        return;
    }

    if (owner && clicked == "addstocks" && vend.id != 0)
    {
        short have = inventory_count(pPeer, static_cast<short>(vend.id));
        short add = std::min(have, static_cast<short>(9999 - vend.count));
        if (add <= 0) return;
        modify_item_inventory(event, ::slot(static_cast<short>(vend.id), -add));
        vend.count = static_cast<u_short>(vend.count + add);
        refresh();
        return;
    }

    if (owner && clicked == "pullstocks" && vend.count > 0)
    {
        vend.count = give_to_backpack(event, static_cast<short>(vend.id), vend.count);
        if (vend.count == 0)
        {
            vend.id = 0;
            vend.price = 0;
        }
        refresh();
        return;
    }

    if (owner && clicked == "withdraw" && vend.earned > 0)
    {
        u_short excess = modify_item_inventory(event, ::slot(242, vend.earned));
        vend.earned = excess;
        refresh();
        return;
    }

    if (owner && clicked.empty() && !hPipe["setprice"].empty())
    {
        int price = atoi(hPipe["setprice"].c_str());
        if (price < 0) price = 0;
        const bool per_lock = atoi(hPipe["chk_perlock"].c_str()) != 0;
        vend.price = (price > 0 && per_lock) ? -price : price;
        refresh();
        return;
    }

    // @note buy path — anyone who is not the configuring owner
    if (!owner && !hPipe["buycount"].empty())
    {
        if (vend.id == 0 || vend.price == 0 || vend.count == 0) return;
        if (atoi(hPipe["expectitem"].c_str()) != vend.id) return;
        if (atoi(hPipe["expectprice"].c_str()) != vend.price) return;

        int buycount = atoi(hPipe["buycount"].c_str());
        if (buycount <= 0) return;

        const bool per_lock = vend.price < 0;
        const int abs_price = std::abs(vend.price);

        int wl_cost = 0;
        int items_out = 0;
        if (per_lock)
        {
            wl_cost = buycount;
            items_out = buycount * abs_price;
        }
        else
        {
            items_out = buycount;
            wl_cost = buycount * abs_price;
        }

        if (items_out > vend.count || wl_cost <= 0) return;
        if (inventory_count(pPeer, 242) < wl_cost) return;

        modify_item_inventory(event, ::slot(242, static_cast<short>(-wl_cost)));
        int remaining = give_to_backpack(event, static_cast<short>(vend.id), static_cast<u_short>(items_out));
        vend.count = static_cast<u_short>(vend.count - (items_out - remaining));
        if (remaining > 0)
        {
            int refund_wl = per_lock
                ? (remaining + abs_price - 1) / abs_price
                : remaining * abs_price;
            refund_wl = std::min(refund_wl, wl_cost);
            modify_item_inventory(event, ::slot(242, static_cast<short>(refund_wl)));
            wl_cost -= refund_wl;
        }

        vend.earned = static_cast<u_short>(std::min(65535, static_cast<int>(vend.earned) + wl_cost));
        if (vend.count == 0)
        {
            vend.id = 0;
            vend.price = 0;
        }
        refresh();
        on::ConsoleMessage(event.peer, "`2Purchase complete!``");
    }
}
