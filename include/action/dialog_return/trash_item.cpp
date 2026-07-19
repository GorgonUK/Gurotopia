#include "pch.hpp"
#include "on/ConsoleMessage.hpp"
#include "tools/logger.hpp"

#include "trash_item.hpp"

void trash_item(ENetEvent& event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    const short itemID = atoi(hPipe["itemID"].c_str());
    short count = atoi(hPipe["count"].c_str());

    const ::item &item = id_to_item(itemID);

    for (const ::slot &slot : pPeer->slots)
        if (slot.id == itemID)
            if (count > slot.count) count = slot.count;
            else if (count < 0) count = 0;

    modify_item_inventory(event, ::slot(itemID, -count));
    on::ConsoleMessage(event.peer, std::format("{} `w{}`` recycled, `w0`` gems earned.", count, item.raw_name));
    log_event("trash", std::format("{}({})", pPeer->growid, pPeer->user_id),
        pPeer->recent_worlds.back(), std::format("item={} ({}) count={}", itemID, item.raw_name, count));
}