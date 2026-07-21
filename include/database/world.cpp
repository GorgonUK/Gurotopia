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

namespace
{
    constexpr u_int WORLD_MAGIC = 0x44525747u; // 'GWRD'
    constexpr u_short WORLD_VERSION = 5; // @note v5 appends combiners
    constexpr std::size_t MAX_COMBINERS = 500ull;
    constexpr std::size_t MAX_COMBINER_SLOTS = 100ull;
    constexpr std::size_t WORLD_BLOCKS = 100ull * 60ull;
    constexpr std::size_t MAX_OBJECTS = 10000ull;
    constexpr std::size_t MAX_DOORS = 1000ull;
    constexpr std::size_t MAX_DISPLAYS = 1000ull;
    constexpr std::size_t MAX_VENDINGS = 1000ull;
    constexpr std::size_t MAX_MAGPLANTS = 500ull;
    constexpr std::size_t MAX_TILE_LOCKS = 500ull;
    constexpr std::size_t MAX_TILE_LOCK_AREA = 256ull;
    constexpr std::size_t MAX_RANDOM = 1000ull;
    constexpr std::size_t MAX_LETTERS = 1000ull;
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
            world.displays.size() > MAX_DISPLAYS || world.vendings.size() > MAX_VENDINGS ||
            world.magplants.size() > MAX_MAGPLANTS || world.tile_locks.size() > MAX_TILE_LOCKS ||
            world.combiners.size() > MAX_COMBINERS || world.random_blocks.size() > MAX_RANDOM)
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

        w.write_u32(static_cast<u_int>(world.letters.size()));
        for (const ::letter &l : world.letters)
        {
            w.write_i32(l.uid);
            w.write_str(l.from);
            w.write_str(l.message);
            w.write_f32(l.pos.x);
            w.write_f32(l.pos.y);
            w.write_i16(l.im.id);
            w.write_i16(l.im.count);
        }

        w.write_u32(static_cast<u_int>(world.vendings.size()));
        for (const ::vending &v : world.vendings)
        {
            w.write_f32(v.pos.x);
            w.write_f32(v.pos.y);
            w.write_u16(v.id);
            w.write_u16(v.count);
            w.write_i32(v.price);
            w.write_u16(v.earned);
        }

        w.write_u32(static_cast<u_int>(world.magplants.size()));
        for (const ::magplant &m : world.magplants)
        {
            w.write_f32(m.pos.x);
            w.write_f32(m.pos.y);
            w.write_u16(m.id);
            w.write_u16(m.count);
            w.write_u8(m.enabled ? 1 : 0);
        }

        w.write_u32(static_cast<u_int>(world.tile_locks.size()));
        for (const ::tile_lock &tl : world.tile_locks)
        {
            w.write_f32(tl.pos.x);
            w.write_f32(tl.pos.y);
            w.write_u16(tl.lock_id);
            w.write_i32(tl.owner);
            w.write_u8(tl.is_public ? 1 : 0);
            for (int uid : tl.access)
                w.write_i32(uid);
            w.write_u32(static_cast<u_int>(tl.area.size()));
            for (const ::pos &p : tl.area)
            {
                w.write_f32(p.x);
                w.write_f32(p.y);
            }
        }

        w.write_u32(static_cast<u_int>(world.combiners.size()));
        for (const ::combiner &c : world.combiners)
        {
            w.write_f32(c.pos.x);
            w.write_f32(c.pos.y);
            w.write_u32(static_cast<u_int>(c.contents.size()));
            for (const ::slot &s : c.contents)
            {
                w.write_i16(s.id);
                w.write_i16(s.count);
            }
        }

        return w.buf;
    }

    void decode_world(::world &world, const std::vector<u_char> &blob)
    {
        ByteReader r{ blob.data(), blob.size(), 0 };
        if (r.read_u32() != WORLD_MAGIC)
            throw std::runtime_error("bad world magic");
        const u_short version = r.read_u16();
        if (version < 1 || version > WORLD_VERSION)
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

        world.letters.clear();
        if (version >= 2)
        {
            u_int letter_count = r.read_u32();
            if (letter_count > MAX_LETTERS)
                throw std::runtime_error("too many letters");
            for (u_int i = 0; i < letter_count; ++i)
            {
                int uid = r.read_i32();
                std::string from = r.read_str(32);
                std::string message = r.read_str();
                float x = r.read_f32();
                float y = r.read_f32();
                short im_id = r.read_i16();
                short im_count = r.read_i16();
                world.letters.emplace_back(uid, std::move(from), std::move(message), ::pos{x, y}, ::slot{im_id, im_count});
            }
        }

        world.vendings.clear();
        world.magplants.clear();
        if (version >= 3)
        {
            u_int vending_count = r.read_u32();
            if (vending_count > MAX_VENDINGS)
                throw std::runtime_error("too many vendings");
            for (u_int i = 0; i < vending_count; ++i)
            {
                float x = r.read_f32();
                float y = r.read_f32();
                u_short id = r.read_u16();
                u_short count = r.read_u16();
                int price = r.read_i32();
                u_short earned = r.read_u16();
                world.vendings.emplace_back(::pos{x, y}, id, count, price, earned);
            }

            u_int magplant_count = r.read_u32();
            if (magplant_count > MAX_MAGPLANTS)
                throw std::runtime_error("too many magplants");
            for (u_int i = 0; i < magplant_count; ++i)
            {
                float x = r.read_f32();
                float y = r.read_f32();
                u_short id = r.read_u16();
                u_short count = r.read_u16();
                bool enabled = r.read_u8() != 0;
                world.magplants.emplace_back(::pos{x, y}, id, count, enabled);
            }
        }

        world.tile_locks.clear();
        if (version >= 4)
        {
            u_int lock_count = r.read_u32();
            if (lock_count > MAX_TILE_LOCKS)
                throw std::runtime_error("too many tile locks");
            for (u_int i = 0; i < lock_count; ++i)
            {
                float x = r.read_f32();
                float y = r.read_f32();
                u_short lock_id = r.read_u16();
                int owner = r.read_i32();
                bool is_public = r.read_u8() != 0;
                ::tile_lock tl(::pos{x, y}, lock_id, owner, is_public);
                for (int &uid : tl.access)
                    uid = r.read_i32();
                u_int area_count = r.read_u32();
                if (area_count > MAX_TILE_LOCK_AREA)
                    throw std::runtime_error("tile lock area too large");
                tl.area.reserve(area_count);
                for (u_int a = 0; a < area_count; ++a)
                {
                    float ax = r.read_f32();
                    float ay = r.read_f32();
                    tl.area.emplace_back(ax, ay);
                }
                world.tile_locks.push_back(std::move(tl));
            }
        }

        world.combiners.clear();
        if (version >= 5)
        {
            u_int combiner_count = r.read_u32();
            if (combiner_count > MAX_COMBINERS)
                throw std::runtime_error("too many combiners");
            for (u_int i = 0; i < combiner_count; ++i)
            {
                float x = r.read_f32();
                float y = r.read_f32();
                ::combiner c(::pos{x, y});
                u_int slot_count = r.read_u32();
                if (slot_count > MAX_COMBINER_SLOTS)
                    throw std::runtime_error("combiner holds too many stacks");
                c.contents.reserve(slot_count);
                for (u_int s = 0; s < slot_count; ++s)
                {
                    short id = r.read_i16();
                    short count = r.read_i16();
                    c.contents.emplace_back(id, count);
                }
                world.combiners.push_back(std::move(c));
            }
        }

        if (r.pos != r.size)
            throw std::runtime_error("trailing world blob data");
    }
}

int tile_lock_capacity(u_short lock_id)
{
    switch (lock_id)
    {
        case 202: return 10;   // Small Lock
        case 204: return 48;   // Big Lock
        case 206: return 200;  // Huge Lock
        case 4994: return 200; // Builder's Lock
        default: return 10;
    }
}

std::vector<::pos> claim_tile_lock_area(::world &world, ::pos lock_pos, int capacity)
{
    std::vector<::pos> claimed;
    claimed.reserve(static_cast<std::size_t>(std::max(1, capacity)));
    claimed.push_back(lock_pos);

    if (capacity <= 1) return claimed;

    std::deque<::pos> queue;
    std::unordered_set<int> visited;
    visited.insert(cord(lock_pos.x_int(), lock_pos.y_int()));

    auto try_push = [&](int x, int y)
    {
        if (!in_bounds(x, y)) return;
        const int idx = cord(x, y);
        if (!visited.insert(idx).second) return;

        // skip tiles already claimed by another tile lock
        if (tile_lock_at(world, ::pos{x, y}) != nullptr) return;

        const ::block &b = world.blocks[idx];
        // don't expand through bedrock / main door (keeps claims local)
        const ::item &fg = id_to_item(b.fg);
        if (fg.type == type::STRONG || fg.type == type::MAIN_DOOR) return;

        queue.emplace_back(x, y);
    };

    try_push(lock_pos.x_int() - 1, lock_pos.y_int());
    try_push(lock_pos.x_int() + 1, lock_pos.y_int());
    try_push(lock_pos.x_int(), lock_pos.y_int() - 1);
    try_push(lock_pos.x_int(), lock_pos.y_int() + 1);

    while (!queue.empty() && static_cast<int>(claimed.size()) < capacity)
    {
        ::pos cur = queue.front();
        queue.pop_front();
        claimed.push_back(cur);

        try_push(cur.x_int() - 1, cur.y_int());
        try_push(cur.x_int() + 1, cur.y_int());
        try_push(cur.x_int(), cur.y_int() - 1);
        try_push(cur.x_int(), cur.y_int() + 1);
    }

    return claimed;
}

::tile_lock *tile_lock_at(::world &world, ::pos punch)
{
    for (::tile_lock &tl : world.tile_locks)
        if (std::ranges::find(tl.area, punch) != tl.area.end() || tl.pos == punch)
            return &tl;
    return nullptr;
}

const ::tile_lock *tile_lock_at(const ::world &world, ::pos punch)
{
    for (const ::tile_lock &tl : world.tile_locks)
        if (std::ranges::find(tl.area, punch) != tl.area.end() || tl.pos == punch)
            return &tl;
    return nullptr;
}

bool peer_can_edit_tile(const ::peer *pPeer, const ::world &world, ::pos punch)
{
    if (pPeer == nullptr) return false;
    if (pPeer->role) return true;

    // world lock gate (same as previous tile_change check)
    if (world.owner && !world.is_public)
    {
        if (pPeer->user_id != world.owner &&
            std::ranges::find(world.access, pPeer->user_id) == world.access.end())
            return false;
    }

    const ::tile_lock *tl = tile_lock_at(world, punch);
    if (tl == nullptr) return true;
    if (tl->is_public) return true;
    if (pPeer->user_id == tl->owner) return true;
    if (std::ranges::find(tl->access, pPeer->user_id) != tl->access.end()) return true;
    // world owner can always edit under their world lock
    if (world.owner && pPeer->user_id == world.owner) return true;
    return false;
}

void apply_tile_lock(::world &world, ::tile_lock &lock)
{
    for (const ::pos &p : lock.area)
    {
        if (!in_bounds(p)) continue;
        world.blocks[cord(p.x_int(), p.y_int())].state[2] |= S_LOCKED;
    }
    world.mark_dirty();
}

void remove_tile_lock(::world &world, ::pos lock_pos)
{
    auto it = std::ranges::find(world.tile_locks, lock_pos, &::tile_lock::pos);
    if (it == world.tile_locks.end()) return;

    for (const ::pos &p : it->area)
    {
        if (!in_bounds(p)) continue;
        // only clear if no other lock still covers this tile
        ::pos check = p;
        bool still = false;
        for (const ::tile_lock &other : world.tile_locks)
        {
            if (other.pos == lock_pos) continue;
            if (std::ranges::find(other.area, check) != other.area.end() || other.pos == check)
            {
                still = true;
                break;
            }
        }
        if (!still)
            world.blocks[cord(p.x_int(), p.y_int())].state[2] &= ~S_LOCKED;
    }
    world.tile_locks.erase(it);
    world.mark_dirty();
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

bool world_has_xenonite(const ::world &world)
{
    return std::ranges::any_of(world.blocks, [](const ::block &block) 
    { 
        return block.fg != 0 && id_to_item(block.fg).type == type::XENONITE; 
    });
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
        for (const ::object &o : world.objects)
        {
            if (o.uid > world.last_object_uid)
                world.last_object_uid = o.uid;
            note_object_uid(o.uid);
        }
        note_object_uid(world.last_object_uid);
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
    state.type = (value << 24) | 0x000008; // @note 0x{}000008
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

void generate_world(::world &world, const std::string& name)
{
    ransuu ransuu;
    // @note real GT: 100 wide × 60 tall; main door at y=24, bedrock under it at y=25,
    // dirt/cave from y=25, lava near y=50–53, bottom 6 rows (y=54–59) are bedrock.
    constexpr int WIDTH = 100;
    constexpr int HEIGHT = 60;
    constexpr int DOOR_Y = 24;
    constexpr int SURFACE_Y = 25;
    constexpr int ROCK_Y = 26;
    constexpr int LAVA_Y = 50;
    constexpr int BEDROCK_Y = 54;

    const u_short main_door = static_cast<u_short>(ransuu[{2, WIDTH - 4}]);
    std::vector<::block> blocks(static_cast<std::size_t>(WIDTH * HEIGHT), ::block{0, 0});

    for (std::size_t i = 0ull; i < blocks.size(); ++i)
    {
        ::block &block = blocks[i];
        const int y = static_cast<int>(i / WIDTH);

        if (y >= SURFACE_Y)
        {
            block.bg = 14; // cave background
            if (y >= ROCK_Y && y < LAVA_Y && ransuu[{0, 38}] <= 1)
                block.fg = 10; // rock
            else if (y >= LAVA_Y && y < BEDROCK_Y && ransuu[{0, 8}] < 3)
                block.fg = 4; // lava
            else
                block.fg = (y >= BEDROCK_Y) ? 8 : 2; // bedrock / dirt
        }
    }

    // Place exit AFTER terrain fill. Never use cord(door, DOOR_Y + 1) — without
    // parenthesized macro args that used to put bedrock in the sky, not under the door.
    {
        const std::size_t door_i = static_cast<std::size_t>(DOOR_Y) * static_cast<std::size_t>(WIDTH) + main_door;
        const std::size_t pad_i = door_i + static_cast<std::size_t>(WIDTH);
        blocks[door_i].fg = 6;
        blocks[door_i].label = "EXIT";
        blocks[pad_i].fg = 8;
        blocks[pad_i].bg = 14;
    }

    world.blocks = std::move(blocks);
    world.name = std::move(name);
    world.objects.clear();
    // Keep the global drop-uid high-water so the client baseline does not reset to 0
    // when entering a brand-new world after one that already spawned drops.
    world.last_object_uid = g_object_uid.load();
    ensure_main_door_bedrock(world);
    world.mark_dirty();
}

void ensure_main_door_bedrock(::world &world)
{
    if (world.blocks.empty()) return;
    const std::size_t width = 100ull;
    if (world.blocks.size() % width != 0) return;

    constexpr int SURFACE_Y = 25;
    constexpr int BEDROCK_Y = 54;
    bool changed = false;

    // Remove sky bedrock left by the old cord(x, y+1) precedence bug (y < surface).
    for (std::size_t i = 0; i < world.blocks.size(); ++i)
    {
        if (world.blocks[i].fg != 8) continue;
        const int y = static_cast<int>(i / width);
        if (y >= SURFACE_Y) continue;
        world.blocks[i].fg = 0;
        changed = true;
    }

    // Exactly one bedrock under each main door. Extra surface-row bedrock (from the
    // brief 3-wide pad mistake) becomes normal dirt again.
    for (std::size_t i = 0; i < world.blocks.size(); ++i)
    {
        if (world.blocks[i].fg != 6) continue;
        const std::size_t under = i + width;
        if (under >= world.blocks.size()) continue;
        if (world.blocks[under].fg != 8 || world.blocks[under].bg == 0)
        {
            world.blocks[under].fg = 8;
            if (world.blocks[under].bg == 0)
                world.blocks[under].bg = 14;
            changed = true;
        }
    }
    for (std::size_t i = 0; i < world.blocks.size(); ++i)
    {
        if (world.blocks[i].fg != 8) continue;
        const int y = static_cast<int>(i / width);
        if (y < SURFACE_Y || y >= BEDROCK_Y) continue;
        if (i < width) continue;
        if (world.blocks[i - width].fg == 6) continue; // pad under main door — keep
        world.blocks[i].fg = 2;
        if (world.blocks[i].bg == 0)
            world.blocks[i].bg = 14;
        changed = true;
    }
    if (changed) world.mark_dirty();
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
    blocks[cord(pos.x, (pos.y + 1))].bg = 14;
    world.mark_dirty();
    return true;
}

void blast::thermonuclear(::world &world, const std::string& name)
{
    ransuu ransuu;

    constexpr int WIDTH = 100;
    constexpr int HEIGHT = 60;
    constexpr int DOOR_Y = 24;
    constexpr int BEDROCK_Y = 54;

    const u_short main_door = static_cast<u_short>(ransuu[{2, WIDTH - 4}]);
    std::vector<::block> blocks(static_cast<std::size_t>(WIDTH * HEIGHT), ::block{0, 0});
    for (std::size_t i = 0ull; i < blocks.size(); ++i)
    {
        const int y = static_cast<int>(i / WIDTH);
        blocks[i].fg = (y >= BEDROCK_Y) ? 8 : 0;
    }
    {
        const std::size_t door_i = static_cast<std::size_t>(DOOR_Y) * static_cast<std::size_t>(WIDTH) + main_door;
        const std::size_t pad_i = door_i + static_cast<std::size_t>(WIDTH);
        blocks[door_i].fg = 6;
        blocks[pad_i].fg = 8;
    }
    world.blocks = std::move(blocks);
    world.name = std::move(name);
    world.objects.clear();
    world.last_object_uid = g_object_uid.load();
    ensure_main_door_bedrock(world);
    world.mark_dirty();
}
