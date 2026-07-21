#include "pch.hpp"
#include "action/openPiggyBank.hpp"
#include "on/SetBux.hpp"
#include "on/ConsoleMessage.hpp"

#include "piggy_bank.hpp"

void piggy_bank(ENetEvent &event, const ::hPipe &hPipe)
{
    if (hPipe["buttonClicked"] != "piggy_collect")
        return;

    ::peer *pPeer = &peer_of(event);

    const int cap = pPeer->piggy_cap();
    if (pPeer->piggy_gems < cap)
        return; // button is greyed out until the bank is full; ignore forged returns

    pPeer->gems = std::clamp(pPeer->gems + cap, 0, std::numeric_limits<signed>::max());
    pPeer->piggy_gems = 0;
    if (pPeer->piggy_level < 1000)
        ++pPeer->piggy_level; // next bank target grows by another 1.5M
    pPeer->mark_dirty();

    on::SetBux(event); // wallet + HUD piggy label refresh
    on::ConsoleMessage(event.peer, std::format(
        "`2You collected {} Gems from your Piggy Bank!`` Next goal: `w{} Gems``.",
        on::format_gems_commas(cap), on::format_gems_commas(pPeer->piggy_cap())
    ));

    action::openPiggyBank(event, ""); // redraw with the new, larger bank
}
