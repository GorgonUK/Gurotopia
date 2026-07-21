#include "pch.hpp"

#include <algorithm>
#include <deque>
#include <unordered_set>

#include "world.hpp"

/*
    Tile-lock rules (Small/Big/Huge/Builder locks that claim a flood-filled area).
    Split out of world.cpp. Declarations live in world.hpp.
*/

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
