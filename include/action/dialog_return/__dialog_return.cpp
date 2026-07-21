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
    {"popup", &popup},

    {"drop_item", &drop_item},
    {"trash_item", &trash_item},
    {"find_item", &find_item},

    {"gateway_edit", &gateway_edit},
    {"door_edit", &gateway_edit},
    {"sign_edit", &gateway_edit},

    {"billboard_edit", &billboard_edit},
    {"lock_edit", &lock_edit},
    {"tile_lock_edit", &tile_lock_edit},

    {"create_blast", &create_blast},
    {"socialportal", &socialportal},
    {"megaphone", &megaphone},

    {"mailbox_edit", &letter_box},
    {"bulletin_edit", &letter_box},
    {"donation_edit", &letter_box},

    {"quest_menu", &quest_menu},
    {"goalslist", &quest_menu},
    {"surgery", &surgery},

    {"display_edit", &display_edit},
    {"vending", &vending_edit},
    {"magplant_edit", &magplant_edit},

    {"trade_edit", &trade_edit},
    {"combiner_edit", &combiner_edit},

    {"set_online_status", &set_online_status},
    {"paginated_personal_notebook_view", &paginated_personal_notebook_view},
    {"paginated_personal_notebook_edit", &paginated_personal_notebook_edit},
};