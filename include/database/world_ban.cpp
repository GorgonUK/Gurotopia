#include "pch.hpp"

#include "world_ban.hpp"

using namespace std::chrono;

namespace
{
    struct ban
    {
        std::string world{};
        int uid{};
        steady_clock::time_point until{};
    };
    std::vector<::ban> bans{};
}

void world_ban_add(const std::string &world, int uid)
{
    std::erase_if(bans, [](const ::ban &b) { return steady_clock::now() >= b.until; }); // @note drop expired entries so the list stays small

    bans.emplace_back(world, uid, steady_clock::now() + minutes(10));
}

bool world_banned(const std::string &world, int uid)
{
    for (const ::ban &b : bans)
        if (b.world == world && b.uid == uid && steady_clock::now() < b.until)
            return true;

    return false;
}
