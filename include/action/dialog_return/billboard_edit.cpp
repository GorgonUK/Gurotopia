#include "pch.hpp"

#include "on/BillboardChange.hpp"
#include "on/ConsoleMessage.hpp"

#include "billboard_edit.hpp"

void billboard_edit(ENetEvent& event, const ::hPipe &hPipe)
{
    ::peer *pPeer = &peer_of(event);

    const bool per_item = hPipe["chk_peritem"] == "1";
    const bool per_lock = hPipe["chk_perlock"] == "1";
    if (per_item && per_lock)
    {
        constexpr std::string_view message =
            "You can't select both 'locks per item' and 'items per lock'.";
        send_varlist(event.peer, {
            "OnTalkBubble", pPeer->netid, std::string{ message }, 0u, 1u
        });
        on::ConsoleMessage(event.peer, std::string{ message });
        return;
    }

    if (!hPipe["billboard_item"].empty())
        pPeer->billboard.id = atoi(hPipe["billboard_item"].c_str());
    pPeer->billboard.show = atoi(hPipe["billboard_toggle"].c_str());
    pPeer->billboard.isBuying = atoi(hPipe["billboard_buying_toggle"].c_str());
    pPeer->billboard.perItem = per_item;
    pPeer->billboard.price = atoi(hPipe["setprice"].c_str());

    on::BillboardChange(event);
}