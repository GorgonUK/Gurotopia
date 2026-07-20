#include "pch.hpp"
#include "database.hpp"
#include "items.hpp"
#include "world.hpp"
#include "on/SetClothing.hpp"
#include "on/CountryState.hpp"
#include "on/SetBux.hpp"
#include "commands/punch.hpp"
#include "tools/string.hpp"

#include <cstring>

#include "peer.hpp"

using namespace std::chrono;

bool peer::exists(const std::string& growid)
{
    ::hStmt hStmt{ "SELECT 1 FROM peer WHERE growid = ? LIMIT 1" };

    MYSQL_BIND param = make_bind_in(growid);
    mysql_stmt_bind_param(hStmt.pStmt, &param);
    mysql_stmt_execute(hStmt.pStmt);

    return (!mysql_stmt_store_result(hStmt.pStmt) && mysql_stmt_num_rows(hStmt.pStmt) > 0);
}

template<typename T>
void peer::mysql_insert(const std::string& column, const T& value)
{
    ::hStmt hStmt{ std::format("INSERT INTO peer ({}) VALUES (?)", column).c_str() };

    MYSQL_BIND param = make_bind_in(value);
    mysql_stmt_bind_param(hStmt.pStmt, &param);
    mysql_stmt_execute(hStmt.pStmt);
}
template void peer::mysql_insert<signed>(const std::string&, const signed&);
template void peer::mysql_insert<unsigned>(const std::string&, const unsigned&);
template void peer::mysql_insert<float>(const std::string&, const float&);
template void peer::mysql_insert<std::string>(const std::string&, const std::string&);

template<typename T>
void peer::mysql_update(const std::string& column, const T& value)
{
    ::hStmt hStmt{ std::format("UPDATE peer SET {} = ? WHERE growid = ?", column).c_str() };

    MYSQL_BIND params[2] = {
        make_bind_in(value),       // SET
        make_bind_in(this->growid) // WHERE
    };
    mysql_stmt_bind_param(hStmt.pStmt, params);
    mysql_stmt_execute(hStmt.pStmt);
}
template void peer::mysql_update<signed>(const std::string&, const signed&);
template void peer::mysql_update<unsigned>(const std::string&, const unsigned&);
template void peer::mysql_update<float>(const std::string&, const float&);
template void peer::mysql_update<std::string>(const std::string&, const std::string&);

template<typename T>
T peer::mysql_select(const std::string &column, const std::string &arg)
{
    T value{};
    ::hStmt hStmt{ std::format("SELECT {}({}) FROM peer WHERE growid = ? LIMIT 1", arg, column).c_str() };

    MYSQL_BIND param = make_bind_in(this->growid);
    mysql_stmt_bind_param(hStmt.pStmt, &param);

    unsigned long length = 0;
    MYSQL_BIND result = make_bind_out(value);
    result.length = &length;
    mysql_stmt_bind_result(hStmt.pStmt, &result);

    mysql_stmt_execute(hStmt.pStmt);
    mysql_stmt_fetch(hStmt.pStmt);

    // strings bind into a fixed 1024-byte buffer; trim to the real column
    // length so values compare byte-for-byte (PBKDF2 hashes were failing
    // verify because the buffer stayed padded with NULs).
    if constexpr (std::is_same_v<T, std::string>)
        value.resize(length);

    return value;
}
/* since we will only select during mysql_select_all */ // @note add templates here if use select outside of this file.

void peer::mysql_select_all()
{
    this->user_id    = this->mysql_select<signed>("uid");
    this->growid     = this->mysql_select<std::string>("growid");
    this->password   = this->mysql_select<std::string>("password");
    this->created_at = this->mysql_select<std::time_t>("created_at", "UNIX_TIMESTAMP");
}

bool peer::mysql_load_progress()
{
    if (this->user_id <= 0)
        return false;

    ::hStmt hStmt{
        "SELECT gems, level, xp, slot_size, clothing, fav, role, skin_color, hair_color, "
        "country, fires_removed, gbc_pity, initialized, recent_worlds, my_worlds, achievements, quest, "
        "online_status, notebook, piggy_gems "
        "FROM peer_state WHERE uid = ? LIMIT 1"
    };

    MYSQL_BIND param = make_bind_in(this->user_id);
    if (mysql_stmt_bind_param(hStmt.pStmt, &param))
    {
        fprintf(stderr, "[peer load] bind param: %s\n", mysql_stmt_error(hStmt.pStmt));
        return false;
    }

    signed gems = 0;
    signed piggy_gems = 0;
    unsigned level = 1, xp = 0, slot_size = 16;
    unsigned role = 0, skin_color = 2527912447u, hair_color = 4278255615u;
    unsigned fires_removed = 0, gbc_pity = 0, initialized = 0;
    unsigned online_status = 0;
    std::string country;
    std::vector<u_char> clothing_blob(40, 0);
    std::vector<u_char> fav_blob;
    std::vector<u_char> recent_blob, my_blob;
    std::vector<u_char> ach_blob(64, 0);
    std::vector<u_char> quest_blob(16, 0);
    std::vector<u_char> notebook_blob;
    unsigned long clothing_len = 0, fav_len = 0, country_len = 0, recent_len = 0, my_len = 0, ach_len = 0, quest_len = 0, notebook_len = 0;

    fav_blob.resize(512);
    notebook_blob.resize(2048);

    MYSQL_BIND results[20]{};
    results[0]  = make_bind_out(gems);
    results[1]  = make_bind_out(level);
    results[2]  = make_bind_out(xp);
    results[3]  = make_bind_out(slot_size);
    results[4]  = make_bind_out_blob(clothing_blob, clothing_len);
    results[5]  = make_bind_out_blob(fav_blob, fav_len);
    results[6]  = make_bind_out(role);
    results[7]  = make_bind_out(skin_color);
    results[8]  = make_bind_out(hair_color);
    results[9]  = make_bind_out(country);
    results[9].length = &country_len;
    results[10] = make_bind_out(fires_removed);
    results[11] = make_bind_out(gbc_pity);
    results[12] = make_bind_out(initialized);
    results[13] = make_bind_out_blob(recent_blob, recent_len);
    results[14] = make_bind_out_blob(my_blob, my_len);
    results[15] = make_bind_out_blob(ach_blob, ach_len);
    results[16] = make_bind_out_blob(quest_blob, quest_len);
    results[17] = make_bind_out(online_status);
    results[18] = make_bind_out_blob(notebook_blob, notebook_len);
    results[19] = make_bind_out(piggy_gems);

    if (mysql_stmt_bind_result(hStmt.pStmt, results))
    {
        fprintf(stderr, "[peer load] bind result: %s\n", mysql_stmt_error(hStmt.pStmt));
        return false;
    }

    if (mysql_stmt_execute(hStmt.pStmt) || mysql_stmt_store_result(hStmt.pStmt))
    {
        fprintf(stderr, "[peer load] exec: %s\n", mysql_stmt_error(hStmt.pStmt));
        return false;
    }

    if (mysql_stmt_num_rows(hStmt.pStmt) == 0)
    {
        // brand-new account row: no progression yet
        this->inventory_initialized = false;
        this->dirty = false;
        return true;
    }

    int fetch = mysql_stmt_fetch(hStmt.pStmt);
    if (fetch != 0 && fetch != MYSQL_DATA_TRUNCATED)
    {
        fprintf(stderr, "[peer load] fetch: %s\n", mysql_stmt_error(hStmt.pStmt));
        return false;
    }

    country.resize(country_len);
    trim_blob(clothing_blob, clothing_len);
    trim_blob(fav_blob, fav_len);
    trim_blob(recent_blob, recent_len);
    trim_blob(my_blob, my_len);
    trim_blob(ach_blob, ach_len);
    trim_blob(quest_blob, quest_len);
    trim_blob(notebook_blob, notebook_len);

    this->gems = gems;
    this->piggy_gems = std::clamp(piggy_gems, 0, PIGGY_CAP);
    this->level = { static_cast<u_short>(level), static_cast<u_short>(xp) };
    this->slot_size = static_cast<short>(slot_size);
    this->role = static_cast<u_char>(role);
    this->skin_color = skin_color;
    this->hair_color = hair_color;
    this->country = country;
    this->fires_removed = static_cast<u_short>(fires_removed);
    this->gbc_pity = static_cast<u_short>(gbc_pity);
    this->inventory_initialized = (initialized != 0);
    this->online_status = static_cast<u_char>(online_status <= 2 ? online_status : 0);

    this->clothing.fill(0.0f);
    if (clothing_blob.size() >= sizeof(float) * 10ull)
        std::memcpy(this->clothing.data(), clothing_blob.data(), sizeof(float) * 10ull);

    this->fav.clear();
    for (std::size_t i = 0; i + 1 < fav_blob.size(); i += sizeof(short))
    {
        short id = 0;
        std::memcpy(&id, fav_blob.data() + i, sizeof(short));
        this->fav.push_back(id);
    }

    this->ach_progress.fill(0);
    for (std::size_t i = 0; i < this->ach_progress.size() && (i + 1) * sizeof(u_int) <= ach_blob.size(); ++i)
        std::memcpy(&this->ach_progress[i], ach_blob.data() + i * sizeof(u_int), sizeof(u_int));

    this->quest = ::Quest{};
    if (quest_blob.size() >= sizeof(u_int) * 4ull)
    {
        u_int packed[4]{};
        std::memcpy(packed, quest_blob.data(), sizeof(packed));
        this->quest = ::Quest{ packed[0], packed[1], packed[2], packed[3] };
    }

    this->notebook_pages.fill(std::string{});
    {
        std::size_t offset = 0;
        for (std::size_t i = 0; i < this->notebook_pages.size() && offset + sizeof(u_short) <= notebook_blob.size(); ++i)
        {
            u_short len = 0;
            std::memcpy(&len, notebook_blob.data() + offset, sizeof(u_short));
            offset += sizeof(u_short);
            if (offset + len > notebook_blob.size()) break;
            this->notebook_pages[i].assign(
                reinterpret_cast<const char*>(notebook_blob.data() + offset),
                reinterpret_cast<const char*>(notebook_blob.data() + offset + len)
            );
            offset += len;
        }
    }

    // '\n'-joined world names -> fill the fixed array from the back so that
    // .back() stays the most-recent entry (matches join_request/tile_change).
    auto load_world_list = [](const std::vector<u_char> &blob, auto &dest)
    {
        dest.fill(std::string{});
        if (blob.empty())
            return;

        std::vector<std::string> names;
        std::string text(blob.begin(), blob.end());
        for (std::size_t start = 0; start <= text.size(); )
        {
            std::size_t nl = text.find('\n', start);
            if (nl == std::string::npos) nl = text.size();
            if (nl > start) names.emplace_back(text.substr(start, nl - start));
            start = nl + 1;
        }

        std::size_t count = std::min(names.size(), dest.size());
        for (std::size_t i = 0; i < count; ++i)
            dest[dest.size() - count + i] = names[names.size() - count + i];
    };
    load_world_list(recent_blob, this->recent_worlds);
    load_world_list(my_blob, this->my_worlds);

    // inventory rows
    this->slots.clear();
    ::hStmt invStmt{ "SELECT item_id, count FROM peer_inventory WHERE uid = ?" };
    MYSQL_BIND inv_param = make_bind_in(this->user_id);
    mysql_stmt_bind_param(invStmt.pStmt, &inv_param);

    signed item_id_s = 0, count_s = 0;
    MYSQL_BIND inv_results[2] = {
        make_bind_out(item_id_s),
        make_bind_out(count_s)
    };
    mysql_stmt_bind_result(invStmt.pStmt, inv_results);

    if (mysql_stmt_execute(invStmt.pStmt) || mysql_stmt_store_result(invStmt.pStmt))
    {
        fprintf(stderr, "[peer load] inv: %s\n", mysql_stmt_error(invStmt.pStmt));
        return false;
    }

    while (mysql_stmt_fetch(invStmt.pStmt) == 0)
        this->slots.emplace_back(static_cast<short>(item_id_s), static_cast<short>(count_s));

    this->update_effects();
    this->dirty = false;
    return true;
}

bool peer::mysql_save_progress()
{
    if (this->user_id <= 0 || this->growid.empty())
        return false;

    if (!mysql_begin())
        return false;

    std::vector<u_char> clothing_blob(sizeof(float) * 10ull, 0);
    std::memcpy(clothing_blob.data(), this->clothing.data(), clothing_blob.size());

    std::vector<u_char> fav_blob(this->fav.size() * sizeof(short), 0);
    if (!this->fav.empty())
        std::memcpy(fav_blob.data(), this->fav.data(), fav_blob.size());

    auto join_world_list = [](const auto &src)
    {
        std::string text;
        for (const std::string &name : src)
            if (!name.empty())
            {
                if (!text.empty()) text.push_back('\n');
                text.append(name);
            }
        return std::vector<u_char>(text.begin(), text.end());
    };
    std::vector<u_char> recent_blob = join_world_list(this->recent_worlds);
    std::vector<u_char> my_blob = join_world_list(this->my_worlds);

    std::vector<u_char> ach_blob(this->ach_progress.size() * sizeof(u_int), 0);
    std::memcpy(ach_blob.data(), this->ach_progress.data(), ach_blob.size());

    const u_int quest_packed[4]{ this->quest.goal, this->quest.progress, this->quest.target, this->quest.reward_gems };
    std::vector<u_char> quest_blob(sizeof(quest_packed), 0);
    std::memcpy(quest_blob.data(), quest_packed, quest_blob.size());

    std::vector<u_char> notebook_blob;
    for (const std::string &page : this->notebook_pages)
    {
        const u_short len = static_cast<u_short>(std::min<std::size_t>(page.size(), 256ull));
        const std::size_t at = notebook_blob.size();
        notebook_blob.resize(at + sizeof(u_short) + len);
        std::memcpy(notebook_blob.data() + at, &len, sizeof(u_short));
        if (len != 0)
            std::memcpy(notebook_blob.data() + at + sizeof(u_short), page.data(), len);
    }

    signed gems = this->gems;
    signed piggy_gems = this->piggy_gems;
    unsigned level = this->level.front();
    unsigned xp = this->level.back();
    signed slot_size = this->slot_size;
    unsigned role = this->role;
    unsigned skin_color = this->skin_color;
    unsigned hair_color = this->hair_color;
    unsigned fires_removed = this->fires_removed;
    unsigned gbc_pity = this->gbc_pity;
    unsigned initialized = this->inventory_initialized ? 1u : 0u;
    unsigned online_status = this->online_status;

    ::hStmt upsert{
        "INSERT INTO peer_state "
        "(uid, gems, level, xp, slot_size, clothing, fav, role, skin_color, hair_color, "
        "country, fires_removed, gbc_pity, initialized, recent_worlds, my_worlds, achievements, quest, "
        "online_status, notebook, piggy_gems) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON DUPLICATE KEY UPDATE "
        // @note role deliberately NOT updated: the DB is authoritative for it, so a
        // manual `UPDATE peer_state SET role=1` can't be clobbered by a session save.
        "gems=VALUES(gems), level=VALUES(level), xp=VALUES(xp), slot_size=VALUES(slot_size), "
        "clothing=VALUES(clothing), fav=VALUES(fav), "
        "skin_color=VALUES(skin_color), hair_color=VALUES(hair_color), country=VALUES(country), "
        "fires_removed=VALUES(fires_removed), gbc_pity=VALUES(gbc_pity), initialized=VALUES(initialized), "
        "recent_worlds=VALUES(recent_worlds), my_worlds=VALUES(my_worlds), achievements=VALUES(achievements), "
        "quest=VALUES(quest), online_status=VALUES(online_status), notebook=VALUES(notebook), "
        "piggy_gems=VALUES(piggy_gems)"
    };

    MYSQL_BIND params[21] = {
        make_bind_in(this->user_id),
        make_bind_in(gems),
        make_bind_in(level),
        make_bind_in(xp),
        make_bind_in(slot_size),
        make_bind_in_blob(clothing_blob),
        make_bind_in_blob(fav_blob),
        make_bind_in(role),
        make_bind_in(skin_color),
        make_bind_in(hair_color),
        make_bind_in(this->country),
        make_bind_in(fires_removed),
        make_bind_in(gbc_pity),
        make_bind_in(initialized),
        make_bind_in_blob(recent_blob),
        make_bind_in_blob(my_blob),
        make_bind_in_blob(ach_blob),
        make_bind_in_blob(quest_blob),
        make_bind_in(online_status),
        make_bind_in_blob(notebook_blob),
        make_bind_in(piggy_gems)
    };

    if (mysql_stmt_bind_param(upsert.pStmt, params) || mysql_stmt_execute(upsert.pStmt))
    {
        fprintf(stderr, "[peer save] state: %s\n", mysql_stmt_error(upsert.pStmt));
        mysql_rollback();
        return false;
    }

    {
        ::hStmt del{ "DELETE FROM peer_inventory WHERE uid = ?" };
        MYSQL_BIND del_param = make_bind_in(this->user_id);
        if (mysql_stmt_bind_param(del.pStmt, &del_param) || mysql_stmt_execute(del.pStmt))
        {
            fprintf(stderr, "[peer save] del inv: %s\n", mysql_stmt_error(del.pStmt));
            mysql_rollback();
            return false;
        }
    }

    for (const ::slot &slot : this->slots)
    {
        if (slot.count <= 0)
            continue;

        signed item_id = slot.id;
        signed count = slot.count;
        ::hStmt ins{ "INSERT INTO peer_inventory (uid, item_id, count) VALUES (?, ?, ?)" };
        MYSQL_BIND ins_params[3] = {
            make_bind_in(this->user_id),
            make_bind_in(item_id),
            make_bind_in(count)
        };
        if (mysql_stmt_bind_param(ins.pStmt, ins_params) || mysql_stmt_execute(ins.pStmt))
        {
            fprintf(stderr, "[peer save] inv: %s\n", mysql_stmt_error(ins.pStmt));
            mysql_rollback();
            return false;
        }
    }

    if (!mysql_commit())
        return false;

    this->dirty = false;
    return true;
}

void autosave_peers()
{
    peers("", peer_condition::PEER_ALL, [](ENetPeer &p)
    {
        ::peer *pPeer = static_cast<::peer*>(p.data);
        if (pPeer && pPeer->dirty)
            pPeer->mysql_save_progress();
    });
}

void save_all_peers()
{
    peers("", peer_condition::PEER_ALL, [](ENetPeer &p)
    {
        ::peer *pPeer = static_cast<::peer*>(p.data);
        if (pPeer)
            pPeer->mysql_save_progress();
    });
}

u_short peer::emplace(::slot slot) 
{
    this->dirty = true;
    if (auto it = std::ranges::find(this->slots, slot.id, &::slot::id); it != this->slots.end()) 
    {
        const u_short excess = static_cast<u_short>(std::max(0, (it->count + slot.count) - 200));
        it->count = std::min(it->count + slot.count, 200);
        if (it->count == 0)
        {
            const ::item &item = id_to_item(it->id);
            if (item.cloth_type != clothing::none) this->clothing[item.cloth_type] = 0;
            this->slots.erase(it); // @note free the backpack slot once the stack is gone
        }
        return excess;
    }
    // New stack: refuse if backpack has no free slot (keeps client/server inventory in sync).
    if (slot.count > 0 && static_cast<short>(this->slots.size()) >= this->slot_size)
        return static_cast<u_short>(slot.count);
    if (slot.count != 0)
        this->slots.emplace_back(std::move(slot));
    return 0;
}

void peer::credit_gems(ENetEvent &event, int amount)
{
    if (amount <= 0) return;

    this->gems = std::clamp(this->gems + amount, 0, std::numeric_limits<signed>::max());
    this->piggy_gems = std::min(this->piggy_gems + amount, PIGGY_CAP);
    this->mark_dirty();
    on::SetBux(event);
}

void peer::add_xp(ENetEvent &event, u_short value) 
{
    this->dirty = true;
    u_short &lvl = this->level.front();
    u_short &xp = this->level.back() += value; // @note factor the new xp amount

    for (; lvl < 125; )
    {
        u_short xp_formula = 50 * (lvl * lvl + 2); // @author https://www.growtopiagame.com/forums/member/553046-kasete
        if (xp < xp_formula) break;

        xp -= xp_formula;
        lvl++;

        if (lvl == 50) 
        {
            modify_item_inventory(event, ::slot{1400, 1}); // @note Mini Growtopian
            /* @todo based on account age give peer other items... */
        }
        if (lvl == 125) on::CountryState(event);

        send_particle_effect(event, this->pos, {0x00, 0x2e}); // @todo make particle effect smaller like growtopia
        send_varlist(event.peer, { 
            "OnTalkBubble", 
            this->netid,
            std::format("`{}{}`` is now level {}!", this->prefix, this->growid, lvl) 
        });
    }
}

void peer::update_effects()
{
    this->punch_effect = 0;
    for (float cloth : this->clothing)
    {
        u_char punch_id = get_punch_id((u_int)cloth);
        if (punch_id != 0) // @note an actual change rather than no effect.
            this->punch_effect = punch_id;
    }
}

ENetHost *host;

std::vector<ENetPeer*> peers(const std::string &world, peer_condition condition, std::function<void(ENetPeer&)> fun)
{
    std::vector<ENetPeer*> _peers{};
    _peers.reserve(host->peerCount);

    for (ENetPeer &peer : std::span(host->peers, host->peerCount))
        if (peer.state == ENET_PEER_STATE_CONNECTED)
        {
            // @note a peer can be CONNECTED at the ENet layer while its queued
            // CONNECT event is still pending, so peer.data is null until _connect
            // runs. Skip it to avoid dereferencing null during broadcasts.
            ::peer *pOthers = static_cast<::peer*>(peer.data);
            if (pOthers == nullptr) continue;

            if (condition == peer_condition::PEER_SAME_WORLD)
            {
                if (pOthers->netid == 0 || (pOthers->recent_worlds.back() != world)) continue;
            }
            fun(peer);
            _peers.push_back(&peer);
        }

    return _peers;
}

void safe_disconnect_peers(int code)
{
    puts("killing gurotopia...");

    save_all_peers();
    save_all_worlds();

    peers("", peer_condition::PEER_ALL, [](ENetPeer &p) { enet_peer_disconnect(&p, 0); });
    enet_host_flush(host);
    enet_host_destroy(host);
    host = nullptr; // @todo clean this up better
    enet_deinitialize();

    puts("killed gurotopia safely!");
}

state get_state(const std::vector<u_char> &&packet) 
{
    const int     *i32   = reinterpret_cast<const int*>(packet.data());
    const u_int *u_i32 = reinterpret_cast<const u_int*>(packet.data());
    const float   *f_i32 = reinterpret_cast<const float*>(packet.data());

    return state{
        .type = i32[1],
        .netid = i32[2],
        .uid = i32[3],
        .peer_state = i32[4],
        .count = f_i32[5],
        .id = i32[6],
        .pos = ::pos{f_i32[7], f_i32[8]},
        .speed = ::pos{f_i32[9], f_i32[10]},
        .idk = f_i32[11],
        .punch = ::pos{i32[12], i32[13]},
        .size = u_i32[14]
    };
}

std::vector<u_char> compress_state(const state &state) 
{
    std::vector<u_char> data(sizeof(::state), 0x00);
    int   *i32   = reinterpret_cast<int*>(data.data());
    u_int *u_i32 = reinterpret_cast<u_int*>(data.data());
    float *f_i32 = reinterpret_cast<float*>(data.data());

    i32[0] = state.packet_create;
    i32[1] = state.type;
    i32[2] = state.netid;
    i32[3] = state.uid;
    i32[4] = state.peer_state;
    f_i32[5] = state.count;
    i32[6] = state.id;
    f_i32[7] = state.pos.x;
    f_i32[8] = state.pos.y;
    f_i32[9] = state.speed.x;
    f_i32[10] = state.speed.y;
    f_i32[11] = state.idk;
    i32[12] = state.punch.x;
    i32[13] = state.punch.y;
    u_i32[14] = state.size;
    return data;
}

void send_inventory_state(ENetEvent &event)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    std::vector<u_char> data = compress_state(::state{
        .type = 0x09, // @note PACKET_SEND_INVENTORY_STATE
        .netid = pPeer->netid,
        .peer_state = peer_state::S_EXTENDED
    });

    // @note inventory payload begins at byte 58 (overlaps the last 2 bytes of the
    // 60-byte tank header). Needs: 2 ints (slot_size, count) + N slot ints.
    // The old `+5` left the buffer 1 byte short and corrupted the heap on free.
    const std::size_t size = pPeer->slots.size();
    data.resize(58ull + sizeof(int) * (2ull + size));

    int *i32 = reinterpret_cast<int*>(&data[58ull]);

#ifdef _WIN32
    *i32++ = _byteswap_ulong(pPeer->slot_size);
    *i32++ = _byteswap_ulong(static_cast<u_int>(size));
#else // @note linux
    *i32++ = __builtin_bswap32(pPeer->slot_size);
    *i32++ = __builtin_bswap32(static_cast<u_int>(size));
#endif
    for (const ::slot &slot : pPeer->slots)
        *i32++ = slot.id | (slot.count & 0xff) << 16;

	enet_peer_send(event.peer, 0, enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE));
}
