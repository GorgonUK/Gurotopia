#include "pch.hpp"

#include <cstring>

#include "ip_tracker.hpp"

using namespace std::chrono;

namespace
{
    constexpr int MAX_FAILS = 5;

    struct track
    {
        u_int ip{};
        u_char fails{};
        steady_clock::time_point first_fail{};
        steady_clock::time_point blocked_until{};
    };
    std::vector<::track> tracks{};

    u_int to_ip(const ENetAddress &address)
    {
        u_int ip{};
        std::memcpy(&ip, address.host.v4, sizeof(ip));
        return ip;
    }

    ::track *find_track(u_int ip)
    {
        auto it = std::ranges::find(tracks, ip, &::track::ip);
        return (it != tracks.end()) ? &*it : nullptr;
    }
}

bool ip_login_blocked(const ENetAddress &address)
{
    ::track *track = find_track(to_ip(address));
    return track != nullptr && steady_clock::now() < track->blocked_until;
}

void ip_login_failed(const ENetAddress &address)
{
    std::erase_if(tracks, [](const ::track &t) // @note drop stale entries so the list stays small
    {
        return steady_clock::now() >= t.blocked_until && 
               steady_clock::now() - t.first_fail > minutes(10);
    });

    const u_int ip = to_ip(address);
    ::track *track = find_track(ip);
    if (track == nullptr)
    {
        tracks.emplace_back(ip, 1, steady_clock::now());
        return;
    }

    if (steady_clock::now() - track->first_fail > minutes(10)) // @note window expired, restart the count
    {
        track->fails = 1;
        track->first_fail = steady_clock::now();
        return;
    }

    if (++track->fails >= MAX_FAILS)
        track->blocked_until = steady_clock::now() + minutes(15);
}

void ip_login_succeeded(const ENetAddress &address)
{
    std::erase_if(tracks, [ip = to_ip(address)](const ::track &t) { return t.ip == ip; });
}
