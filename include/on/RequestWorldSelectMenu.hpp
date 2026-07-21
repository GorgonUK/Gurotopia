#pragma once

namespace on
{
    extern void RequestWorldSelectMenu(ENetEvent& event);
    extern void RequestWorldCategoryMenu(ENetEvent &event);
    extern void RequestWorldCategory(ENetEvent &event, u_char category);
}
