#include "pch.hpp"
#include "database/quests.hpp"
#include "quest.hpp"

void quest(ENetEvent& event, const std::string_view text)
{
    quest_dialog(event);
}
