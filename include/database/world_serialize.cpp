#include "pch.hpp"
#include "database.hpp"

#include <cstring>
#include <stdexcept>

#include "world.hpp"

/*
    World blob (de)serialization + MySQL persistence.

    Split out of world.cpp: this is the binary on-disk format (ByteWriter/ByteReader,
    encode_world/decode_world) and the `world` table load/save/autosave that use it.
    Bump WORLD_VERSION and append a new versioned block when the format grows.
*/
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
        unsigned category = world.category;
        long long total_visits = world.total_visits;
        float weather_x = world.weather.x;
        float weather_y = world.weather.y;
        unsigned last_uid = world.last_object_uid;

        ::hStmt stmt{
            "INSERT INTO world "
            "(name, owner, is_public, lock_state, minimum_entry_level, category, total_visits, "
            "weather_x, weather_y, last_object_uid, data) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON DUPLICATE KEY UPDATE "
            "owner=VALUES(owner), is_public=VALUES(is_public), lock_state=VALUES(lock_state), "
            "minimum_entry_level=VALUES(minimum_entry_level), weather_x=VALUES(weather_x), "
            "category=VALUES(category), "
            "weather_y=VALUES(weather_y), last_object_uid=VALUES(last_object_uid), data=VALUES(data)"
            // total_visits is owned by record_world_visit()'s atomic UPDATE — do not clobber it.
        };

        MYSQL_BIND params[11] = {
            make_bind_in(world.name),
            make_bind_in(owner),
            make_bind_in(is_public),
            make_bind_in(lock_state),
            make_bind_in(min_level),
            make_bind_in(category),
            make_bind_in(total_visits),
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
        "SELECT owner, is_public, lock_state, minimum_entry_level, category, total_visits, weather_x, weather_y, "
        "last_object_uid, data FROM world WHERE name = ? LIMIT 1"
    };

    MYSQL_BIND param = make_bind_in(name);
    if (mysql_stmt_bind_param(stmt.pStmt, &param))
    {
        fprintf(stderr, "[world load] bind: %s\n", mysql_stmt_error(stmt.pStmt));
        return false;
    }

    signed owner = 0;
    unsigned is_public = 0, lock_state = 0, min_level = 1, category = 0, last_uid = 0;
    long long total_visits = 0;
    float weather_x = 0, weather_y = 0;
    std::vector<u_char> blob;
    unsigned long blob_len = 0;
    blob.resize(2u << 20); // 2 MiB

    MYSQL_BIND results[10] = {
        make_bind_out(owner),
        make_bind_out(is_public),
        make_bind_out(lock_state),
        make_bind_out(min_level),
        make_bind_out(category),
        make_bind_out(total_visits),
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
        world.category = static_cast<u_char>(std::min(category, 15u));
        world.total_visits = std::max(0ll, total_visits);
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

void record_world_visit(::world &world)
{
    if (world.name.empty())
        return;

    auto bump = [&]() -> bool {
        ::hStmt stmt{"UPDATE world SET total_visits = total_visits + 1 WHERE name = ?"};
        MYSQL_BIND param = make_bind_in(world.name);
        if (mysql_stmt_bind_param(stmt.pStmt, &param) || mysql_stmt_execute(stmt.pStmt))
        {
            fprintf(stderr, "[world visit] %s: %s\n", world.name.c_str(), mysql_stmt_error(stmt.pStmt));
            return false;
        }
        return mysql_stmt_affected_rows(stmt.pStmt) > 0;
    };

    if (!bump())
    {
        // New worlds may not have a DB row until the first save.
        if (!world_save(world) || !bump())
            return;
    }

    ++world.total_visits;
}

void save_all_worlds()
{
    for (::world &w : worlds)
    {
        if (world_save(w))
            w.dirty = false;
    }
}
