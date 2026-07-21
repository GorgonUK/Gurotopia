#include "pch.hpp"
#include "WardrobePresetUpdate.hpp"

namespace
{
    std::string preset_csv(const std::array<short, 10ull> &slots)
    {
        std::string out;
        for (std::size_t i = 0; i < slots.size(); ++i)
        {
            if (i) out.push_back(',');
            out.append(std::to_string(slots[i]));
        }
        return out;
    }
}

void on::WardrobePresetUpdate(ENetEvent &event)
{
    ::peer &pPeer = peer_of(event);
    send_varlist(event.peer, {
        "OnWardrobePresetUpdate",
        preset_csv(pPeer.wardrobe_preset_one),
        "0,0,0,0,0,0,0,0,0,0"
    });
}
