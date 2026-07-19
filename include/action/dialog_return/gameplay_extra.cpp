#include "pch.hpp"
#include "tools/create_dialog.hpp"
#include "on/ConsoleMessage.hpp"
#include "on/SetBux.hpp"
#include "tools/ransuu.hpp"

#include "gameplay_extra.hpp"

/* ---------- tile lock edit ---------- */

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

/* ---------- trade ---------- */

static ENetPeer *find_peer_by_netid(const std::string &world_name, short netid)
{
    ENetPeer *found{};
    peers(world_name, PEER_SAME_WORLD, [netid, &found](ENetPeer &peer)
    {
        if (static_cast<::peer*>(peer.data)->netid == netid)
            found = &peer;
    });
    return found;
}

static void clear_trade(::peer *p)
{
    p->trade_with_netid = -1;
    p->trade_offer.clear();
    p->trade_ready = false;
    p->trade_confirmed = false;
}

static std::string format_offer(const std::vector<::slot> &offer)
{
    if (offer.empty()) return "`o(nothing)``";
    std::string out;
    for (const ::slot &s : offer)
    {
        const ::item &item = id_to_item(s.id);
        out += std::format("`w{}`` x{}  ", item.raw_name, s.count);
    }
    return out;
}

static void send_trade_ui(ENetEvent &event, ENetPeer *other)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    ::peer *pOther = static_cast<::peer*>(other->data);

    ::create_dialog dialog;
    dialog
        .set_default_color("`o")
        .add_label_with_icon("big", std::format("`wTrade with {}``", pOther->growid), 1366)
        .embed_data("netID", pOther->netid)
        .add_spacer("small")
        .add_textbox(std::format("`wYour offer:`` {}", format_offer(pPeer->trade_offer)))
        .add_textbox(std::format("`wTheir offer:`` {}", format_offer(pOther->trade_offer)))
        .add_spacer("small")
        .add_text_input("item_id", "Item ID:", "", 6)
        .add_text_input("item_count", "Count:", "1", 3)
        .add_button("add_item", "`wAdd item to offer``")
        .add_button("clear_offer", "`4Clear my offer``")
        .add_spacer("small");

    if (!pPeer->trade_ready)
        dialog.add_button("ready", "`2Ready``");
    else
        dialog.add_textbox("`2You are ready. Waiting on the other player...``");

    dialog
        .add_button("cancel_trade", "`4Cancel trade``")
        .add_spacer("small")
        .add_quick_exit();

    send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("trade_edit", "", "") });
}

void trade_dialog(ENetEvent &event, ENetPeer *target)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    ::peer *pTarget = static_cast<::peer*>(target->data);

    if (pPeer->trade_with_netid >= 0 || pTarget->trade_with_netid >= 0)
    {
        on::ConsoleMessage(event.peer, "`4One of you is already trading.``");
        return;
    }

    pPeer->trade_with_netid = pTarget->netid;
    pTarget->trade_with_netid = pPeer->netid;
    clear_trade(pPeer); // resets offer/flags but we need netid
    pPeer->trade_with_netid = pTarget->netid;
    clear_trade(pTarget);
    pTarget->trade_with_netid = pPeer->netid;

    on::ConsoleMessage(event.peer, std::format("`2Trade started with `w{}``.``", pTarget->growid));
    on::ConsoleMessage(target, std::format("`2Trade started with `w{}``.``", pPeer->growid));

    send_trade_ui(event, target);
    ENetEvent other_ev{ .peer = target };
    send_trade_ui(other_ev, event.peer);
}

static bool inventory_has(::peer *p, short id, short count)
{
    auto it = std::ranges::find(p->slots, id, &::slot::id);
    return it != p->slots.end() && it->count >= count;
}

static void finalize_trade(ENetPeer *a, ENetPeer *b)
{
    ::peer *pa = static_cast<::peer*>(a->data);
    ::peer *pb = static_cast<::peer*>(b->data);

    // validate both still hold what they offered
    for (const ::slot &s : pa->trade_offer)
    {
        if (id_to_item(s.id).cat & CAT_UNTRADEABLE) { clear_trade(pa); clear_trade(pb); return; }
        if (!inventory_has(pa, s.id, s.count)) { clear_trade(pa); clear_trade(pb); return; }
    }
    for (const ::slot &s : pb->trade_offer)
    {
        if (id_to_item(s.id).cat & CAT_UNTRADEABLE) { clear_trade(pa); clear_trade(pb); return; }
        if (!inventory_has(pb, s.id, s.count)) { clear_trade(pa); clear_trade(pb); return; }
    }

    ENetEvent ea{ .peer = a }, eb{ .peer = b };

    for (const ::slot &s : pa->trade_offer)
    {
        modify_item_inventory(ea, ::slot(s.id, static_cast<short>(-s.count)));
        modify_item_inventory(eb, ::slot(s.id, s.count));
    }
    for (const ::slot &s : pb->trade_offer)
    {
        modify_item_inventory(eb, ::slot(s.id, static_cast<short>(-s.count)));
        modify_item_inventory(ea, ::slot(s.id, s.count));
    }

    on::ConsoleMessage(a, "`2Trade complete!``");
    on::ConsoleMessage(b, "`2Trade complete!``");
    clear_trade(pa);
    clear_trade(pb);
}

void trade_edit(ENetEvent &event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    if (pPeer->trade_with_netid < 0) return;

    ENetPeer *other = find_peer_by_netid(pPeer->recent_worlds.back(), static_cast<short>(pPeer->trade_with_netid));
    if (other == nullptr)
    {
        clear_trade(pPeer);
        on::ConsoleMessage(event.peer, "`4Trade canceled — other player left.``");
        return;
    }
    ::peer *pOther = static_cast<::peer*>(other->data);

    const std::string &btn = hPipe["buttonClicked"];

    if (btn == "cancel_trade")
    {
        clear_trade(pPeer);
        clear_trade(pOther);
        on::ConsoleMessage(event.peer, "`4Trade canceled.``");
        on::ConsoleMessage(other, "`4Trade canceled.``");
        return;
    }

    if (btn == "clear_offer")
    {
        pPeer->trade_offer.clear();
        pPeer->trade_ready = false;
        pOther->trade_ready = false;
        send_trade_ui(event, other);
        ENetEvent oev{ .peer = other };
        send_trade_ui(oev, event.peer);
        return;
    }

    if (btn == "add_item")
    {
        const short id = static_cast<short>(atoi(hPipe["item_id"].c_str()));
        short count = static_cast<short>(atoi(hPipe["item_count"].c_str()));
        if (id <= 0 || count <= 0) return;
        count = std::clamp(count, short{1}, short{200});

        const ::item &item = id_to_item(id);
        if (item.id == 0 || (item.cat & CAT_UNTRADEABLE))
        {
            on::ConsoleMessage(event.peer, "`4That item can't be traded.``");
            return;
        }
        if (!inventory_has(pPeer, id, count))
        {
            on::ConsoleMessage(event.peer, "`4You don't have that many.``");
            return;
        }
        if (pPeer->trade_offer.size() >= 4ull)
        {
            on::ConsoleMessage(event.peer, "`4Offer is full (max 4 stacks).``");
            return;
        }

        // merge into existing offer stack if same id
        auto it = std::ranges::find(pPeer->trade_offer, id, &::slot::id);
        if (it != pPeer->trade_offer.end())
            it->count = static_cast<short>(std::min(200, it->count + count));
        else
            pPeer->trade_offer.emplace_back(id, count);

        pPeer->trade_ready = false;
        pOther->trade_ready = false;
        send_trade_ui(event, other);
        ENetEvent oev{ .peer = other };
        send_trade_ui(oev, event.peer);
        return;
    }

    if (btn == "ready")
    {
        pPeer->trade_ready = true;
        if (pPeer->trade_ready && pOther->trade_ready)
        {
            // both ready — confirm dialog
            ::create_dialog confirm;
            confirm
                .set_default_color("`o")
                .add_label_with_icon("big", "`wConfirm Trade``", 1366)
                .embed_data("netID", pOther->netid)
                .add_spacer("small")
                .add_textbox(std::format("`4You'll give:`` {}", format_offer(pPeer->trade_offer)))
                .add_textbox(std::format("`2You'll get:`` {}", format_offer(pOther->trade_offer)))
                .add_spacer("small")
                .add_button("do_trade", "`2Accept trade``")
                .add_button("cancel_trade", "`4Cancel``");
            send_varlist(event.peer, { "OnDialogRequest", confirm.end_dialog("trade_edit", "", "") });

            ::create_dialog confirm2;
            confirm2
                .set_default_color("`o")
                .add_label_with_icon("big", "`wConfirm Trade``", 1366)
                .embed_data("netID", pPeer->netid)
                .add_spacer("small")
                .add_textbox(std::format("`4You'll give:`` {}", format_offer(pOther->trade_offer)))
                .add_textbox(std::format("`2You'll get:`` {}", format_offer(pPeer->trade_offer)))
                .add_spacer("small")
                .add_button("do_trade", "`2Accept trade``")
                .add_button("cancel_trade", "`4Cancel``");
            send_varlist(other, { "OnDialogRequest", confirm2.end_dialog("trade_edit", "", "") });
            return;
        }
        send_trade_ui(event, other);
        ENetEvent oev{ .peer = other };
        send_trade_ui(oev, event.peer);
        return;
    }

    if (btn == "do_trade")
    {
        pPeer->trade_confirmed = true;
        if (pPeer->trade_confirmed && pOther->trade_confirmed)
            finalize_trade(event.peer, other);
        else
            on::ConsoleMessage(event.peer, "`2Waiting for the other player to confirm...``");
    }
}

/* ---------- chemical combiner / sewing ---------- */

/*
    E-Z Cook Oven / Laboratory flow (matches real GT):
    - S_TOGGLE set  => OPEN  (nonsolid; drops on the tile are visible)
    - S_TOGGLE clear => CLOSED (solid; contents hidden in world.combiners)
    Punch toggles. Closing swallows drops on/near the tile. Opening cooks
    matching recipes and spits results back onto the oven tile.
*/

namespace
{
    struct resolved_recipe
    {
        short result{};
        short result_count{};
        std::array<::slot, 3ull> in{ ::slot{0,0}, ::slot{0,0}, ::slot{0,0} };
    };

    /* @credit https://growtopia.fandom.com/wiki/Guide:Combining — IDs from items.dat */
    const resolved_recipe COMBINER_RECIPES[]
    {
        // Mysterious Chemical = 20Y + 10B + 5P
        { 922, 1, {{ ::slot{924, 20}, ::slot{920, 10}, ::slot{918, 5} }} },
        // Thingamajig
        { 1310, 1, {{ ::slot{922, 1}, ::slot{924, 4}, ::slot{918, 3} }} },
        // Enhanced Intellect
        { 908, 1, {{ ::slot{922, 3}, ::slot{924, 3}, ::slot{920, 3} }} },
        // Combover
        { 770, 1, {{ ::slot{924, 10}, ::slot{916, 5}, ::slot{914, 5} }} },
        // Steel Table x5
        { 926, 5, {{ ::slot{914, 10}, ::slot{920, 5}, ::slot{924, 5} }} },
        // Shockinator x3
        { 902, 3, {{ ::slot{914, 10}, ::slot{916, 5}, ::slot{918, 1} }} },
        // Slime x4
        { 978, 4, {{ ::slot{914, 15}, ::slot{918, 2}, ::slot{920, 4} }} },
        // Lab Coat
        { 772, 1, {{ ::slot{916, 20}, ::slot{918, 20}, ::slot{924, 5} }} },
        // Ze Goggles
        { 904, 1, {{ ::slot{916, 2}, ::slot{920, 4}, ::slot{918, 2} }} },
        // Shocking Hair
        { 906, 1, {{ ::slot{914, 30}, ::slot{916, 4}, ::slot{920, 1} }} },
        // Dragon Tail
        { 1114, 1, {{ ::slot{914, 6}, ::slot{918, 2}, ::slot{924, 3} }} },
    };

    ::combiner *combiner_at(::world &world, ::pos tile)
    {
        const int tx = tile.x_int(), ty = tile.y_int();
        for (::combiner &c : world.combiners)
            if (c.pos.x_int() == tx && c.pos.y_int() == ty)
                return &c;
        return nullptr;
    }

    ::combiner &combiner_get(::world &world, ::pos tile)
    {
        if (::combiner *c = combiner_at(world, tile))
            return *c;
        world.combiners.emplace_back(::pos{static_cast<float>(tile.x_int()), static_cast<float>(tile.y_int())});
        return world.combiners.back();
    }

    std::vector<::slot> combine_contents(std::vector<::slot> &contents)
    {
        auto stack_of = [&contents](short id) -> ::slot* {
            auto it = std::ranges::find(contents, id, &::slot::id);
            return (it != contents.end()) ? &*it : nullptr;
        };

        std::vector<::slot> results;
        for (const resolved_recipe &rec : COMBINER_RECIPES)
        {
            int crafts = 200;
            for (const ::slot &need : rec.in)
            {
                ::slot *have = stack_of(need.id);
                crafts = std::min(crafts, (have != nullptr) ? have->count / need.count : 0);
            }
            if (crafts <= 0) continue;

            for (const ::slot &need : rec.in)
                stack_of(need.id)->count = static_cast<short>(stack_of(need.id)->count - need.count * crafts);

            results.emplace_back(rec.result, static_cast<short>(rec.result_count * crafts));
        }
        std::erase_if(contents, [](const ::slot &s) { return s.count <= 0; });
        return results;
    }

    /* drop a stack centered on the oven tile (no scatter off-tile) */
    void spawn_on_oven(ENetEvent &event, ::slot s, ::pos tile, ::world &world)
    {
        const ::pos at{ tile.x * 32.0f + 8.0f, tile.y * 32.0f + 8.0f };
        while (s.count > 0)
        {
            const short give = std::min<short>(s.count, 200);
            add_object(event, ::slot(s.id, give), at, world);
            s.count -= give;
        }
    }

    bool same_or_adjacent_tile(const ::pos &obj_pixel, ::pos tile)
    {
        const ::pos t = obj_pixel.by_32(true);
        return std::abs(t.x_int() - tile.x_int()) <= 1 && std::abs(t.y_int() - tile.y_int()) <= 1;
    }
}

void combiner_toggle(ENetEvent &event, const ::state &state, ::block &block, ::world &world)
{
    const bool is_open = (block.state[2] & S_TOGGLE) != 0;
    ::combiner &comb = combiner_get(world, state.punch);

    if (is_open)
    {
        // After a cook, products sit on the oven — punch once to collect them.
        if (comb.output_ready)
        {
            int collected_stacks = 0;
            for (std::size_t i = 0; i < world.objects.size(); )
            {
                ::object &obj = world.objects[i];
                if (!(obj.pos.by_32(true) == state.punch)) { ++i; continue; }

                const short id = static_cast<short>(obj.id);
                const short count = static_cast<short>(obj.count);
                const u_int uid = obj.uid;

                world.objects.erase(world.objects.begin() + i);
                despawn_object(event, static_cast<signed>(uid));
                modify_item_inventory(event, ::slot(id, count));
                on::ConsoleMessage(event.peer, std::format("`2Collected `w{} {}`` from the oven.``", count, id_to_item(id).raw_name));
                ++collected_stacks;
            }
            comb.output_ready = false;
            world.mark_dirty();
            if (collected_stacks > 0)
                return; // stay open
            // fall through to close if the drops somehow vanished
        }

        // Close and swallow ingredients on/near the tile
        int swallowed = 0;
        for (std::size_t i = 0; i < world.objects.size(); )
        {
            const ::object &obj = world.objects[i];
            if (!same_or_adjacent_tile(obj.pos, state.punch)) { ++i; continue; }

            auto stored = std::ranges::find(comb.contents, static_cast<short>(obj.id), &::slot::id);
            if (stored != comb.contents.end())
                stored->count = static_cast<short>(std::min(30000, stored->count + obj.count));
            else if (comb.contents.size() < 100ull)
                comb.contents.emplace_back(obj.id, obj.count);
            else { ++i; continue; }

            const u_int uid = obj.uid;
            world.objects.erase(world.objects.begin() + i);
            despawn_object(event, static_cast<signed>(uid));
            ++swallowed;
        }

        block.state[2] &= ~S_TOGGLE;
        comb.output_ready = false;
        world.mark_dirty();
        send_tile_update(event, state, block, world);

        if (swallowed > 0)
            on::ConsoleMessage(event.peer, std::format("`oThe oven closes and swallows `w{}`` stack(s)...``", swallowed));
        else
            on::ConsoleMessage(event.peer, "`oOven closed. Drop ingredients on it while open, then close it to cook.``");
        return;
    }

    // CLOSED -> open: cook, then drop results onto the oven tile
    std::vector<::slot> output;

    if (!comb.contents.empty())
    {
        std::vector<::slot> results = combine_contents(comb.contents);
        for (const ::slot &s : results)
        {
            output.push_back(s);
            on::ConsoleMessage(event.peer, std::format("`2The oven created `w{} {}``!``", s.count, id_to_item(s.id).raw_name));
        }
        for (const ::slot &s : comb.contents)
            output.push_back(s);

        if (results.empty() && !output.empty())
            on::ConsoleMessage(event.peer, "`oNothing combined — your ingredients are returned.``");
        else if (!results.empty() && !comb.contents.empty())
            on::ConsoleMessage(event.peer, "`oLeftover ingredients came back out.``");

        comb.contents.clear();
    }

    block.state[2] |= S_TOGGLE;
    comb.output_ready = !output.empty();
    world.mark_dirty();
    send_tile_update(event, state, block, world);

    for (const ::slot &s : output)
        spawn_on_oven(event, s, state.punch, world);

    if (comb.output_ready)
        on::ConsoleMessage(event.peer, "`oPunch the open oven again to collect everything inside.``");
}

void combiner_dialog(ENetEvent &event, const ::state &state, ::world &world, const ::item &item)
{
    ::create_dialog dialog;
    dialog
        .set_default_color("`o")
        .add_label_with_icon("big", std::format("`w{}``", item.raw_name), item.id)
        .embed_data("tilex", state.punch.x)
        .embed_data("tiley", state.punch.y)
        .add_spacer("small");

    if (item.type == type::CHEMICAL_COMBINER)
    {
        ::block &block = world.blocks[cord(state.punch.x_int(), state.punch.y_int())];
        const bool closed = (block.state[2] & S_TOGGLE) == 0;

        dialog.add_textbox(std::format("The machine is {}``.", closed ? "`4CLOSED" : "`2OPEN"));
        dialog.add_textbox("Open: drop ingredients on it. Punch to close (cook). Punch to open (results appear inside). Punch again to collect.");

        ::combiner *comb = nullptr;
        for (::combiner &c : world.combiners)
            if (c.pos.x_int() == state.punch.x_int() && c.pos.y_int() == state.punch.y_int())
                comb = &c;
        if (comb != nullptr && !comb->contents.empty())
        {
            std::string held = "`wInside:`` ";
            for (const ::slot &s : comb->contents)
                held += std::format("`w{}`` x{}  ", id_to_item(s.id).raw_name, s.count);
            dialog.add_textbox(held);
        }
    }
    else // SEWING_MACHINE
    {
        dialog
            .add_textbox("Feed two clothing items to sew a new outfit piece.")
            .add_spacer("small")
            .add_text_input("cloth1", "Clothing item ID:", "", 6)
            .add_text_input("cloth2", "Clothing item ID:", "", 6)
            .add_spacer("small")
            .add_button("sew", "`2Sew``");
    }

    dialog.add_spacer("small").add_quick_exit();
    send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("combiner_edit", "Close", "") });
}

void combiner_edit(ENetEvent &event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
    if (world == worlds.end()) return;

    const ::pos pos{ atoi(hPipe["tilex"].c_str()), atoi(hPipe["tiley"].c_str()) };
    ::block &block = world->blocks[cord(pos.x_int(), pos.y_int())];
    const ::item &machine = id_to_item(block.fg);

    if (hPipe["buttonClicked"] == "sew" && machine.type == type::SEWING_MACHINE)
    {
        const short a = static_cast<short>(atoi(hPipe["cloth1"].c_str()));
        const short b = static_cast<short>(atoi(hPipe["cloth2"].c_str()));
        const ::item &ia = id_to_item(a);
        const ::item &ib = id_to_item(b);
        if (ia.type != type::CLOTHING || ib.type != type::CLOTHING)
        {
            on::ConsoleMessage(event.peer, "`4Both inputs must be clothing.``");
            return;
        }
        if (!inventory_has(pPeer, a, 1) || !inventory_has(pPeer, b, 1))
        {
            on::ConsoleMessage(event.peer, "`4You're missing those clothes.``");
            return;
        }

        modify_item_inventory(event, ::slot(a, -1));
        modify_item_inventory(event, ::slot(b, -1));

        // pick a random clothing item of similar-or-lower rarity from items.dat
        std::vector<u_short> pool;
        const u_short rarity_cap = std::max(ia.rarity, ib.rarity);
        for (const ::item &it : items)
        {
            if (it.type == type::CLOTHING && it.id != 0 && it.rarity <= rarity_cap && it.rarity < 999)
                pool.push_back(it.id);
        }
        if (pool.empty())
        {
            on::ConsoleMessage(event.peer, "`4The sewing machine jammed.``");
            return;
        }

        ransuu rng;
        const u_short result = pool[static_cast<std::size_t>(rng[{0, static_cast<int>(pool.size() - 1)}])];
        modify_item_inventory(event, ::slot(static_cast<short>(result), 1));
        on::ConsoleMessage(event.peer, std::format("`2Sewed a `w{}``!``", id_to_item(result).raw_name));
    }
}
