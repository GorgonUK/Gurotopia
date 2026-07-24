#include "pch.hpp"
#include "on/ConsoleMessage.hpp"
#include "database/server_config.hpp"
#include "tools/ransuu.hpp"

#include "trash_item.hpp"

namespace
{
    int recycle_max_gems(const ::item &item)
    {
        if (item.id == 242 || item.id == 1796 || item.id == 7188)
            return 0;

        const int base = static_cast<int>(item.rarity / 4) + 1;
        return std::max(0, static_cast<int>(std::lround(
            static_cast<double>(base) * gServer_config.gem_drop_multiplier
        )));
    }
}

void trash_item(ENetEvent& event, const ::hPipe &hPipe)
{
    ::peer *pPeer = &peer_of(event);

    const short itemID = static_cast<short>(atoi(hPipe["itemID"].c_str()));
    short count = static_cast<short>(atoi(hPipe["count"].c_str()));
    if (count <= 0) return;

    const ::item &item = id_to_item(itemID);
    if (item.id == 0) return;

    const short have = inventory_count(*pPeer, itemID);
    if (have <= 0) return;
    if (count > have) count = have;

    modify_item_inventory(event, ::slot(itemID, static_cast<short>(-count)));

    ransuu rng;
    const int max_gems = recycle_max_gems(item);
    int earned = 0;
    // label says "per item" — roll once per recycled unit
    for (short i = 0; i < count; ++i)
        earned += (max_gems > 0) ? rng[{0, max_gems}] : 0;

    on::ConsoleMessage(event.peer, std::format(
        "{} `w{}`` recycled, `w{}`` gems earned.", count, item.raw_name, earned
    ));

    if (earned > 0)
        pPeer->credit_gems(event, earned);

    send_action(*event.peer, "play_sfx", "file|audio/trash.wav\ndelayMS|0\n");
}
