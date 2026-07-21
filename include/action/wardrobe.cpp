#include "pch.hpp"
#include "on/SetClothing.hpp"
#include "on/WardrobePresetUpdate.hpp"

#include "wardrobe.hpp"

void action::wardrobe_preset_load(ENetEvent &event, const std::string &header)
{
    (void)header;
    on::WardrobePresetUpdate(event);
}

void action::wardrobe_save_ui(ENetEvent &event, const std::string &header)
{
    ::hPipe pipe{ header };
    if (pipe["preset"] != "one")
        return;

    ::peer &pPeer = peer_of(event);
    for (std::size_t i = 0; i < pPeer.wardrobe_preset_one.size(); ++i)
        pPeer.wardrobe_preset_one[i] = static_cast<short>(pPeer.clothing[i]);
    pPeer.mark_dirty();
}

void action::wardrobe_unequip_all(ENetEvent &event, const std::string &header)
{
    (void)header;
    ::peer &pPeer = peer_of(event);
    pPeer.clothing.fill(0.0f);
    pPeer.update_effects();
    pPeer.mark_dirty();
    on::SetClothing(*event.peer);
}

void action::wardrobe_load_ui(ENetEvent &event, const std::string &header)
{
    ::hPipe pipe{ header };
    if (pipe["preset"] != "one")
        return;

    ::peer &pPeer = peer_of(event);
    for (std::size_t i = 0; i < pPeer.wardrobe_preset_one.size(); ++i)
    {
        const short id = pPeer.wardrobe_preset_one[i];
        pPeer.clothing[i] = static_cast<float>(id);
        if (id != 0)
            send_varlist(event.peer, { "OnEquipNewItem", signed{ id } }, pPeer.netid);
    }
    pPeer.update_effects();
    pPeer.mark_dirty();
    on::SetClothing(*event.peer);
}
