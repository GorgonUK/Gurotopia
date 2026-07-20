#include "pch.hpp"
#include "tools/create_dialog.hpp"
#include "on/ConsoleMessage.hpp"
#include "tools/ransuu.hpp"

#include "combiner.hpp"

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

    bool inventory_has(::peer *p, short id, short count)
    {
        auto it = std::ranges::find(p->slots, id, &::slot::id);
        return it != p->slots.end() && it->count >= count;
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
