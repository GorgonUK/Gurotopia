#include "pch.hpp"
#include "tools/create_dialog.hpp"
#include "on/ConsoleMessage.hpp"

#include "trade.hpp"

static ENetPeer *find_peer_by_netid(const std::string &world_name, short netid)
{
    ENetPeer *found{};
    peers(world_name, PEER_SAME_WORLD, [netid, &found](ENetPeer &peer)
    {
        if (static_cast<::peer*>(peer.data)->netid == netid)
            found = &peer;
    });
    return found;
}

static void clear_trade(::peer *p)
{
    p->trade_with_netid = -1;
    p->trade_offer.clear();
    p->trade_ready = false;
    p->trade_confirmed = false;
}

static std::string format_offer(const std::vector<::slot> &offer)
{
    if (offer.empty()) return "`o(nothing)``";
    std::string out;
    for (const ::slot &s : offer)
    {
        const ::item &item = id_to_item(s.id);
        out += std::format("`w{}`` x{}  ", item.raw_name, s.count);
    }
    return out;
}

static void send_trade_ui(ENetEvent &event, ENetPeer *other)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    ::peer *pOther = static_cast<::peer*>(other->data);

    ::create_dialog dialog;
    dialog
        .set_default_color("`o")
        .add_label_with_icon("big", std::format("`wTrade with {}``", pOther->growid), 1366)
        .embed_data("netID", pOther->netid)
        .add_spacer("small")
        .add_textbox(std::format("`wYour offer:`` {}", format_offer(pPeer->trade_offer)))
        .add_textbox(std::format("`wTheir offer:`` {}", format_offer(pOther->trade_offer)))
        .add_spacer("small")
        .add_text_input("item_id", "Item ID:", "", 6)
        .add_text_input("item_count", "Count:", "1", 3)
        .add_button("add_item", "`wAdd item to offer``")
        .add_button("clear_offer", "`4Clear my offer``")
        .add_spacer("small");

    if (!pPeer->trade_ready)
        dialog.add_button("ready", "`2Ready``");
    else
        dialog.add_textbox("`2You are ready. Waiting on the other player...``");

    dialog
        .add_button("cancel_trade", "`4Cancel trade``")
        .add_spacer("small")
        .add_quick_exit();

    send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("trade_edit", "", "") });
}

void trade_dialog(ENetEvent &event, ENetPeer *target)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    ::peer *pTarget = static_cast<::peer*>(target->data);

    if (pPeer->trade_with_netid >= 0 || pTarget->trade_with_netid >= 0)
    {
        on::ConsoleMessage(event.peer, "`4One of you is already trading.``");
        return;
    }

    pPeer->trade_with_netid = pTarget->netid;
    pTarget->trade_with_netid = pPeer->netid;
    clear_trade(pPeer); // resets offer/flags but we need netid
    pPeer->trade_with_netid = pTarget->netid;
    clear_trade(pTarget);
    pTarget->trade_with_netid = pPeer->netid;

    on::ConsoleMessage(event.peer, std::format("`2Trade started with `w{}``.``", pTarget->growid));
    on::ConsoleMessage(target, std::format("`2Trade started with `w{}``.``", pPeer->growid));

    send_trade_ui(event, target);
    ENetEvent other_ev{ .peer = target };
    send_trade_ui(other_ev, event.peer);
}

static bool inventory_has(::peer *p, short id, short count)
{
    auto it = std::ranges::find(p->slots, id, &::slot::id);
    return it != p->slots.end() && it->count >= count;
}

static void finalize_trade(ENetPeer *a, ENetPeer *b)
{
    ::peer *pa = static_cast<::peer*>(a->data);
    ::peer *pb = static_cast<::peer*>(b->data);

    // validate both still hold what they offered
    for (const ::slot &s : pa->trade_offer)
    {
        if (id_to_item(s.id).cat & CAT_UNTRADEABLE) { clear_trade(pa); clear_trade(pb); return; }
        if (!inventory_has(pa, s.id, s.count)) { clear_trade(pa); clear_trade(pb); return; }
    }
    for (const ::slot &s : pb->trade_offer)
    {
        if (id_to_item(s.id).cat & CAT_UNTRADEABLE) { clear_trade(pa); clear_trade(pb); return; }
        if (!inventory_has(pb, s.id, s.count)) { clear_trade(pa); clear_trade(pb); return; }
    }

    ENetEvent ea{ .peer = a }, eb{ .peer = b };

    for (const ::slot &s : pa->trade_offer)
    {
        modify_item_inventory(ea, ::slot(s.id, static_cast<short>(-s.count)));
        modify_item_inventory(eb, ::slot(s.id, s.count));
    }
    for (const ::slot &s : pb->trade_offer)
    {
        modify_item_inventory(eb, ::slot(s.id, static_cast<short>(-s.count)));
        modify_item_inventory(ea, ::slot(s.id, s.count));
    }

    on::ConsoleMessage(a, "`2Trade complete!``");
    on::ConsoleMessage(b, "`2Trade complete!``");
    clear_trade(pa);
    clear_trade(pb);
}

void trade_edit(ENetEvent &event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    if (pPeer->trade_with_netid < 0) return;

    ENetPeer *other = find_peer_by_netid(pPeer->recent_worlds.back(), static_cast<short>(pPeer->trade_with_netid));
    if (other == nullptr)
    {
        clear_trade(pPeer);
        on::ConsoleMessage(event.peer, "`4Trade canceled — other player left.``");
        return;
    }
    ::peer *pOther = static_cast<::peer*>(other->data);

    const std::string &btn = hPipe["buttonClicked"];

    if (btn == "cancel_trade")
    {
        clear_trade(pPeer);
        clear_trade(pOther);
        on::ConsoleMessage(event.peer, "`4Trade canceled.``");
        on::ConsoleMessage(other, "`4Trade canceled.``");
        return;
    }

    if (btn == "clear_offer")
    {
        pPeer->trade_offer.clear();
        pPeer->trade_ready = false;
        pOther->trade_ready = false;
        send_trade_ui(event, other);
        ENetEvent oev{ .peer = other };
        send_trade_ui(oev, event.peer);
        return;
    }

    if (btn == "add_item")
    {
        const short id = static_cast<short>(atoi(hPipe["item_id"].c_str()));
        short count = static_cast<short>(atoi(hPipe["item_count"].c_str()));
        if (id <= 0 || count <= 0) return;
        count = std::clamp(count, short{1}, short{200});

        const ::item &item = id_to_item(id);
        if (item.id == 0 || (item.cat & CAT_UNTRADEABLE))
        {
            on::ConsoleMessage(event.peer, "`4That item can't be traded.``");
            return;
        }
        if (!inventory_has(pPeer, id, count))
        {
            on::ConsoleMessage(event.peer, "`4You don't have that many.``");
            return;
        }
        if (pPeer->trade_offer.size() >= 4ull)
        {
            on::ConsoleMessage(event.peer, "`4Offer is full (max 4 stacks).``");
            return;
        }

        // merge into existing offer stack if same id
        auto it = std::ranges::find(pPeer->trade_offer, id, &::slot::id);
        if (it != pPeer->trade_offer.end())
            it->count = static_cast<short>(std::min(200, it->count + count));
        else
            pPeer->trade_offer.emplace_back(id, count);

        pPeer->trade_ready = false;
        pOther->trade_ready = false;
        send_trade_ui(event, other);
        ENetEvent oev{ .peer = other };
        send_trade_ui(oev, event.peer);
        return;
    }

    if (btn == "ready")
    {
        pPeer->trade_ready = true;
        if (pPeer->trade_ready && pOther->trade_ready)
        {
            // both ready — confirm dialog
            ::create_dialog confirm;
            confirm
                .set_default_color("`o")
                .add_label_with_icon("big", "`wConfirm Trade``", 1366)
                .embed_data("netID", pOther->netid)
                .add_spacer("small")
                .add_textbox(std::format("`4You'll give:`` {}", format_offer(pPeer->trade_offer)))
                .add_textbox(std::format("`2You'll get:`` {}", format_offer(pOther->trade_offer)))
                .add_spacer("small")
                .add_button("do_trade", "`2Accept trade``")
                .add_button("cancel_trade", "`4Cancel``");
            send_varlist(event.peer, { "OnDialogRequest", confirm.end_dialog("trade_edit", "", "") });

            ::create_dialog confirm2;
            confirm2
                .set_default_color("`o")
                .add_label_with_icon("big", "`wConfirm Trade``", 1366)
                .embed_data("netID", pPeer->netid)
                .add_spacer("small")
                .add_textbox(std::format("`4You'll give:`` {}", format_offer(pOther->trade_offer)))
                .add_textbox(std::format("`2You'll get:`` {}", format_offer(pPeer->trade_offer)))
                .add_spacer("small")
                .add_button("do_trade", "`2Accept trade``")
                .add_button("cancel_trade", "`4Cancel``");
            send_varlist(other, { "OnDialogRequest", confirm2.end_dialog("trade_edit", "", "") });
            return;
        }
        send_trade_ui(event, other);
        ENetEvent oev{ .peer = other };
        send_trade_ui(oev, event.peer);
        return;
    }

    if (btn == "do_trade")
    {
        pPeer->trade_confirmed = true;
        if (pPeer->trade_confirmed && pOther->trade_confirmed)
            finalize_trade(event.peer, other);
        else
            on::ConsoleMessage(event.peer, "`2Waiting for the other player to confirm...``");
    }
}
