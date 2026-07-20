#pragma once

namespace on
{
    extern void SetBux(ENetEvent& event);
    /* refresh the HUD Piggy Bank button label (e.g. "10K/1.5M") */
    extern void sync_piggy_bank(ENetEvent &event);
    extern std::string format_gems_commas(int amount);
}
