#include "pch.hpp"
#include "on/NameChanged.hpp"
#include "on/SetClothing.hpp"
#include "on/Action.hpp"
#include "on/ConsoleMessage.hpp"
#include "commands/weather.hpp"
#include "item_activate.hpp"
#include "fishing.hpp"
#include "tools/ransuu.hpp"
#include "tools/create_dialog.hpp"
#include "action/quit_to_exit.hpp"
#include "action/join_request.hpp"
#include "action/dialog_return/letter_box.hpp"
#include "action/dialog_return/display_edit.hpp"
#include "action/dialog_return/vending.hpp"
#include "action/dialog_return/magplant.hpp"
#include "action/dialog_return/combiner.hpp"
#include "database/server_config.hpp"
#include "database/achievements.hpp"
#include "database/quests.hpp"
#include "item_activate_object.hpp"

#include "tile_change.hpp"

using namespace std::chrono;
using namespace std::literals::chrono_literals; // @note for 'ms' 's' (millisec, seconds)

static void add_gem_drops(ENetEvent &event, int base_gems, const ::pos &pos, ::world &world)
{
    int gems = std::max(1, static_cast<int>(
        std::lround(static_cast<double>(base_gems) * gServer_config.gem_drop_multiplier)
    ));

    for (short denomination : {100, 50, 10, 5, 1})
        for (; gems >= denomination; gems -= denomination)
            add_drop(event, {112, denomination}, pos, world);
}

void tile_change(ENetEvent& event, state state) 
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    try
    {
        auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
        if (world == worlds.end()) return;

        ::block &block = world->blocks[cord(state.punch.x, state.punch.y)];

        // @note before the empty-tile bail-out: punching bare water (fg/bg 0) must still reel / cast
        if (try_fishing(event, state, block, *world)) return;

        const ::item &item = id_to_item((state.id != 32 && state.id != 18) ? state.id : (block.fg != 0) ? block.fg : block.bg);
        if (item.id == 0) return;

        if (block.state[3] & S_FIRE) // @note allow anyone to take out fire
            if (pPeer->clothing[hand] == 3066/* fire hose */)
            {
                remove_fire(event, state, block, *world);
                return; // @note avoid hitting the block
            }

        // @note guests may wrench public-interact tiles (buy from vending, etc.) without build rights
        const bool public_wrench =
            state.id == 32 && item.type == type::VENDING_MACHINE;

        if (!(item.cat & CAT_PUBLIC) && !public_wrench) // @note if block is public skip validating if peer is owner or access
            if (!peer_can_edit_tile(pPeer, *world, state.punch)) return;

        bool lock_visuals{}; // @todo this looks sloppy
        bool push_growth{};  // @note true after placing a seed/provider (refresh growth countdown)
        
        if (state.id == 18) // @note punching a block
        {
            static bool punch{}; // @note true if tile_change has been called within this inital (punch)

            if (!punch) // @note put all multiple punch features here
            {
                punch = true;
                if (pPeer->clothing[hand] == 5480) // @note Rayman's Fist
                {
                    ::state copy_state = state;

                    /* @note up and down */
                    if (state.punch.y == state.pos.by_32(true).y)
                    {
                        copy_state.punch.x += (pPeer->facing_left) ? -1 : 1;
                        tile_change(event, std::move(copy_state));
                        
                        copy_state.punch.x += (pPeer->facing_left) ? -1 : 1;
                        tile_change(event, std::move(copy_state));
                    }
                    /* @note left and right <- -> */
                    else if (state.punch.x == state.pos.by_32(true).x)
                    {
                        copy_state.punch.y += (state.punch.y < state.pos.by_32(true).y) ? -1 : 1;
                        tile_change(event, std::move(copy_state));

                        copy_state.punch.y += (state.punch.y < state.pos.by_32(true).y) ? -1 : 1;
                        tile_change(event, std::move(copy_state));
                    }
                    /* @note horizontal adjacent \/ */
                    else if (state.punch.y != state.pos.by_32(true).y)
                    {
                        copy_state.punch.x += (pPeer->facing_left) ? -1 : 1;
                        copy_state.punch.y += (state.punch.y < state.pos.by_32(true).y) ? -1 : 1;
                        tile_change(event, std::move(copy_state));

                        copy_state.punch.x += (pPeer->facing_left) ? -1 : 1;
                        copy_state.punch.y += (state.punch.y < state.pos.by_32(true).y) ? -1 : 1;
                        tile_change(event, std::move(copy_state));
                    }
                }
                punch = false;
            }
            ransuu ransuu;
            u_char apply_damage_value{}; // @note used to change a tile value without using send_tile_update() 

            if (pPeer->clothing[hand] == 2952/*Digger's Spade*/)
            {
                if(item.id == 2/*Dirt*/ || item.id == 14)
                {
                    if (block.fg != 0) block.hits[0] = 3;
                    else block.hits[1] = 3;
    
                    int color = (item.id ==  2/*Dirt*/) ? ransuu[{0x02, 0x03}]/* @note idk if this is the correct one, at least by looking at the color it looks like dirt*/ : 
                                  (item.id == 14/*Cave Background*/) ? ransuu[{0x0e, 0x0f}] : 0x02;

                    send_particle_effect(event, state.punch.by_32(), {color, 0x61});
                }
            }
            switch (item.id)
            {
                case 758: // @note Roulette Wheel
                {
                    const u_char number = ransuu[{0, 36}];
                    const char color = (number == 0) ? '2' : (ransuu[{0, 3}] < 2) ? 'b' : '4';
                    const std::string message = std::format("[`{}{}`` spun the wheel and got `{}{}``!]", pPeer->prefix, pPeer->growid, color, number);
                    peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&event, &pPeer, message](ENetPeer& peer)
                    {
                        send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, message }, -1, 2000);
                        on::ConsoleMessage(event.peer, message, 2000);
                    });
                    break;
                }
            }
            switch (item.type)
            {
                case type::STRONG: throw std::runtime_error("It's too strong to break.");
                case type::MAIN_DOOR: throw std::runtime_error("(stand over and punch to use)");
                case type::LOCK:
                {
                    if (is_tile_lock(item.id))
                    {
                        // only owner (or admin) may break a tile lock
                        auto tl = std::ranges::find(world->tile_locks, state.punch, &::tile_lock::pos);
                        if (tl != world->tile_locks.end() &&
                            !pPeer->role && pPeer->user_id != tl->owner &&
                            !(world->owner && pPeer->user_id == world->owner))
                            throw std::runtime_error("`4That lock isn't yours.``");
                        break;
                    }

                    if (world->owner != pPeer->user_id)
                        throw std::runtime_error(std::format("`5[```w{}`` `$World Locked`` by (null)`5]``", world->name)); // @todo add owner name
                    break;
                }
                case type::PROVIDER:
                {
                    if (block_elapsed_seconds(block.tick) >= item.tick)
                    {
                        switch (item.id)
                        {
                            case 1008: // @note ATM
                            {
                                int gems = ransuu[{1, 100}]; // @note source: https://growtopia.fandom.com/wiki/ATM_Machine
                                add_gem_drops(event, gems, state.punch.by_32(), *world);
                                        
                                break;
                            }
                            case 872:/*chicken*/ case 866:/*cow*/ case 1632:/*coffee maker*/ case 3888:/*sheep*/
                            {
                                add_drop(event, ::slot(item.id+2, ransuu[{1, 2}]), state.punch.by_32(), *world);
                                break;
                            }
                            case 5116:/*tea set*/
                            {
                                add_drop(event, ::slot(item.id-2, ransuu[{1, 2}]), state.punch.by_32(), *world);
                                break;
                            }
                            case 2798:/*well*/
                            {
                                add_drop(event, ::slot(822/*water bucket*/, ransuu[{1, 2}]), state.punch.by_32(), *world);
                                break;
                            }
                            case 928:/*science station*/ // @note source: https://growtopia.fandom.com/wiki/Science_Station
                            {
                                short chemcial = 
                                    (!ransuu[{0, 16}]) ? chemcial = 918/*P*/ : 
                                    (!ransuu[{0, 8}])  ? chemcial = 920/*B*/ : 
                                    (!ransuu[{0, 6}])  ? chemcial = 924/*Y*/ : 
                                    (!ransuu[{0, 4}])  ? chemcial = 916/*R*/ : chemcial = 914/*G*/;
                                add_drop(event, {chemcial, 1}, state.punch.by_32(), *world);
                                break;
                            }
                        }
                        block.tick = growth_planted_tick(item.tick);
                        send_tile_update(event, std::move(state), block, *world); // @note update countdown on provider.

                        pPeer->add_xp(event, 1);
                        return;
                    }
                    break;
                }
                case type::SEED:
                {
                    if (block_elapsed_seconds(block.tick) >= item.tick) // @todo limit this check.
                    {
                        block.hits[0] = 99;
                        add_drop(event, ::slot(item.id - 1, ransuu[{2, 12}]), state.punch.by_32(), *world); // @note fruit (from tree)
                    }
                    else
                    {
                        // @note not ready yet — refresh the growth_speed countdown, don't break early
                        send_tile_update(event, state, block, *world);
                        return;
                    }
                    break;
                }
                case type::WEATHER_MACHINE:
                {
                    ::block &weather_machine = world->blocks[cord(world->weather.x, world->weather.y)];

                    if (!(block.state[2] & S_TOGGLE) && !(weather_machine.state[2] & S_TOGGLE)) weather_machine.state[2] &= ~S_TOGGLE; // @note so we can avoid the upcoming ^= if the weather machine is already toggled
                    block.state[2] ^= S_TOGGLE; // @note if punched twice it can detoggle that is why we use ^= not |=
                    
                    world->weather = state.punch;
                    
                    peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [block, item](ENetPeer& p)
                    {
                        send_varlist(&p, { "OnSetCurrentWeather", (block.state[2] & S_TOGGLE) ? get_weather_id(item.id) : 0 });
                    });
                    break;
                }
                case type::TOGGLEABLE_BLOCK:
                case type::TOGGLEABLE_ANIMATED_BLOCK:
                case type::CHEST:
                {
                    block.state[2] ^= S_TOGGLE;
                    if (item.id == 226) // @note Signal Jammer
                    {
                        on::ConsoleMessage(event.peer, (block.state[2] & S_TOGGLE) ? 
                            "Signal jammer enabled. This world is now `4hidden`` from the universe." :
                            "Signal jammer disabled.  This world is `2visible`` to the universe.");
                    }
                    break;
                }
                case type::CHEMICAL_COMBINER:
                {
                    combiner_toggle(event, state, block, *world);
                    return; // @note open/close only — don't chip the machine each toggle
                }
                case type::RANDOM:
                {
                    apply_damage_value = 
                        (item.id == 456/*Dice*/) ? ransuu[{0, 5}] : 
                        (item.id == 1300/*Roshambo*/) ? ransuu[{1, 3}] : 0;

                    auto random = std::ranges::find(world->random_blocks, state.punch, &::random_block::pos);
                    if (random == world->random_blocks.end())
                    {
                        world->random_blocks.emplace_back(::random_block{apply_damage_value, state.punch});
                    }
                    else random->value = apply_damage_value;
                    break;
                }
            }
            tile_apply_damage(event, std::move(state), block, apply_damage_value);

            if (block.hits[0] >= item.hits) block.fg = 0, block.hits[0] = 0;
            else if (block.hits[1] >= item.hits) block.bg = 0, block.hits[1] = 0;
            else return;
            
            /* @todo update these changes with tile_update() */
            block.label = "";
            block.state[2] = 0x00; // @note reset tile direction
            block.state[3] &= ~S_VANISH; // @note remove paint

            if (item.type == type::MAILBOX || item.type == type::BULLETIN || item.type == type::DONATION_BOX)
                std::erase_if(world->letters, [&state](const ::letter &letter) { return letter.pos == state.punch; }); // @note broken box drops its contents into the void

            if (item.type == type::DISPLAY_BLOCK)
            {
                auto display = std::ranges::find(world->displays, state.punch, &::display::pos);
                if (display != world->displays.end())
                {
                    if (display->id != 0)
                        add_drop(event, ::slot(static_cast<short>(display->id), 1), state.punch.by_32(), *world);
                    world->displays.erase(display);
                }
            }

            if (item.type == type::VENDING_MACHINE)
            {
                auto vend = std::ranges::find(world->vendings, state.punch, &::vending::pos);
                if (vend != world->vendings.end())
                {
                    while (vend->count > 0)
                    {
                        short give = std::min(vend->count, static_cast<u_short>(200));
                        add_drop(event, ::slot(static_cast<short>(vend->id), give), state.punch.by_32(), *world);
                        vend->count = static_cast<u_short>(vend->count - give);
                    }
                    if (vend->earned > 0)
                        add_drop(event, ::slot(242, vend->earned), state.punch.by_32(), *world);
                    world->vendings.erase(vend);
                }
            }

            if (item.type == type::MAGPLANT)
            {
                auto mag = std::ranges::find(world->magplants, state.punch, &::magplant::pos);
                if (mag != world->magplants.end())
                {
                    while (mag->count > 0 && mag->id != 0)
                    {
                        short give = std::min(mag->count, static_cast<u_short>(200));
                        add_drop(event, ::slot(static_cast<short>(mag->id), give), state.punch.by_32(), *world);
                        mag->count = static_cast<u_short>(mag->count - give);
                    }
                    world->magplants.erase(mag);
                }
            }

            if (item.type == type::CHEMICAL_COMBINER)
            {
                auto comb = std::ranges::find(world->combiners, state.punch, &::combiner::pos);
                if (comb != world->combiners.end())
                {
                    for (::slot s : comb->contents)
                        while (s.count > 0)
                        {
                            short give = std::min<short>(s.count, 200);
                            add_drop(event, ::slot(s.id, give), state.punch.by_32(), *world);
                            s.count -= give;
                        }
                    world->combiners.erase(comb);
                }
            }

            if (item.type == type::XENONITE && !world_has_xenonite(*world))
            {
                peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [](ENetPeer& p) 
                {
                    ::peer *pOthers = static_cast<::peer*>(p.data);
                    pOthers->state &= ~S_DOUBLE_JUMP;
                    on::SetClothing(p);
                });
            }
            
            if (item.id == 392/*Heartstone*/ || item.id == 3402/*GBC*/ || item.id == 9350/*Super GBC*/)
            {
                short reward =
                    (!ransuu[{0, 99}]) ? 1458 : // @note GHC
                    (!ransuu[{0, 20}]) ? 362 : // @note Angel Wings
                    (!ransuu[{0, 8}])  ? 366 : // @note Heartbow
                    (!ransuu[{0, 8}])  ? 1470 : // @note Ruby Necklace
                    (!ransuu[{0, 20}]) ? 2384 : // @note Love Bug
                    (!ransuu[{0, 4}])  ? 2396 : // @note Valensign
                    (!ransuu[{0, 10}]) ? 3388 : // @note Heartbreaker Hammer
                    (!ransuu[{0, 10}]) ? 2390 : // @note Teeny Angel Wings
                    (!ransuu[{0, 10}]) ? 3396 : // @note Lovebird Pendant
                    (!ransuu[{0, 2}])  ? 3404 : // @note Sour Lollipop
                    (!ransuu[{0, 4}])  ? 3406 : // @note Sweet Lollipop
                    (!ransuu[{0, 2}])  ? 3408 : // @note Pink Marble Arch
                    388; // @note Perfume
                    // @todo add all the remaining drops - https://growtopia.fandom.com/wiki/Golden_Booty_Chest

                add_drop(event, ::slot(reward, (reward == 3408 || reward == 3404) ? 10 : 1), state.punch.by_32(), *world);
                if (reward == 1458)
                {
                    std::string message = std::format("msg|`4The Power of Love! `2{} found a `#Golden Heart Crystal`2 in a `#{}`2!", pPeer->growid, item.raw_name);
                    peers(pPeer->recent_worlds.back(), PEER_ALL, [message](ENetPeer &p)
                    {
                        send_action(p, "log", message.c_str());
                    });
                }
                if (++pPeer->gbc_pity % 100 == 0) modify_item_inventory(event, ::slot{9350, 1});
            }
            else if (item.type == type::LOCK && !is_tile_lock(item.id))
            {
                if (!pPeer->role)
                {
                    pPeer->prefix.front() = 'w';
                    on::NameChanged(event);
                }
                
                world->owner = 0;
            }
            else if (item.type == type::LOCK && is_tile_lock(item.id))
            {
                remove_tile_lock(*world, state.punch);
            }

            if (item.cat == CAT_RETURN)
            {
                int uid = add_object(event, ::slot(item.id, 1), state.pos, *world);
                item_activate_object(event, ::state{.id = uid, .punch = state.punch});
            }
            else if (u_char(item.property) & 04) { } // @note "This item never drops any seeds."; should it drop a block?
            else // @note normal break (drop gem, seed, block & give XP)
            {
                if (item.type != type::SEED)
                { /* gem drop */
                    /* if greater than 1, assume it's a farmable.*/
                    u_char rarity_to_gem =
                        (item.rarity >= 87) ? 22 : 
                        (item.rarity >= 68) ? 18 : 
                        (item.rarity >= 53) ? 14 : 
                        (item.rarity >= 41) ? 11 : 
                        (item.rarity >= 36) ? 10 :
                        (item.rarity >= 32) ? 9 :
                        (item.rarity >= 24) ? 5 : 1;

                    if (!ransuu[{0, (rarity_to_gem > 1) ? 1 : 4}]) // @note double chances if farmable.
                    {
                        int gems = ransuu[{1, rarity_to_gem}];
                        add_gem_drops(event, gems, state.punch.by_32(), *world);
                    }
                    if (!ransuu[{0, (rarity_to_gem > 1) ? 2 : 4}]) add_drop(event, ::slot(item.id + 1, 1), state.punch.by_32(), *world); 
                    else if (!ransuu[{0, (rarity_to_gem > 1) ? 4 : 8}]) add_drop(event, ::slot(item.id, 1), state.punch.by_32(), *world);
                }
                else
                {
                    // @note breaking a tree: chance to drop the seed (item.id is the seed)
                    if (!ransuu[{0, 3}])
                        add_drop(event, ::slot(item.id, 1), state.punch.by_32(), *world);

                    achievement_progress(event, ACH_TREES_HARVESTED);
                    quest_progress(event, QUEST_HARVEST_TREES);
                }

                pPeer->add_xp(event, std::trunc(1.0f + item.rarity / 5.0f));
                achievement_progress(event, ACH_BLOCKS_BROKEN);
                quest_progress(event, QUEST_BREAK_BLOCKS);
            }
        } // @note delete im, id
        else if (item.cloth_type != clothing::none) 
        {
            if (state.punch != pPeer->pos.by_32(true)) throw std::runtime_error("To wear clothing, use on yourself");

            item_activate(event, state);
            return; 
        }
        else if (item.type == type::CONSUMEABLE) 
        {
            if (item.raw_name.find(" Blast") != std::string::npos)
            {
                send_varlist(event.peer, {
                    "OnDialogRequest",
                    std::format(
                        "set_default_color|`o\n"
                        "embed_data|id|{0}\n"
                        "add_label_with_icon|big|`w{1}``|left|{0}|\n"
                        "add_label|small|This item creates a new world! Enter a unique name for it.|left\n"
                        "add_text_input|name|New World Name||24|\n"
                        "end_dialog|create_blast|Cancel|Create!|\n", // @todo rgt "Create!" is a purple-ish pink color
                        item.id, item.raw_name
                    )
                });
            }

            if (item.raw_name.find("Paint Bucket - ") != std::string::npos && pPeer->clothing[hand] != 3494) throw std::runtime_error("you need a Paintbrush to apply paint!");
            if (item.raw_name.find("Hair Dye") != std::string::npos)
            {
                if (state.punch != pPeer->pos.by_32(true)) throw std::runtime_error("Don't spill your dye!");
                else if (world->blocks[cord(pPeer->pos.by_32(true).x, pPeer->pos.by_32(true).y)].fg != 230/*Bathtub*/) throw std::runtime_error("You'll make a huge mess if you do that outside the Bathtub!");

                on::Action(event, "shower");
                // audio/shower.wav
                send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "You dyed your hair!", 0u, 1u });
            }
            float color{}; // @note the color of the particle effect.
            float particle{};
            switch (item.id)
            {
                case 1404: // @note Door Mover
                {
                    if (!door_mover(*world, state.punch)) throw std::runtime_error("There's no room to put the door there! You need 2 empty spaces vertically.");

                    std::string remember_name = world->name;
                    peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer& p) 
                    { 
                        ENetEvent fake{.peer = &p};
                        action::quit_to_exit(fake, "", true); // @note everyone in world exits
                        action::join_request(fake, "", remember_name); // @note everyone in world re-joins
                    });
                    return;
                }
                case 822: // @note Water Bucket
                {
                    if (block.state[3] & S_FIRE) remove_fire(event, state, block, *world);
                    else block.state[3] ^= S_WATER;
                    break;
                }
                case 1866: // @note Block Glue
                {
                    block.state[3] ^= S_GLUE;
                    break;
                }
                case 3062: // @note Pocket Lighter
                {
                    if (block.fg == 0 && block.bg == 0) throw std::runtime_error("There's nothing to burn!");
                    if (block.state[3] & (S_FIRE | S_WATER)) return; // @note avoid fire on water & fire on fire

                    block.state[3] |= S_FIRE;

                    std::string message = "`7[```4MWAHAHAHA!! FIRE FIRE FIRE```7]``";
                    peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer& p) 
                    {
                        send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, message, 0u });
                        on::ConsoleMessage(event.peer, message);
                    });
                    particle = 0x96;

                    if (block.fg == 3090) // @note Highly Combustible Box
                    {
                        block.fg = 3128; // @note Combusted Box
                        if (!(block.state[2] & S_TOGGLE)/*closed*/) {} // @todo recipes: https://growtopia.fandom.com/wiki/Guide:Highly_Combustible_Box
                    }
                    break;
                }
                case 3404:/*Sour Lollipop*/ case 3406:/*Sweet Lollipop*/
                {
                    send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "`#YUM!:D", 0u });

                    break;
                }
                case 3400: // @note Love Potion #8
                {
                    if (block.fg != 10) return; // @note Rock

                    block.fg = 392; // @note Heartstone
                    particle = 0x2c;
                    break;
                }
                case 1488: // @note Experience Potion
                {
                    send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "`#GULP! You got smarter!", 0u });
                    pPeer->add_xp(event, 10000);
                    break;
                }
                case 834: // @note Fireworks
                {
                    fireworks(event, state.punch.by_32());
                    break;
                }
                case 2480: // @note Megaphone
                {
                    send_varlist(event.peer, {
                        "OnDialogRequest",
                        ::create_dialog()
                            .set_default_color("`o")
                            .add_label_with_icon("big", "`wMegaphone``", item.id)
                            .add_textbox("Enter a message you want to broadcast to every player in Growtopia! This will use up 1 Megaphone")
                            .add_text_input("message", "", "", 128)
                            .end_dialog("megaphone", "Nevermind", "Broadcast")
                    });
                    break;
                }
                case 408: // @note Duct Tape
                {
                    peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer& p) 
                    {
                        ::peer *_p = static_cast<::peer*>(p.data);

                        if (state.punch == _p->pos.by_32(true))
                        {
                            _p->state |= S_DUCT_TAPE; // @todo add a 10 minute timer that will remove it.
                            on::SetClothing(p);
                        }
                    });
                    break;
                }
                case 3478: // @note Paint Bucket - Red
                {
                    block.state[3] |= S_RED;
                    color = bgra::RED, particle = 0xa8; 
                    break;
                }
                case 3480: // @note Paint Bucket - Yellow
                {
                    block.state[3] |= S_YELLOW;
                    color = bgra::GREEN | bgra::RED, particle = 0xa8;
                    break;
                }
                case 3482: // @note Paint Bucket - Green
                {
                    block.state[3] |= S_GREEN;
                    color = bgra::GREEN, particle = 0xa8;
                    break;
                }
                case 3484: // @note Paint Bucket - Aqua
                {
                    block.state[3] |= S_AQUA;
                    color = bgra::BLUE | bgra::GREEN, particle = 0xa8;
                    break;
                }
                case 3486: // @note Paint Bucket - Blue
                {
                    block.state[3] |= S_BLUE;
                    color = bgra::BLUE, particle = 0xa8;
                    break;
                }
                case 3488: // @note Paint Bucket - Purple
                {
                    block.state[3] |= S_PURPLE;
                    color = bgra::BLUE | bgra::RED, particle = 0xa8; // @note blue + red
                    break;
                }
                case 3490: // @note Paint Bucket - Charcoal
                {
                    block.state[3] |= S_CHARCOAL;
                    color = bgra::BLUE | bgra::GREEN | bgra::RED | bgra::ALPHA, particle = 0xa8; // @note max will provide a pure black color. idk if growtopia is the same.
                    break;
                }
                case 3492: // @note Paint Bucket - Vanish
                {
                    block.state[3] &= ~S_VANISH;
                    color = bgra::BLUE | bgra::GREEN | bgra::RED, particle = 0xa8; // @todo get exact color. I just guessed T-T
                }
                case 3822: break; // Red Hair Dye
                case 3824: break; // Green Hair Dye
                case 3826: break; // Blue Hair Dye
                default: return; // @note prevent taking the consumeable if nothing happended
            }
            if (particle > 0.0f)
            {
                send_particle_effect(event, state.punch.by_32(), {color, particle});
            }
            send_tile_update(event, std::move(state), block, *world);

            modify_item_inventory(event, ::slot(item.id, -1));
            pPeer->add_xp(event, 1);
            return;
        }
        else if (state.id == 32)
        {
            switch (item.type)
            {
                case type::LOCK:
                {
                    if (is_tile_lock(item.id))
                    {
                        auto tl = std::ranges::find(world->tile_locks, state.punch, &::tile_lock::pos);
                        if (tl == world->tile_locks.end()) break;
                        if (!pPeer->role && pPeer->user_id != tl->owner) break;

                        send_varlist(event.peer, {
                            "OnDialogRequest",
                            std::format(
                                "set_default_color|`o\n"
                                "add_label_with_icon|big|`wEdit {}``|left|{}|\n"
                                "embed_data|tilex|{}\n"
                                "embed_data|tiley|{}\n"
                                "add_spacer|small|\n"
                                "add_textbox|This lock protects `w{}`` tiles around it.|left|\n"
                                "add_spacer|small|\n"
                                "add_checkbox|checkbox_public|Allow anyone to Build and Break|{}\n"
                                "add_spacer|small|\n"
                                "end_dialog|tile_lock_edit|Cancel|OK|\n",
                                item.raw_name, item.id, state.punch.x, state.punch.y,
                                tl->area.size(), to_char(tl->is_public)
                            )
                        });
                        break;
                    }

                    if (pPeer->user_id == world->owner)
                    {
                        send_varlist(event.peer, {
                            "OnDialogRequest",
                            std::format(
                                "set_default_color|`o\n"
                                "add_label_with_icon|big|`wEdit {}``|left|{}|\n"
                                "add_popup_name|LockEdit|\n"
                                "add_label|small|`wAccess list:``|left\n"
                                "embed_data|tilex|{}\n"
                                "embed_data|tiley|{}\n"
                                "add_spacer|small|\n"
                                "add_label|small|Currently, you're the only one with access.``|left\n"
                                "add_spacer|small|\n"
                                "add_player_picker|playerNetID|`wAdd``|\n"
                                "add_checkbox|checkbox_public|Allow anyone to Build and Break|{}\n"
                                "add_checkbox|checkbox_disable_music|Disable Custom Music Blocks|{}\n"
                                "add_text_input|tempo|Music BPM|100|3|\n"
                                "add_checkbox|checkbox_disable_music_render|Make Custom Music Blocks invisible|0\n"
                                "add_checkbox|checkbox_set_as_home_world|Set as Home World|0|\n"
                                "add_text_input|minimum_entry_level|World Level: |{}|3|\n"
                                "add_smalltext|Set minimum world entry level.|\n"
                                "add_button|sessionlength_dialog|`wSet World Timer``|noflags|0|0|\n"
                                "add_button|changecat|`wCategory: None``|noflags|0|0|\n"
                                "add_button|getKey|Get World Key|noflags|0|0|\n"
                                "end_dialog|lock_edit|Cancel|OK|\n",
                                item.raw_name, item.id, state.punch.x, state.punch.y, to_char(world->is_public), (world->lock_state & DISABLE_MUSIC) ? "1" : "0", world->minimum_entry_level
                            )
                        });
                    }
                    break;
                }
                case type::DOOR:
                case type::PORTAL:
                {
                    std::string dest, id{};
                    for (::door& door : world->doors)
                        if (door.pos == state.punch) dest = door.dest, id = door.id;
                        
                    send_varlist(event.peer, {
                        "OnDialogRequest",
                        std::format(
                            "set_default_color|`o\n"
                            "add_label_with_icon|big|`wEdit {}``|left|{}|\n"
                            "add_text_input|door_name|Label|{}|100|\n"
                            "add_popup_name|DoorEdit|\n"
                            "add_text_input|door_target|Destination|{}|24|\n"
                            "add_smalltext|Enter a Destination in this format: `2WORLDNAME:ID``|left|\n"
                            "add_smalltext|Leave `2WORLDNAME`` blank (:ID) to go to the door with `2ID`` in the `2Current World``.|left|\n"
                            "add_text_input|door_id|ID|{}|11|\n"
                            "add_smalltext|Set a unique `2ID`` to target this door as a Destination from another!|left|\n"
                            "add_checkbox|checkbox_locked|Is open to public|1\n"
                            "embed_data|tilex|{}\n"
                            "embed_data|tiley|{}\n"
                            "end_dialog|door_edit|Cancel|OK|", 
                            item.raw_name, item.id, block.label, dest, id, state.punch.x, state.punch.y
                        )
                    });
                    break;
                }
                case type::SIGN:
                {
                    send_varlist(event.peer, {
                        "OnDialogRequest",
                        std::format(
                            "set_default_color|`o\n"
                            "add_popup_name|SignEdit|\n"
                            "add_label_with_icon|big|`wEdit {}``|left|{}|\n"
                            "add_textbox|What would you like to write on this sign?``|left|\n"
                            "add_text_input|sign_text||{}|128|\n"
                            "embed_data|tilex|{}\n"
                            "embed_data|tiley|{}\n"
                            "end_dialog|sign_edit|Cancel|OK|", 
                            item.raw_name, item.id, block.label, state.punch.x, state.punch.y
                        )
                    });
                    break;
                }
                case type::ENTRANCE:
                {
                    send_varlist(event.peer, {
                        "OnDialogRequest",
                        std::format(
                            "set_default_color|`o\n"
                            "add_label_with_icon|big|`wEdit {}``|left|{}|\n"
                            "add_checkbox|checkbox_public|Is open to public|{}\n"
                            "embed_data|tilex|{}\n"
                            "embed_data|tiley|{}\n"
                            "end_dialog|gateway_edit|Cancel|OK|\n", 
                            item.raw_name, item.id, to_char((block.state[2] & S_PUBLIC)), state.punch.x, state.punch.y
                        )
                    });
                    break;
                }
                case type::DISPLAY_BLOCK:
                {
                    display_edit_dialog(event, state, *world, item);
                    break;
                }
                case type::MAILBOX:
                case type::BULLETIN:
                case type::DONATION_BOX:
                {
                    letter_box_dialog(event, state, *world, item);
                    break;
                }
                case type::VENDING_MACHINE:
                {
                    vending_dialog(event, state, *world, item);
                    break;
                }
                case type::MAGPLANT:
                {
                    magplant_dialog(event, state, *world, item);
                    break;
                }
                case type::CHEMICAL_COMBINER:
                case type::SEWING_MACHINE:
                {
                    combiner_dialog(event, state, *world, item);
                    break;
                }
            }
            return; // @note leave early else wrench will act as a block unlike fist which breaks. this is cause of state_visuals()
        }
        else // @note placing a block
        {
            if (block.fg != 0) // @note placing something ontop of exisitng block
            {
                bool update_tile{};
                switch (items[world->blocks[cord(state.punch.x, state.punch.y)].fg].type)
                {
                    case type::DISPLAY_BLOCK:
                    {
                        auto display = std::ranges::find(world->displays, state.punch, &::display::pos);
                        if (display != world->displays.end() && display->id != 0)
                            throw std::runtime_error("There's already something in this Display Block.");
                        if (item.cat & CAT_UNTRADEABLE)
                            throw std::runtime_error("That item can't go in a Display Block.");
                        if (display == world->displays.end())
                            world->displays.emplace_back(::display(item.id, state.punch));
                        else
                            display->id = item.id;
                        modify_item_inventory(event, ::slot(item.id, -1));
                        world->mark_dirty();
                        update_tile = true;
                        break;
                    }
                    case type::SEED:
                    {
                        for (::item &item : items)
                        {
                            if ((item.splice[0] == state.id && item.splice[1] == block.fg) ||
                                (item.splice[1] == state.id && item.splice[0] == block.fg) /* allow reverse splice combo */)
                            {
                                auto splice0 = std::ranges::find(items, item.splice[0], &::item::id);
                                auto splice1 = std::ranges::find(items, item.splice[1], &::item::id);

                                send_varlist(event.peer, {
                                    "OnTalkBubble", 
                                    pPeer->netid, 
                                    std::format("`w{}`` and `w{}`` have been spliced to make a `${} Tree``!", 
                                        splice0->raw_name, splice1->raw_name, item.raw_name.substr(0, item.raw_name.length()-5/* seed*/)), // @todo this is hardcoded
                                    0u,
                                    1u
                                });
                                block.tick = growth_planted_tick(item.tick);
                                block.fg = item.id;
                                update_tile = true;
                                break;
                            }
                        }
                        break;
                    }
                }
                if (update_tile)
                    send_tile_update(event, std::move(state), block, *world);
                return;
            }
            if (item.collision == collision::FULL)
            {
                if (state.punch == state.pos.by_32(true)) return; // @todo when moving avoid collision.
            }
            switch (item.type)
            {
                case type::LOCK:
                {
                    if (is_tile_lock(item.id))
                    {
                        if (tile_lock_at(*world, state.punch) != nullptr)
                            throw std::runtime_error("There's already a lock covering this area.");

                        ::tile_lock tl(state.punch, static_cast<u_short>(item.id), pPeer->user_id, false);
                        tl.area = claim_tile_lock_area(*world, state.punch, tile_lock_capacity(static_cast<u_short>(item.id)));
                        apply_tile_lock(*world, tl);
                        world->tile_locks.push_back(std::move(tl));
                        lock_visuals = true;

                        const std::string placed_message = std::format(
                            "`5[```w{}`` placed a `$Area Lock`` protecting `w{}`` tiles`5]``",
                            pPeer->growid, world->tile_locks.back().area.size());
                        peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&pPeer, placed_message](ENetPeer& peer)
                        {
                            send_varlist(&peer, { "OnTalkBubble", pPeer->netid, placed_message });
                            on::ConsoleMessage(&peer, placed_message);
                        });
                        break;
                    }

                    if (!world->owner)
                    {
                        world->owner = pPeer->user_id;
                        lock_visuals = true;
                        if (!pPeer->role) 
                        {
                            pPeer->prefix.front() = '2';
                            on::NameChanged(event);
                        }
                        if (std::ranges::find(pPeer->my_worlds, world->name) == pPeer->my_worlds.end()) 
                        {
                            std::ranges::rotate(pPeer->my_worlds, pPeer->my_worlds.begin() + 1);
                            pPeer->my_worlds.back() = world->name;
                            pPeer->mark_dirty();
                        }
                        std::string placed_message = std::format("`5[```w{}`` has been `$World Locked`` by {}`5]``", world->name, pPeer->growid);
                        peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&event, &pPeer, placed_message](ENetPeer& peer) 
                        {
                            send_varlist(&peer, { "OnTalkBubble", pPeer->netid, placed_message });
                            on::ConsoleMessage(&peer, placed_message);
                        });
                    }
                    else throw std::runtime_error("Only one `$World Lock`` can be placed in a world, you'd have to remove the other one first.");
                    break;
                }
                case type::ENTRANCE:
                {
                    block.state[2] |= S_PUBLIC;
                    break;
                }
                case type::PROVIDER:
                {
                    block.tick = growth_planted_tick(item.tick);
                    break;
                }
                case type::SEED:
                {
                    block.state[2] |= 0x11;
                    block.tick = growth_planted_tick(item.tick);
                    break;
                }
                case type::XENONITE:
                {
                    peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [](ENetPeer& p) 
                    {
                        ::peer *pOthers = static_cast<::peer*>(p.data);
                        pOthers->state |= S_DOUBLE_JUMP;
                        on::SetClothing(p);
                        on::ConsoleMessage(&p, "`9The Xenonite Crystal empowers everyone in the world!``");
                    });
                    break;
                }
                case type::VENDING_MACHINE:
                {
                    // @note real GT: vendings only work in World Locked worlds
                    if (!world->owner)
                        throw std::runtime_error("Vending Machines can only be placed in a `$World Locked`` world.");
                    if (std::ranges::find(world->vendings, state.punch, &::vending::pos) == world->vendings.end())
                        world->vendings.emplace_back(state.punch);
                    break;
                }
                case type::CHEMICAL_COMBINER:
                {
                    // start OPEN (nonsolid) so ingredients can be dropped onto it
                    block.state[2] |= S_TOGGLE;
                    if (std::ranges::find_if(world->combiners, [&](const ::combiner &c) {
                            return c.pos.x_int() == state.punch.x_int() && c.pos.y_int() == state.punch.y_int();
                        }) == world->combiners.end())
                        world->combiners.emplace_back(state.punch);
                    break;
                }
                case type::MAGPLANT:
                {
                    if (std::ranges::find(world->magplants, state.punch, &::magplant::pos) == world->magplants.end())
                        world->magplants.emplace_back(state.punch);
                    break;
                }
            }
            block.state[2] |= (pPeer->facing_left) ? S_LEFT : S_RIGHT;
            (item.type == type::BACKGROUND) ? block.bg = state.id : block.fg = state.id;
            pPeer->emplace(::slot(item.id, -1));
            world->mark_dirty();
            achievement_progress(event, ACH_BLOCKS_PLACED);
            quest_progress(event, QUEST_PLACE_BLOCKS);

            // @note push elapsed after placing so the client countdown matches growth_speed
            push_growth = (item.type == type::SEED || item.type == type::PROVIDER);
        }
        // @note common tail: sends the tile-change visual for BOTH breaking and placing
        ::pos affected = state.punch;
        const int lock_item_id = state.id;
        state.netid = pPeer->netid; // @todo sometimes rgt has this as 0
        state_visuals(*event.peer, std::move(state)); // finished.
        if (push_growth)
            send_tile_update(event, ::state{ .punch = affected }, block, *world);
        if (lock_visuals) 
        {
            state_visuals(*event.peer, ::state{
                .type = 0x0f, // @note PACKET_SEND_LOCK
                .netid = pPeer->user_id, 
                .peer_state = peer_state::S_EXTENDED, 
                .id = lock_item_id,
                .punch = affected
            });
        }
    }
    catch (const std::exception& exc)
    {
        if (exc.what() && *exc.what()) 
        {
            send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, exc.what(), 0u, 1u });
        }
        return;
    }
}
