#include "pch.hpp"

#include "protocol.hpp"
#include "tankIDName.hpp"
#include "refresh_item_data.hpp"
#include "enter_game.hpp"

#include "dialog_return.hpp"
#include "friends.hpp"

#include "join_request.hpp"
#include "quit_to_exit.hpp"
#include "respawn.hpp"
#include "setSkin.hpp"
#include "input.hpp"
#include "drop.hpp"
#include "info.hpp"
#include "trash.hpp"
#include "wrench.hpp"
#include "itemfavourite.hpp"
#include "inventoryfavuitrigger.hpp"
#include "store.hpp"
#include "storenavigate.hpp"
#include "buy.hpp"
#include "openPiggyBank.hpp"
#include "wardrobe.hpp"

#include "quit.hpp"

#include "__action.hpp"

std::unordered_map<std::string, std::function<void(ENetEvent&, const std::string&)>> action_pool
{
    {"protocol", &action::protocol},
    {"tankIDName", &action::tankIDName},
    {"action|refresh_item_data", &action::refresh_item_data},
    {"action|enter_game", &action::enter_game},

    {"action|dialog_return", &action::dialog_return},
    {"action|friends", &action::friends},

    {"action|join_request", [](ENetEvent& e, const std::string& h){ action::join_request(e, h, ""); }},
    {"action|quit_to_exit", [](ENetEvent& e, const std::string& h){ action::quit_to_exit(e, h, false); }},
    {"action|respawn", &action::respawn},
    {"action|respawn_spike", &action::respawn},
    {"action|setSkin", &action::setSkin},
    {"action|input", &action::input},
    {"action|drop", &action::drop},
    {"action|info", &action::info},
    {"action|trash", &action::trash},
    {"action|wrench", &action::wrench},
    {"action|itemfavourite", &action::itemfavourite},
    {"action|inventoryfavuitrigger", &action::inventoryfavuitrigger},
    {"action|store", &action::store},
    {"action|storenavigate", &action::storenavigate},
    {"action|buy", [](ENetEvent& e, const std::string& h){ action::buy(e, h, ""); }},
    {"action|openPiggyBank", &action::openPiggyBank},
    {"action|wardrobe_preset_load", &action::wardrobe_preset_load},
    {"action|wardrobe_save_ui", &action::wardrobe_save_ui},
    {"action|wardrobe_unequip_all", &action::wardrobe_unequip_all},
    {"action|wardrobe_load_ui", &action::wardrobe_load_ui},

    {"action|quit", &action::quit}
};