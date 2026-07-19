#include "pch.hpp"
#include "on/ConsoleMessage.hpp"
#include "on/SetBux.hpp"

#include "give.hpp"

static bool require_mod(ENetEvent &event)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    if (pPeer->role < MODERATOR)
    {
        on::ConsoleMessage(event.peer, "`4You don't have permission to use that command.``");
        return false;
    }
    return true;
}

/* /give <id> [count] — give yourself an item (mod+) */
void give(ENetEvent& event, const std::string_view text)
{
    if (!require_mod(event)) return;

    std::vector<std::string> args = readch(std::string(text), ' ');
    if (args.size() < 2ull)
    {
        on::ConsoleMessage(event.peer, "`oUsage: `/give <itemID> [count]``");
        return;
    }

    const int id = atoi(args[1].c_str());
    int count = (args.size() >= 3ull) ? atoi(args[2].c_str()) : 1;
    if (id <= 0 || count <= 0)
    {
        on::ConsoleMessage(event.peer, "`4Invalid item ID or count.``");
        return;
    }
    count = std::clamp(count, 1, 200);

    const ::item &item = id_to_item(static_cast<u_short>(id));
    if (item.id == 0)
    {
        on::ConsoleMessage(event.peer, "`4Unknown item ID.``");
        return;
    }

    modify_item_inventory(event, ::slot(static_cast<short>(id), static_cast<short>(count)));
    on::ConsoleMessage(event.peer, std::format("`2Gave `w{}`` x{}``.", item.raw_name, count));
}

/* /setgems <amount> — set your gem balance (mod+) */
void setgems(ENetEvent& event, const std::string_view text)
{
    if (!require_mod(event)) return;

    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    std::vector<std::string> args = readch(std::string(text), ' ');
    if (args.size() < 2ull)
    {
        on::ConsoleMessage(event.peer, "`oUsage: `/setgems <amount>``");
        return;
    }

    const long long amount = std::atoll(args[1].c_str());
    if (amount < 0 || amount > std::numeric_limits<signed>::max())
    {
        on::ConsoleMessage(event.peer, "`4Invalid gem amount.``");
        return;
    }

    pPeer->gems = static_cast<signed>(amount);
    pPeer->mark_dirty();
    on::SetBux(event);
    on::ConsoleMessage(event.peer, std::format("`2Gems set to `w{}``.", pPeer->gems));
}
