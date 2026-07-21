#include "pch.hpp"

#include "EmoticonDataChanged.hpp"

namespace
{
    constexpr std::string_view secret_hint =
        "`9This Growmoji can be found in a secret event.``";
    constexpr std::string_view event_hint =
        "`9This Growmoji can be found in an event.``";
    constexpr std::string_view store_hint =
        "`9You need to purchase this Growmoji from the store.``";
    constexpr std::string_view locke_hint =
        "`9These Growmojis can be found in the Growmoji Chest sold by Locke the Traveling Salesman.``";

    const std::vector<on::emoticon_entry> catalog
    {
        {"sigh", "\u010F", "", true}, {"mad", "\u0110", "", true},
        {"smile", "\u011B", "", true}, {"tongue", "\u0108", "", true},
        {"wow", "\u0111", "", true}, {"no", "\u0103", "", true},
        {"shy", "\u0106", "", true}, {"wink", "\u0107", "", true},
        {"music", "\u010C", "", true}, {"lol", "\u011A", "", true},
        {"yes", "\u0102", "", true}, {"love", "\u0104", "", true},
        {"megaphone", "\u010E", "", true}, {"heart", "\u0115", "", true},
        {"cool", "\u011C", "", true}, {"kiss", "\u0118", "", true},
        {"agree", "\u0109", "", true}, {"see-no-evil", "\u0113", "", true},
        {"dance", "\u0112", "", true}, {"build", "\u010D", "", true},
        {"ghost", "\u012D", "", true}, {"lucky", "\u0134", "", true},

        {"oops", "\u0105", secret_hint, false}, {"sleep", "\u010A", secret_hint, false},
        {"punch", "\u010B", secret_hint, false}, {"bheart", "\u0114", secret_hint, false},
        {"cry", "\u011D", secret_hint, false}, {"eyes", "\u0138", secret_hint, false},
        {"weary", "\u0139", secret_hint, false}, {"moyai", "\u013A", secret_hint, false},
        {"plead", "\u013B", secret_hint, false},

        {"bunny", "\u011F", event_hint, false}, {"cactus", "\u0120", event_hint, false},
        {"pine", "\u0121", event_hint, false}, {"peace", "\u0122", event_hint, false},
        {"terror", "\u0123", event_hint, false}, {"troll", "\u0124", event_hint, false},
        {"fireworks", "\u0126", event_hint, false}, {"party", "\u0129", event_hint, false},
        {"song", "\u012C", event_hint, false}, {"nuke", "\u012E", event_hint, false},
        {"halo", "\u012F", event_hint, false},

        {"wl", "\u0101", store_hint, false}, {"grow", "\u0116", store_hint, false},
        {"gems", "\u0117", store_hint, false}, {"gtoken", "\u0119", store_hint, false},
        {"vend", "\u011E", store_hint, false}, {"football", "\u0127", store_hint, false},

        {"alien", "\u0128", locke_hint, false}, {"evil", "\u0125", locke_hint, false},
        {"pizza", "\u012A", locke_hint, false}, {"clap", "\u012B", locke_hint, false},
        {"turkey", "\u0130", locke_hint, false}, {"gift", "\u0131", locke_hint, false},
        {"cake", "\u0132", locke_hint, false}, {"heartarrow", "\u0133", locke_hint, false},
        {"shamrock", "\u0135", locke_hint, false}, {"grin", "\u0136", locke_hint, false},
        {"ill", "\u0137", locke_hint, false}
    };
}

const std::vector<on::emoticon_entry>& on::emoticon_catalog()
{
    return catalog;
}

void on::EmoticonDataChanged(ENetEvent& event)
{
    std::string EmoticonData;
    EmoticonData.reserve(catalog.size() * 25);
    for (const auto &entry : catalog)
    {
        EmoticonData.append(std::format(
            "({})|{}|{}&", entry.name, entry.glyph, to_char(entry.unlocked)
        ));
    }

    send_varlist(event.peer, {
        "OnEmoticonDataChanged", 
        201560520, 
        EmoticonData
    });
}
