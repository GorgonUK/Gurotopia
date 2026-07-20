#pragma once

enum ach : u_char
{
    ACH_BLOCKS_BROKEN,
    ACH_BLOCKS_PLACED,
    ACH_TREES_HARVESTED,
    ACH_WORLDS_VISITED,
    ACH_FISH_CAUGHT,
    ACH_SURGERIES_DONE,
    ACH_QUESTS_DONE,
    ACH_COUNT
};
static_assert(ACH_COUNT <= 8ull, "peer::ach_progress only holds 8 counters");

struct achievement
{
    std::string_view name{};
    std::string_view description{};
    u_int goal{};
    u_short reward_gems{};
    u_short icon{}; // badge icon for earned add_achieve rows
};
extern const std::array<::achievement, ACH_COUNT> achievements;

/* add progress; announces & pays the gem reward when the goal is reached */
extern void achievement_progress(ENetEvent &event, ::ach ach, u_int add = 1);

/* number of completed achievements */
extern u_char achievements_completed(const ::peer &peer);
