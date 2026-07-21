#pragma once

namespace action
{
    extern void wardrobe_preset_load(ENetEvent &event, const std::string &header);
    extern void wardrobe_save_ui(ENetEvent &event, const std::string &header);
    extern void wardrobe_unequip_all(ENetEvent &event, const std::string &header);
    extern void wardrobe_load_ui(ENetEvent &event, const std::string &header);
}
