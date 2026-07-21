#include "pch.hpp"
#include "SetBux.hpp"

namespace
{
    std::string piggy_hud_amount(int banked)
    {
        if (banked >= 1'000'000)
        {
            const double millions = banked / 1'000'000.0;
            if (std::abs(millions - std::round(millions)) < 0.05)
                return std::format("{}M", static_cast<int>(std::lround(millions)));
            return std::format("{:.1f}M", millions);
        }
        if (banked >= 1000)
            return std::format("{}K", banked / 1000);
        return std::to_string(banked);
    }

    std::string comma_gems(int n)
    {
        std::string raw = std::to_string(n);
        std::string out;
        const int len = static_cast<int>(raw.size());
        for (int i = 0; i < len; ++i)
        {
            out.push_back(raw[static_cast<std::size_t>(i)]);
            const int left = len - i - 1;
            if (left > 0 && left % 3 == 0)
                out.push_back(',');
        }
        return out;
    }
}

void on::sync_piggy_bank(ENetEvent &event)
{
    ::peer *pPeer = &peer_of(event);
    const int cap = pPeer->piggy_cap();
    pPeer->piggy_gems = std::clamp(pPeer->piggy_gems, 0, cap);

    const std::string text = std::format("{}/{}", piggy_hud_amount(pPeer->piggy_gems), piggy_hud_amount(cap));
    send_varlist(event.peer, {
        "OnEventButtonDataSet",
        "PiggyBankButton",
        1,
        std::format(
            "{{\"active\":true,\"buttonAction\":\"openPiggyBank\",\"buttonState\":1,"
            "\"buttonTemplate\":\"BaseEventButton\",\"counter\":0,\"counterMax\":0,"
            "\"itemIdIcon\":0,\"name\":\"PiggyBankButton\",\"notification\":0,\"order\":20,"
            "\"rcssClass\":\"piggybank\",\"text\":\"{}\",\"visibilityFlag\":3}}",
            text
        )
    });
}

void on::SetBux(ENetEvent& event)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    pPeer->gems = std::clamp(pPeer->gems, 0, std::numeric_limits<signed>::max());

    send_varlist(event.peer, { "OnSetBux", pPeer->gems, 1, 1 });
    on::sync_piggy_bank(event);
}

std::string on::format_gems_commas(int amount)
{
    return comma_gems(amount);
}

std::string on::format_gems_compact(int amount)
{
    return piggy_hud_amount(amount);
}
