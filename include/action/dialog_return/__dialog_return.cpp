#include "pch.hpp"

#include "popup.hpp"
#include "drop_item.hpp"
#include "trash_item.hpp"
#include "find_item.hpp"
#include "gateway_edit.hpp"
#include "billboard_edit.hpp"
#include "lock_edit.hpp"
#include "create_blast.hpp"
#include "socialportal.hpp"
#include "megaphone.hpp"
#include "letter_box.hpp"
#include "quest_menu.hpp"
#include "surgery.hpp"
#include "display_edit.hpp"
#include "vending.hpp"
#include "magplant.hpp"
#include "trade.hpp"
#include "combiner.hpp"
#include "set_online_status.hpp"
#include "paginated_personal_notebook.hpp"

#include "__dialog_return.hpp"

std::unordered_map<std::string, std::function<void(ENetEvent &, const ::hPipe &)>> dialog_return_pool
{
    {"popup", std::bind(&popup, std::placeholders::_1, std::placeholders::_2)},

    {"drop_item", std::bind(&drop_item, std::placeholders::_1, std::placeholders::_2)},
    {"trash_item", std::bind(&trash_item, std::placeholders::_1, std::placeholders::_2)},
    {"find_item", std::bind(&find_item, std::placeholders::_1, std::placeholders::_2)},

    {"gateway_edit", std::bind(&gateway_edit, std::placeholders::_1, std::placeholders::_2)},
    {"door_edit", std::bind(&gateway_edit, std::placeholders::_1, std::placeholders::_2)},
    {"sign_edit", std::bind(&gateway_edit, std::placeholders::_1, std::placeholders::_2)},

    {"billboard_edit", std::bind(&billboard_edit, std::placeholders::_1, std::placeholders::_2)},
    {"lock_edit", std::bind(&lock_edit, std::placeholders::_1, std::placeholders::_2)},
    {"tile_lock_edit", std::bind(&tile_lock_edit, std::placeholders::_1, std::placeholders::_2)},

    {"create_blast", std::bind(&create_blast, std::placeholders::_1, std::placeholders::_2)},
    {"socialportal", std::bind(&socialportal, std::placeholders::_1, std::placeholders::_2)},
    {"megaphone", std::bind(&megaphone, std::placeholders::_1, std::placeholders::_2)},

    {"mailbox_edit", std::bind(&letter_box, std::placeholders::_1, std::placeholders::_2)},
    {"bulletin_edit", std::bind(&letter_box, std::placeholders::_1, std::placeholders::_2)},
    {"donation_edit", std::bind(&letter_box, std::placeholders::_1, std::placeholders::_2)},

    {"quest_menu", std::bind(&quest_menu, std::placeholders::_1, std::placeholders::_2)},
    {"goalslist", std::bind(&quest_menu, std::placeholders::_1, std::placeholders::_2)},
    {"surgery", std::bind(&surgery, std::placeholders::_1, std::placeholders::_2)},

    {"display_edit", std::bind(&display_edit, std::placeholders::_1, std::placeholders::_2)},
    {"vending", std::bind(&vending_edit, std::placeholders::_1, std::placeholders::_2)},
    {"magplant_edit", std::bind(&magplant_edit, std::placeholders::_1, std::placeholders::_2)},

    {"trade_edit", std::bind(&trade_edit, std::placeholders::_1, std::placeholders::_2)},
    {"combiner_edit", std::bind(&combiner_edit, std::placeholders::_1, std::placeholders::_2)},

    {"set_online_status", std::bind(&set_online_status, std::placeholders::_1, std::placeholders::_2)},
    {"paginated_personal_notebook_view", std::bind(&paginated_personal_notebook_view, std::placeholders::_1, std::placeholders::_2)},
    {"paginated_personal_notebook_edit", std::bind(&paginated_personal_notebook_edit, std::placeholders::_1, std::placeholders::_2)},
};