#include "pch.hpp"
#include "tools/ransuu.hpp"
#include "on/ConsoleMessage.hpp"
#include "database.hpp"
#include "server_config.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <ctime>
#include <deque>
#include <unordered_set>

#include "world.hpp"

using namespace std::chrono;
using namespace std::literals::chrono_literals; // @note for 'ms' 's' (millisec, seconds)



int block_elapsed_seconds(std::time_t tick)
{
    if (tick <= 0)
        return 0;
    std::time_t now = std::time(nullptr);
    if (now < tick)
        return 0;
    long long elapsed = static_cast<long long>(now) - static_cast<long long>(tick);
    if (elapsed > 0x7fffffffll)
        return 0x7fffffff;
    return static_cast<int>(elapsed);
}

std::time_t growth_planted_tick(int grow_seconds)
{
    std::time_t now = std::time(nullptr);

    // @note growth is timed & rendered client-side from "seconds since planted".
    // pre-age the tick so the client's own 1x countdown (and the server harvest
    // check) both complete in grow_seconds / growth_speed real seconds.
    float speed = gServer_config.growth_speed;
    if (grow_seconds > 0 && speed > 1.0f)
    {
        double offset = static_cast<double>(grow_seconds) * (1.0 - 1.0 / static_cast<double>(speed));
        now -= static_cast<std::time_t>(offset);
    }
    return now;
}

bool world_has_xenonite(const ::world &world)
{
    return std::ranges::any_of(world.blocks, [](const ::block &block) 
    { 
        return block.fg != 0 && id_to_item(block.fg).type == type::XENONITE; 
    });
}


world::world(const std::string& name) 
{
    this->name = name;
}

std::deque<world> worlds;
std::atomic<u_int> g_object_uid{0};

void note_object_uid(u_int uid)
{
    u_int cur = g_object_uid.load(std::memory_order_relaxed);
    while (uid > cur && !g_object_uid.compare_exchange_weak(cur, uid, std::memory_order_relaxed))
    {
    }
}

::world* current_world(const ::peer& p)
{
    auto it = std::ranges::find(worlds, p.recent_worlds.back(), &::world::name);
    return (it != worlds.end()) ? &*it : nullptr;
}

::world* current_world(ENetEvent& event)
{
    return current_world(peer_of(event));
}

void send_action(ENetPeer& p, const std::string& action, const std::string& str) 
{
    const std::string &fmt_action = std::format("action|{}\n", action);
    std::vector<u_char> data(sizeof(int) + fmt_action.length() + str.length(), 0x00);
    
    data[0] = packet::GAME_MESSAGE;
    {
        const u_char *i8 = reinterpret_cast<const u_char*>(fmt_action.c_str());
        for (std::size_t i = 0ull; i < fmt_action.length(); ++i)
            data[sizeof(int) + i] = i8[i];
    }
    if (!str.empty())
    {
        const u_char *i8 = reinterpret_cast<const u_char*>(str.c_str());
        for (std::size_t i = 0ull; i < str.length(); ++i)
            data[sizeof(int) + fmt_action.length() + i] = i8[i];
    }
    
    enet_peer_send(&p, 0, enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE));
}

void send_data(ENetPeer &peer, const std::vector<u_char> &&data)
{
    ENetPacket *packet = enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE);
    if (packet == nullptr || packet->dataLength < sizeof(::state)) return;

    enet_peer_send(&peer, 1, packet);
}

void state_visuals(ENetPeer &peer, state &&state) 
{
    ::peer *pPeer = &peer_of(peer);

    peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer &p) 
    {
        send_data(p, compress_state(state));
    });
}

void tile_apply_damage(ENetEvent& event, state state, block &block, u_int value)
{
    ::peer *pPeer = &peer_of(event);

    if (::world* world = current_world(*pPeer))
        world->mark_dirty();

    (block.fg == 0) ? ++block.hits[1] : ++block.hits[0];
    state.type = (value << 24) | packet::TILE_APPLY_DAMAGE;
    state.uid = static_cast<int>(pPeer->clothing[hand]); // @note equipped hand item (PACKET item_net_id)
    state.id = 6; // @note idk exactly
    state.netid = pPeer->netid;
    state_visuals(*event.peer, std::move(state));
}

u_short modify_item_inventory(ENetEvent& event, ::slot slot)
{   
    ::peer *pPeer = &peer_of(event);

    ::state state{.id = slot.id};
    if (slot.count < 0) state.type = (slot.count*-1 << 16) | packet::MODIFY_ITEM_INVENTORY; // @note 0x00{}000d
    else                state.type = (slot.count    << 24) | packet::MODIFY_ITEM_INVENTORY; // @note 0x{}00000d
    state_visuals(*event.peer, std::move(state));

    return pPeer->emplace(::slot(slot.id, slot.count));
}

void item_change_object(ENetEvent& event, ::state state) 
{
    state.type = packet::ITEM_CHANGE_OBJECT;

    state_visuals(*event.peer, std::move(state));
}

void merge_object(ENetEvent& event, ::slot slot, const ::pos& pos, ::world &world)
{
    world.mark_dirty();
    auto object = std::ranges::find_if(world.objects, [&](const ::object &object) {
        return object.id == slot.id && (object.pos.by_32(true) == pos.by_32(true));
    });
    if (object == world.objects.end()) return;
    /* @todo avoid surpassing 200 and call add_object() for the remaining amount */
    object->count = static_cast<u_short>(std::min(200, static_cast<int>(object->count) + static_cast<int>(slot.count)));

    item_change_object(event, ::state{
        .netid = (int)0xfffffffd,
        .uid   = (int)object->uid,
        .count = static_cast<float>(object->count),
        .id    = object->id,
        .pos   = object->pos
    });
}

void remove_object(ENetEvent& event, signed uid)
{
    ::peer *pPeer = &peer_of(event);

    if (::world* world = current_world(*pPeer))
        world->mark_dirty();

    item_change_object(event, ::state{
        .netid = pPeer->netid,
        .uid   = (int)0xffffffff,
        .id    = uid
    });
}

void despawn_object(ENetEvent& event, signed uid)
{
    if (::world* world = current_world(event))
        world->mark_dirty();

    // @note netid -1: client deletes the drop without treating it as a player pickup
    item_change_object(event, ::state{
        .netid = -1,
        .uid   = (int)0xffffffff,
        .id    = uid
    });
}

int add_object(ENetEvent& event, ::slot slot, const ::pos& pos, ::world &world)
{
    world.mark_dirty();

    // @note Magplant suck: enabled plants with a matching filter absorb drops
    for (::magplant &mag : world.magplants)
    {
        if (!mag.enabled || mag.id == 0 || mag.id != static_cast<u_short>(slot.id))
            continue;
        if (mag.count >= ::magplant::CAPACITY)
            continue;

        const u_short room = static_cast<u_short>(::magplant::CAPACITY - mag.count);
        const u_short take = std::min(static_cast<u_short>(slot.count), room);
        mag.count = static_cast<u_short>(mag.count + take);
        slot.count = static_cast<short>(slot.count - take);

        ::block &mag_block = world.blocks[cord(mag.pos.x_int(), mag.pos.y_int())];
        send_tile_update(event, ::state{ .punch = mag.pos }, mag_block, world);

        if (slot.count <= 0)
            return 0;
    }

    /* @todo got a little messy */
    auto object = std::ranges::find_if(world.objects, [&](const ::object &object) {
        return object.id == slot.id && (object.pos.by_32(true) == pos.by_32(true));
    });
    if (object != world.objects.end() && object->count < 200/*@todo*/) 
    {
        merge_object(event, slot, pos, world);
        return object->uid;
    }
    const u_int uid = g_object_uid.fetch_add(1, std::memory_order_relaxed) + 1;
    if (uid > world.last_object_uid)
        world.last_object_uid = uid;
    ::object it = world.objects.emplace_back(::object(slot.id, slot.count, pos, uid));

    item_change_object(event, ::state{
        .netid = (int)0xffffffff,
        .uid   = (int)it.uid,
        .count = static_cast<float>(slot.count),
        .id    = it.id,
        .pos   = pos
    });
    return it.uid;
}

void add_drop(ENetEvent &event, ::slot im, ::pos pos, ::world &world) // @todo
{
    ransuu ransuu;
    add_object(event, im, ::pos{
        pos.x + ransuu[{0, 16}],
        pos.y + ransuu[{0, 16}]
    }, world);
}

u_short give_to_backpack(ENetEvent& event, short id, u_short amount)
{
    while (amount > 0)
    {
        const short give = static_cast<short>(std::min<u_short>(amount, 200));
        const u_short excess = modify_item_inventory(event, ::slot(id, give));
        const short actually = static_cast<short>(give - excess);
        if (actually <= 0) break; // backpack refused everything (full)
        amount = static_cast<u_short>(amount - actually);
        if (excess > 0) break;    // partial fit -> backpack now full
    }
    return amount;
}

void spill_drops(ENetEvent& event, short id, int count, ::pos at, ::world& world)
{
    while (count > 0)
    {
        const short give = static_cast<short>(std::min(count, 200));
        add_drop(event, ::slot(id, give), at, world);
        count -= give;
    }
}

void send_tile_update(ENetEvent &event, ::state state, ::block &block, ::world &world) 
{
    world.mark_dirty();
    state.type = packet::SEND_TILE_UPDATE_DATA;
    state.peer_state = peer_state::S_EXTENDED;
    std::vector<u_char> data = compress_state(state);

    short pos = sizeof(::state); // @note start after state bytes (as every packet has)
    data.resize(pos + 8ull); // @note {2} {2} 00 00 00 00
    
    *reinterpret_cast<short*>(&data[pos]) = block.fg; pos += sizeof(short);
    *reinterpret_cast<short*>(&data[pos]) = block.bg; pos += sizeof(short);
    pos += sizeof(short);
    
    data[pos++] = block.state[2] ;
    data[pos++] = block.state[3];
    const ::item &item = id_to_item(block.fg);
    switch (item.type)
    {
        case type::LOCK:
        {
            if (!is_tile_lock(block.fg)) world.is_public = (block.state[2] & S_PUBLIC);

            int lock_owner = world.owner;
            int access_count = std::ranges::count_if(world.access, std::identity{});
            u_char flags = world.lock_state;

            if (is_tile_lock(block.fg))
            {
                auto tl = std::ranges::find(world.tile_locks, state.punch, &::tile_lock::pos);
                if (tl != world.tile_locks.end())
                {
                    lock_owner = tl->owner;
                    access_count = std::ranges::count_if(tl->access, std::identity{});
                    flags = tl->is_public ? S_PUBLIC : 0;
                }
            }

            data.resize(data.size() + 1ull + 1ull + 4ull + 4ull + 4ull + (access_count * 4));

            data[pos++] = 0x03;
            data[pos++] = flags;
            *reinterpret_cast<int*>(&data[pos]) = lock_owner; pos += sizeof(int);
            *reinterpret_cast<int*>(&data[pos]) = access_count; pos += sizeof(int);
            /* @todo access list */
            break;
        }
        case type::DOOR:
        case type::PORTAL:
        {
            short len = block.label.length();
            data.resize(pos + 1ull + 2ull + len + 1ull); // @note 01 {2} {} 0 0

            data[pos++] = 0x01;
            
            *reinterpret_cast<short*>(&data[pos]) = len; pos += sizeof(short);
            for (const char &c : block.label) data[pos++] = c;
            data[pos++] = '\0';
            break;
        }
        case type::MAILBOX: // @note mailbox reuses the sign extra, matching join_request
        case type::SIGN:
        {
            short len = block.label.length();
            data.resize(pos + 1ull + 2ull + len + 4ull); // @note 02 {2} {} ff ff ff ff

            data[pos++] = 0x02;

            *reinterpret_cast<short*>(&data[pos]) = len; pos += sizeof(short);
            for (const char &c : block.label) data[pos++] = c;
            *reinterpret_cast<int*>(&data[pos]) = 0xffffffff; pos += sizeof(int);
            break;
        }
        case type::BULLETIN:
        case type::DONATION_BOX:
        {
            data.resize(pos + 1ull + 6ull + 1ull); // @note {1} 00 00 00 00 00 00 {1}

            data[pos++] = (item.type == type::BULLETIN) ? 0x07 : 0x0c;
            *reinterpret_cast<short*>(&data[pos]) = 0; pos += sizeof(short); // @note 3 empty strings
            *reinterpret_cast<short*>(&data[pos]) = 0; pos += sizeof(short);
            *reinterpret_cast<short*>(&data[pos]) = 0; pos += sizeof(short);
            data[pos++] = 0x00; // @note flags
            break;
        }
        case type::XENONITE:
        {
            data.resize(pos + 1ull + 1ull + 4ull);

            data[pos++] = 0x12;
            data[pos++] = 0x00; // @note flags
            *reinterpret_cast<u_int*>(&data[pos]) = 0; pos += sizeof(u_int); // @note flags2
            break;
        }
        case type::SPIRIT_BOARD:
        {
            data.resize(pos + 1ull + 4ull + 4ull + 4ull);

            data[pos++] = 0x44;
            *reinterpret_cast<int*>(&data[pos]) = 0; pos += sizeof(int); // @note player count
            *reinterpret_cast<short*>(&data[pos]) = 0; pos += sizeof(short); // @note 2 empty strings
            *reinterpret_cast<short*>(&data[pos]) = 0; pos += sizeof(short);
            *reinterpret_cast<u_int*>(&data[pos]) = 0; pos += sizeof(u_int); // @note item count
            break;
        }
        case type::SEED:
        {
            data.resize(pos + 1ull + 5ull);

            data[pos++] = 0x04;
            *reinterpret_cast<int*>(&data[pos]) = block_elapsed_seconds(block.tick); pos += sizeof(int);
            data[pos++] = 0x03; // @note fruit on tree
            break;
        }
        case type::PROVIDER:
        {
            data.resize(pos + 5ull);

            data[pos++] = 0x09;
            *reinterpret_cast<int*>(&data[pos]) = block_elapsed_seconds(block.tick); pos += sizeof(int);
            break;
        }
        case DISPLAY_BLOCK:
        {
            data.resize(pos + 1ull + 4ull);
            auto display = std::ranges::find(world.displays, state.punch, &::display::pos);

            data[pos++] = 0x17;
            *reinterpret_cast<int*>(&data[pos]) = (display != world.displays.end()) ? display->id : 0;
            pos += sizeof(int);
            break;
        }
        case type::VENDING_MACHINE:
        {
            data.resize(pos + 1ull + 4ull + 4ull);
            auto vend = std::ranges::find(world.vendings, state.punch, &::vending::pos);

            // @note client shows the stocked item only while count>0 and price!=0; else "out of order"
            const bool for_sale = vend != world.vendings.end() && vend->count > 0 && vend->price != 0;
            data[pos++] = 0x18;
            *reinterpret_cast<u_int*>(&data[pos]) = for_sale ? vend->id : 0;
            pos += sizeof(u_int);
            *reinterpret_cast<int*>(&data[pos]) = for_sale ? vend->price : 0;
            pos += sizeof(int);
            break;
        }
        case type::MAGPLANT:
        {
            data.resize(pos + 1ull + 4ull + 4ull + 1ull + 1ull + 2ull);
            auto mag = std::ranges::find(world.magplants, state.punch, &::magplant::pos);

            data[pos++] = 0x3e; // @note TILE_EXTRA magplant
            *reinterpret_cast<u_int*>(&data[pos]) = (mag != world.magplants.end()) ? mag->id : 0;
            pos += sizeof(u_int);
            *reinterpret_cast<u_int*>(&data[pos]) = (mag != world.magplants.end()) ? mag->count : 0;
            pos += sizeof(u_int);
            data[pos++] = (mag != world.magplants.end() && mag->enabled) ? 1 : 0;
            data[pos++] = 0; // @note magnetron
            *reinterpret_cast<u_short*>(&data[pos]) = ::magplant::CAPACITY;
            pos += sizeof(u_short);
            break;
        }
    }

    ::peer *pPeer = &peer_of(event);
    peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer& p) 
    {
        send_data(p, std::vector<u_char>(data)); // @note copy — move would empty after the first peer
    });
}

void send_particle_effect(ENetEvent &event, const ::pos& pos, ::pos speed, int id, float offset)
{
    state_visuals(*event.peer, ::state{
        .type = packet::SEND_PARTICLE_EFFECT,
        .netid = id, // @todo figure out if this is correct, i just assumed from firework visuals
        .id = id,
        .pos = pos,
        .speed = speed,
        .idk = offset
    });
}

void remove_fire(ENetEvent &event, state state, ::block &block, ::world& world)
{
    send_particle_effect(event, state.punch.by_32(), {0x00, 0x95});

    block.state[3] &= ~S_FIRE;
    send_tile_update(event, state, block, world);

    ::peer *pPeer = &peer_of(event);

    if (++pPeer->fires_removed % 100 == 0) 
    {
        on::ConsoleMessage(event.peer, "`oI'm so good at fighting fires, I rescused this `2Highly Combustible Box``!");
        modify_item_inventory(event, {3090/*Combustible Box*/, 1});
    }
    pPeer->add_xp(event, 1);
}

void fireworks(ENetEvent &event, const ::pos& pos)
{
    ransuu ransuu;
    int type  [3]{ ransuu[{0x25, 0x28}], ransuu[{0x25, 0x28}], ransuu[{0x25, 0x28}] };
    int offset[3]{ ransuu[{260, 2200}], ransuu[{260, 2200}], ransuu[{260, 2200}] };

    send_particle_effect(event, pos, {0xb3, type[0]}, 0xc8*0, offset[0]);
    send_particle_effect(event, pos, {0xbe, type[1]}, 0xc8*1, offset[1]);
    send_particle_effect(event, pos, {0x7c, type[2]}, 0xc8*2, offset[2]);
}

