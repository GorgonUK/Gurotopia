#include "pch.hpp"
#include "on/ConsoleMessage.hpp"

#include "worldlock_storage.hpp"

namespace
{
    constexpr short ITEM_WL = 242;
    constexpr short ITEM_DL = 1796;

    /* OnWorldLockStorageResponse blob — matches official captures (msgpack-ish). */
    std::string encode_world_lock_storage(std::uint32_t balance)
    {
        std::string blob;
        blob.push_back(static_cast<char>(0x95));
        blob.push_back(static_cast<char>(0x90));
        if (balance < 128u)
        {
            blob.push_back(static_cast<char>(balance));
            blob.append(3, '\0');
        }
        else if (balance <= 0xffffu)
        {
            blob.push_back(static_cast<char>(0xcd));
            blob.push_back(static_cast<char>((balance >> 8) & 0xff));
            blob.push_back(static_cast<char>(balance & 0xff));
            blob.append(3, '\0');
        }
        else
        {
            blob.push_back(static_cast<char>(0xce));
            blob.push_back(static_cast<char>((balance >> 24) & 0xff));
            blob.push_back(static_cast<char>((balance >> 16) & 0xff));
            blob.push_back(static_cast<char>((balance >> 8) & 0xff));
            blob.push_back(static_cast<char>(balance & 0xff));
            blob.append(3, '\0');
        }
        return blob;
    }

    void send_world_lock_storage(ENetPeer *peer, signed balance)
    {
        send_varlist(peer, {
            "OnWorldLockStorageResponse",
            encode_world_lock_storage(static_cast<std::uint32_t>(std::max(0, balance)))
        });
    }

    void send_collect_message(ENetEvent &event, int dl, int wl)
    {
        ::peer *pPeer = &peer_of(event);
        std::string message;
        if (dl > 0 && wl > 0)
            message = std::format(
                "You collected {} `2Diamond Locks`` and {} `2World Locks`.", dl, wl
            );
        else if (dl > 0)
            message = std::format("You collected {} `2Diamond Locks`.", dl);
        else if (wl > 0)
            message = std::format("You collected {} `2World Locks`.", wl);
        else
            return;

        send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, message, 0u, 1u });
        on::ConsoleMessage(event.peer, message);
    }
}

void action::worldlock_storage_getpage(ENetEvent &event, const std::string &header)
{
    (void)header;
    ::peer *pPeer = &peer_of(event);
    send_world_lock_storage(event.peer, pPeer->world_lock_bank);
}

void action::worldlock_storage_modify_amount(ENetEvent &event, const std::string &header)
{
    ::peer *pPeer = &peer_of(event);
    ::hPipe hPipe{ header };

    const int amount = atoi(hPipe["amount"].c_str());
    const int type = atoi(hPipe["type"].c_str());
    if (amount <= 0) return;

    if (type == -1) // withdraw bank → inventory
    {
        if (amount > pPeer->world_lock_bank) return;

        const int dl = amount / 100;
        const int wl = amount % 100;
        pPeer->world_lock_bank -= amount;
        pPeer->dirty = true;

        if (dl > 0) modify_item_inventory(event, { ITEM_DL, static_cast<short>(dl) });
        if (wl > 0) modify_item_inventory(event, { ITEM_WL, static_cast<short>(wl) });
        send_collect_message(event, dl, wl);
        send_world_lock_storage(event.peer, pPeer->world_lock_bank);
        return;
    }

    if (type == 1) // deposit inventory → bank
    {
        int need = amount;
        const short have_dl = inventory_count(*pPeer, ITEM_DL);
        const short have_wl = inventory_count(*pPeer, ITEM_WL);

        const int take_dl = std::min(static_cast<int>(have_dl), need / 100);
        need -= take_dl * 100;
        if (have_wl < need) return;

        const int take_wl = need;
        if (take_dl > 0)
            modify_item_inventory(event, { ITEM_DL, static_cast<short>(-take_dl) });
        if (take_wl > 0)
            modify_item_inventory(event, { ITEM_WL, static_cast<short>(-take_wl) });

        pPeer->world_lock_bank += amount;
        pPeer->dirty = true;
        send_world_lock_storage(event.peer, pPeer->world_lock_bank);
    }
}
