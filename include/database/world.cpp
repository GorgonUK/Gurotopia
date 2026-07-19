#include "pch.hpp"
#include "tools/ransuu.hpp"
#include "on/ConsoleMessage.hpp"
#include "database.hpp"
#include "server_config.hpp"

#include <cstring>
#include <ctime>

#include "world.hpp"

using namespace std::chrono;
using namespace std::literals::chrono_literals; // @note for 'ms' 's' (millisec, seconds)

namespace
{
    constexpr u_int WORLD_MAGIC = 0x44525747u; // 'GWRD'
    constexpr u_short WORLD_VERSION = 1;
    constexpr std::size_t WORLD_BLOCKS = 100ull * 60ull;
    constexpr std::size_t MAX_OBJECTS = 10000ull;
    constexpr std::size_t MAX_DOORS = 1000ull;
    constexpr std::size_t MAX_DISPLAYS = 1000ull;
    constexpr std::size_t MAX_RANDOM = 1000ull;
    constexpr std::size_t MAX_LABEL = 256ull;

    class ByteWriter
    {
    public:
        std::vector<u_char> buf;

        void write_bytes(const void *p, std::size_t n)
        {
            const u_char *b = static_cast<const u_char*>(p);
            buf.insert(buf.end(), b, b + n);
        }
        void write_u8(u_char v) { buf.push_back(v); }
        void write_u16(u_short v) { write_bytes(&v, sizeof(v)); }
        void write_i16(short v) { write_bytes(&v, sizeof(v)); }
        void write_u32(u_int v) { write_bytes(&v, sizeof(v)); }
        void write_i32(int v) { write_bytes(&v, sizeof(v)); }
        void write_i64(long long v) { write_bytes(&v, sizeof(v)); }
        void write_f32(float v) { write_bytes(&v, sizeof(v)); }
        void write_str(const std::string &s)
        {
            if (s.size() > 0xffff) throw std::runtime_error("string too long");
            write_u16(static_cast<u_short>(s.size()));
            write_bytes(s.data(), s.size());
        }
    };

    class ByteReader
    {
    public:
        const u_char *data{};
        std::size_t size{};
        std::size_t pos{};

        void need(std::size_t n) const
        {
            if (pos + n > size)
                throw std::runtime_error("world blob truncated");
        }
        void read_bytes(void *out, std::size_t n)
        {
            need(n);
            std::memcpy(out, data + pos, n);
            pos += n;
        }
        u_char read_u8() { u_char v; read_bytes(&v, 1); return v; }
        u_short read_u16() { u_short v; read_bytes(&v, sizeof(v)); return v; }
        short read_i16() { short v; read_bytes(&v, sizeof(v)); return v; }
        u_int read_u32() { u_int v; read_bytes(&v, sizeof(v)); return v; }
        int read_i32() { int v; read_bytes(&v, sizeof(v)); return v; }
        long long read_i64() { long long v; read_bytes(&v, sizeof(v)); return v; }
        float read_f32() { float v; read_bytes(&v, sizeof(v)); return v; }
        std::string read_str(std::size_t max_len = MAX_LABEL)
        {
            u_short len = read_u16();
            if (len > max_len)
                throw std::runtime_error("world string too long");
            need(len);
            std::string s(reinterpret_cast<const char*>(data + pos), len);
            pos += len;
            return s;
        }
    };

    std::vector<u_char> encode_world(const ::world &world)
    {
        if (world.blocks.size() != WORLD_BLOCKS)
            throw std::runtime_error("invalid block count");
        if (world.objects.size() > MAX_OBJECTS || world.doors.size() > MAX_DOORS ||
            world.displays.size() > MAX_DISPLAYS || world.random_blocks.size() > MAX_RANDOM)
            throw std::runtime_error("world collection too large");

        ByteWriter w;
        w.write_u32(WORLD_MAGIC);
        w.write_u16(WORLD_VERSION);
        for (int id : world.access)
            w.write_i32(id);

        w.write_u32(static_cast<u_int>(world.blocks.size()));
        for (const ::block &b : world.blocks)
        {
            w.write_i16(b.fg);
            w.write_i16(b.bg);
            w.write_i64(static_cast<long long>(b.tick));
            w.write_bytes(b.state, 4);
            w.write_bytes(b.hits, 2);
            w.write_str(b.label);
        }

        w.write_u32(static_cast<u_int>(world.objects.size()));
        for (const ::object &o : world.objects)
        {
            w.write_u16(o.id);
            w.write_u16(o.count);
            w.write_f32(o.pos.x);
            w.write_f32(o.pos.y);
            w.write_u32(o.uid);
        }

        w.write_u32(static_cast<u_int>(world.doors.size()));
        for (const ::door &d : world.doors)
        {
            w.write_str(d.dest);
            w.write_str(d.id);
            w.write_str(d.password);
            w.write_f32(d.pos.x);
            w.write_f32(d.pos.y);
        }

        w.write_u32(static_cast<u_int>(world.displays.size()));
        for (const ::display &d : world.displays)
        {
            w.write_u32(d.id);
            w.write_f32(d.pos.x);
            w.write_f32(d.pos.y);
        }

        w.write_u32(static_cast<u_int>(world.random_blocks.size()));
        for (const ::random_block &r : world.random_blocks)
        {
            w.write_u8(r.value);
            w.write_f32(r.pos.x);
            w.write_f32(r.pos.y);
        }

        return w.buf;
    }

    void decode_world(::world &world, const std::vector<u_char> &blob)
    {
        ByteReader r{ blob.data(), blob.size(), 0 };
        if (r.read_u32() != WORLD_MAGIC)
            throw std::runtime_error("bad world magic");
        if (r.read_u16() != WORLD_VERSION)
            throw std::runtime_error("unsupported world version");

        for (int &id : world.access)
            id = r.read_i32();

        u_int block_count = r.read_u32();
        if (block_count != WORLD_BLOCKS)
            throw std::runtime_error("unexpected block count");

        world.blocks.clear();
        world.blocks.reserve(block_count);
        for (u_int i = 0; i < block_count; ++i)
        {
            ::block b;
            b.fg = r.read_i16();
            b.bg = r.read_i16();
            b.tick = static_cast<std::time_t>(r.read_i64());
            r.read_bytes(b.state, 4);
            r.read_bytes(b.hits, 2);
            b.label = r.read_str();
            world.blocks.push_back(std::move(b));
        }

        u_int object_count = r.read_u32();
        if (object_count > MAX_OBJECTS)
            throw std::runtime_error("too many objects");
        world.objects.clear();
        world.objects.reserve(object_count);
        for (u_int i = 0; i < object_count; ++i)
        {
            u_short id = r.read_u16();
            u_short count = r.read_u16();
            float x = r.read_f32();
            float y = r.read_f32();
            u_int uid = r.read_u32();
            world.objects.emplace_back(id, count, ::pos{x, y}, uid);
        }

        u_int door_count = r.read_u32();
        if (door_count > MAX_DOORS)
            throw std::runtime_error("too many doors");
        world.doors.clear();
        for (u_int i = 0; i < door_count; ++i)
        {
            std::string dest = r.read_str(64);
            std::string id = r.read_str(64);
            std::string password = r.read_str(64);
            float x = r.read_f32();
            float y = r.read_f32();
            world.doors.emplace_back(std::move(dest), std::move(id), std::move(password), ::pos{x, y});
        }

        u_int display_count = r.read_u32();
        if (display_count > MAX_DISPLAYS)
            throw std::runtime_error("too many displays");
        world.displays.clear();
        for (u_int i = 0; i < display_count; ++i)
        {
            u_int id = r.read_u32();
            float x = r.read_f32();
            float y = r.read_f32();
            world.displays.emplace_back(id, ::pos{x, y});
        }

        u_int random_count = r.read_u32();
        if (random_count > MAX_RANDOM)
            throw std::runtime_error("too many random blocks");
        world.random_blocks.clear();
        for (u_int i = 0; i < random_count; ++i)
        {
            u_char value = r.read_u8();
            float x = r.read_f32();
            float y = r.read_f32();
            world.random_blocks.emplace_back(value, ::pos{x, y});
        }

        if (r.pos != r.size)
            throw std::runtime_error("trailing world blob data");
    }
}

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

bool world_save(const ::world &world)
{
    if (world.name.empty() || world.blocks.size() != WORLD_BLOCKS)
        return false;

    try
    {
        std::vector<u_char> blob = encode_world(world);
        signed owner = world.owner;
        unsigned is_public = world.is_public ? 1u : 0u;
        unsigned lock_state = world.lock_state;
        unsigned min_level = world.minimum_entry_level;
        float weather_x = world.weather.x;
        float weather_y = world.weather.y;
        unsigned last_uid = world.last_object_uid;

        ::hStmt stmt{
            "INSERT INTO world "
            "(name, owner, is_public, lock_state, minimum_entry_level, weather_x, weather_y, last_object_uid, data) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON DUPLICATE KEY UPDATE "
            "owner=VALUES(owner), is_public=VALUES(is_public), lock_state=VALUES(lock_state), "
            "minimum_entry_level=VALUES(minimum_entry_level), weather_x=VALUES(weather_x), "
            "weather_y=VALUES(weather_y), last_object_uid=VALUES(last_object_uid), data=VALUES(data)"
        };

        MYSQL_BIND params[9] = {
            make_bind_in(world.name),
            make_bind_in(owner),
            make_bind_in(is_public),
            make_bind_in(lock_state),
            make_bind_in(min_level),
            make_bind_in(weather_x),
            make_bind_in(weather_y),
            make_bind_in(last_uid),
            make_bind_in_blob(blob)
        };

        if (mysql_stmt_bind_param(stmt.pStmt, params) || mysql_stmt_execute(stmt.pStmt))
        {
            fprintf(stderr, "[world save] %s: %s\n", world.name.c_str(), mysql_stmt_error(stmt.pStmt));
            return false;
        }
        return true;
    }
    catch (const std::exception &ex)
    {
        fprintf(stderr, "[world save] %s: %s\n", world.name.c_str(), ex.what());
        return false;
    }
}

bool world_load(::world &world, const std::string &name)
{
    ::hStmt stmt{
        "SELECT owner, is_public, lock_state, minimum_entry_level, weather_x, weather_y, "
        "last_object_uid, data FROM world WHERE name = ? LIMIT 1"
    };

    MYSQL_BIND param = make_bind_in(name);
    if (mysql_stmt_bind_param(stmt.pStmt, &param))
    {
        fprintf(stderr, "[world load] bind: %s\n", mysql_stmt_error(stmt.pStmt));
        return false;
    }

    signed owner = 0;
    unsigned is_public = 0, lock_state = 0, min_level = 1, last_uid = 0;
    float weather_x = 0, weather_y = 0;
    std::vector<u_char> blob;
    unsigned long blob_len = 0;
    blob.resize(2u << 20); // 2 MiB

    MYSQL_BIND results[8] = {
        make_bind_out(owner),
        make_bind_out(is_public),
        make_bind_out(lock_state),
        make_bind_out(min_level),
        make_bind_out(weather_x),
        make_bind_out(weather_y),
        make_bind_out(last_uid),
        make_bind_out_blob(blob, blob_len)
    };

    if (mysql_stmt_bind_result(stmt.pStmt, results))
    {
        fprintf(stderr, "[world load] bind result: %s\n", mysql_stmt_error(stmt.pStmt));
        return false;
    }

    if (mysql_stmt_execute(stmt.pStmt) || mysql_stmt_store_result(stmt.pStmt))
    {
        fprintf(stderr, "[world load] exec: %s\n", mysql_stmt_error(stmt.pStmt));
        return false;
    }

    if (mysql_stmt_num_rows(stmt.pStmt) == 0)
        return false;

    int fetch = mysql_stmt_fetch(stmt.pStmt);
    if (fetch != 0 && fetch != MYSQL_DATA_TRUNCATED)
    {
        fprintf(stderr, "[world load] fetch: %s\n", mysql_stmt_error(stmt.pStmt));
        return false;
    }
    if (blob_len > blob.size())
    {
        fprintf(stderr, "[world load] %s: blob too large (%lu)\n", name.c_str(), blob_len);
        return false;
    }
    trim_blob(blob, blob_len);

    try
    {
        decode_world(world, blob);
        world.name = name;
        world.owner = owner;
        world.is_public = (is_public != 0);
        world.lock_state = static_cast<u_char>(lock_state);
        world.minimum_entry_level = static_cast<u_char>(min_level);
        world.weather = ::pos{weather_x, weather_y};
        world.last_object_uid = last_uid;
        world.visitors = 0;
        world.netid_counter = 0;
        world.dirty = false;
        return true;
    }
    catch (const std::exception &ex)
    {
        fprintf(stderr, "[world load] %s: %s\n", name.c_str(), ex.what());
        return false;
    }
}

void autosave_worlds()
{
    for (::world &w : worlds)
    {
        if (!w.dirty)
            continue;
        if (world_save(w))
            w.dirty = false;
    }
}

void save_all_worlds()
{
    for (::world &w : worlds)
    {
        if (world_save(w))
            w.dirty = false;
    }
}

world::world(const std::string& name) 
{
    this->name = name;
}

std::vector<world> worlds;

void send_action(ENetPeer& p, const std::string& action, const std::string& str) 
{
    const std::string &fmt_action = std::format("action|{}\n", action);
    std::vector<u_char> data(sizeof(int) + fmt_action.length() + str.length(), 0x00);
    
    data[0] = 3; // @note NET_MESSAGE_GAME_MESSAGE
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
    ::peer *pPeer = static_cast<::peer*>(peer.data);

    peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer &p) 
    {
        send_data(p, compress_state(state));
    });
}

void tile_apply_damage(ENetEvent& event, state state, block &block, u_int value)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
    if (world != worlds.end())
        world->mark_dirty();

    (block.fg == 0) ? ++block.hits[1] : ++block.hits[0];
    state.type = (value << 24) | 0x000008; // @note 0x{}000008
    state.id = 6; // @note idk exactly
    state.netid = pPeer->netid;
	state_visuals(*event.peer, std::move(state));
}

u_short modify_item_inventory(ENetEvent& event, ::slot slot)
{   
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    ::state state{.id = slot.id};
    if (slot.count < 0) state.type = (slot.count*-1 << 16) | 0x000d; // @noote 0x00{}000d
    else                state.type = (slot.count    << 24) | 0x000d; // @noote 0x{}00000d
    state_visuals(*event.peer, std::move(state));

    return pPeer->emplace(::slot(slot.id, slot.count));
}

void item_change_object(ENetEvent& event, ::state state) 
{
    state.type = 0x0e; // @note PACKET_ITEM_CHANGE_OBJECT

    state_visuals(*event.peer, std::move(state));
}

void merge_object(ENetEvent& event, ::slot slot, const ::pos& pos, ::world &world)
{
    world.mark_dirty();
    auto object = std::ranges::find_if(world.objects, [&](const ::object &object) {
        return object.id == slot.id && (object.pos.by_32(true) == pos.by_32(true));
    });
    /* @todo avoid surpassing 200 and call add_object() for the remaining amount */ // @note future self reference peer::emplace()...
    object->count += slot.count;

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
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
    if (world != worlds.end())
        world->mark_dirty();

    item_change_object(event, ::state{
        .netid = pPeer->netid,
        .uid   = (int)0xffffffff,
        .id    = uid
    });
}

int add_object(ENetEvent& event, ::slot slot, const ::pos& pos, ::world &world)
{
    world.mark_dirty();
    /* @todo got a little messy */
    auto object = std::ranges::find_if(world.objects, [&](const ::object &object) {
        return object.id == slot.id && (object.pos.by_32(true) == pos.by_32(true));
    });
    if (object != world.objects.end() && object->count < 200/*@todo*/) 
    {
        merge_object(event, slot, pos, world);
        return object->uid;
    }
    ::object it = world.objects.emplace_back(::object(slot.id, slot.count, pos, ++world.last_object_uid)); // @note a iterator ahead of time

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

void send_tile_update(ENetEvent &event, ::state state, ::block &block, ::world &world) 
{
    world.mark_dirty();
    state.type = 05; // @note PACKET_SEND_TILE_UPDATE_DATA
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
            if (!is_tile_lock(block.fg)) world.is_public = (block.state[2] & S_PUBLIC); // @note check if world lock has S_PUBLIC flag, i will change this later

            int access = std::ranges::count_if(world.access, std::identity{});
            data.resize(data.size() + 1ull + 1ull + 4ull + 4ull + 4ull + (access * 4));

            data[pos++] = 0x03;
            data[pos++] = world.lock_state;
            *reinterpret_cast<int*>(&data[pos]) = world.owner; pos += sizeof(int);
            *reinterpret_cast<int*>(&data[pos]) = access; pos += sizeof(int);
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
            *reinterpret_cast<int*>(&data[pos]) = display->id; pos += sizeof(int);
            break;
        }
    }

    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer& p) 
    {
        send_data(p, std::move(data));
    });
}

void send_particle_effect(ENetEvent &event, const ::pos& pos, ::pos speed, int id, float offset)
{
    state_visuals(*event.peer, ::state{
        .type = 0x11, // @note PACKET_SEND_PARTICLE_EFFECT
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

    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

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

void generate_world(::world &world, const std::string& name)
{
    ransuu ransuu;
    u_short main_door = ransuu[{2, cord(0, 60) / 100 - 4}];
    std::vector<::block> blocks(cord(0, 60), ::block{0, 0});
    
    for (std::size_t i = 0ull; i < blocks.size(); ++i)
    {
        ::block &block = blocks[i];
        if (i >= cord(0, 37))
        {
            block.bg = 14; // @note cave background
            if (i >= cord(0, 38) && i < cord(0, 50) /* (above) lava level */ && ransuu[{0, 38}] <= 1) block.fg = 10; // rock
            else if (i > cord(0, 50) && i < cord(0, 54) /* (above) bedrock level */ && ransuu[{0, 8}] < 3) block.fg = 4; // lava
            else block.fg = (i >= cord(0, 54)) ? 8 : 2;
        }
        if (i == cord(main_door, 36)) block.fg = 6, block.label = "EXIT"; // @note main door
        else if (i == cord(main_door, 37)) block.fg = 8; // @note bedrock (below main door)
    }
    world.blocks = std::move(blocks);
    world.name = std::move(name);
    world.mark_dirty();
}

bool door_mover(::world &world, const ::pos &pos)
{
    std::vector<::block> &blocks = world.blocks;

    if (blocks[cord(pos.x, pos.y)].fg != 0 ||
        blocks[cord(pos.x, (pos.y + 1))].fg != 0) return false;

    for (std::size_t i = 0ull; i < blocks.size(); ++i)
    {
        if (blocks[i].fg == 6/*Main Door*/)
        {
            blocks[i].fg = 0; // @note remove main door
            blocks[cord(i % 100, (i / 100 + 1))].fg = 0; // @note remove bedrock below
            break;
        }
    }
    blocks[cord(pos.x, pos.y)].fg = 6;
    blocks[cord(pos.x, (pos.y + 1))].fg = 8;
    world.mark_dirty();
    return true;
}

void blast::thermonuclear(::world &world, const std::string& name)
{
    ransuu ransuu;

    u_short main_door = ransuu[{2, cord(0, 60) / 100 - 4}];
    std::vector<::block> blocks(cord(0, 60), ::block{0, 0});
    for (std::size_t i = 0ull; i < blocks.size(); ++i)
    {
        blocks[i].fg = (i >= cord(0, 54)) ? 8 : 0;

        if (i == cord(main_door, 36)) blocks[i].fg = 6; // @note main door
        else if (i == cord(main_door, 37)) blocks[i].fg = 8; // @note bedrock (below main door)
    }
    world.blocks = std::move(blocks);
    world.name = std::move(name);
    world.mark_dirty();
}
