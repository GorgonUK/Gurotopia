#pragma once

enum quest_goal : u_char
{
    QUEST_BREAK_BLOCKS,
    QUEST_PLACE_BLOCKS,
    QUEST_HARVEST_TREES,
    QUEST_CATCH_FISH,
    QUEST_GOAL_COUNT
};

struct quest_type
{
    std::string_view verb{}; // @note e.g. "Break" -> "Break 120 blocks"
    std::string_view noun{};
    u_int min{}, max{}; // @note target roll range
    u_int gems_each{}; // @note reward = target * gems_each
};
extern const std::array<::quest_type, QUEST_GOAL_COUNT> quest_types;

/* "Break 120 blocks" for the peer's active quest */
extern std::string quest_describe(const ::Quest &quest);

/* add progress towards the peer's active quest (no-op when goal doesn't match) */
extern void quest_progress(ENetEvent &event, ::quest_goal goal, u_int add = 1);

/* the /quest dialog */
extern void quest_dialog(ENetEvent &event);
