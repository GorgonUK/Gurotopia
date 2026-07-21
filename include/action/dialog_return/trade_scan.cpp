#include "pch.hpp"
#include "tools/create_dialog.hpp"

#include "trade_scan.hpp"

void send_trade_scan_main(ENetEvent &event)
{
    ::create_dialog dialog;
    dialog
        .set_default_color("`o")
        .add_label_with_icon("big", "`wTrade-Scan``", 13816)
        .add_spacer("small")
        .add_textbox("Check what price an item is currently trading at in the economy.")
        .add_textbox("You will require Trade-Scan Credits to check the price of an item.")
        .add_spacer("small")
        .add_textbox("Select an item from your inventory:")
        .add_item_picker("picked_item", "Select Item", "Choose an item")
        .add_spacer("small")
        .add_textbox("If you don't currently have an item in your inventory, you can look it up:")
        .add_button("item_search", "Search for Items")
        .add_spacer("small")
        .add_textbox("Trade-Scan Credits Collected: `20``")
        .add_button("buy_credits", "Buy Trade-Scan Credits")
        .add_spacer("small");
    send_varlist(event.peer, {
        "OnDialogRequest",
        dialog.end_dialog_with_quick_exit("trade_scan_main_ui", "Close", "")
    });
}

void send_trade_scan_search(ENetEvent &event)
{
    ::create_dialog dialog;
    dialog
        .set_default_color("`o")
        .add_label_with_icon("big", "`wTrade-Scan``", 13816)
        .add_spacer("small")
        .add_text_input("item_search_field", "Search for an item:", "", 50)
        .add_spacer("small")
        .add_textbox("Click which ONE you want to check the price of:")
        .add_spacer("small")
        .add_searchable_item_list(
            "sourceType:allItems;listType:iconWithCustomLabel;resultLimit:5;exclusionType:TradeScan",
            "item_search_field"
        )
        .add_spacer("small")
        .add_textbox("Can't find the item you're looking for? Try inputting a more precise name…")
        .add_spacer("small")
        .add_custom_button("back", "textLabel:Back;middle_colour:80543231;border_colour:80543231;");
    send_varlist(event.peer, {
        "OnDialogRequest",
        dialog.end_dialog_with_quick_exit("search_list_ui", "", "")
    });
}

void send_trade_scan_price_check(ENetEvent &event)
{
    ::peer &pPeer = peer_of(event);
    const ::item &item = id_to_item(pPeer.trade_scan_selected_item);
    if (item.id == 0)
    {
        send_trade_scan_main(event);
        return;
    }

    ::create_dialog dialog;
    dialog
        .set_default_color("`o")
        .add_label_with_icon("big", "`wTrade-Scan``", 13816)
        .add_spacer("small")
        .add_textbox("Are you sure you want to check the price of:")
        .add_spacer("small")
        .add_label_with_icon_button("small", std::format("`w{}``", item.raw_name), item.id)
        .add_spacer("small")
        .add_textbox("Checking the price of an item costs `21 Trade-Scan Credit``.")
        .add_spacer("small")
        .add_textbox("You have `20 Trade-Scan Credit``.")
        .add_spacer("small")
        .add_button("buy_credits", "Buy Trade-Scan Credit")
        .add_spacer("small")
        .add_custom_button("back", "textLabel:Back;middle_colour:80543231;border_colour:80543231;")
        .add_custom_button(
            "check_price",
            "textLabel:Check Price;anchor:_button_back;left:1;margin:60,0;state:disabled;opacity:0.3;"
        )
        .add_quick_exit();
    send_varlist(event.peer, {
        "OnDialogRequest",
        dialog.end_dialog("trade_scan_price_check_ui", "", "")
    });
}

void send_trade_scan_buy_credits(ENetEvent &event)
{
    ::peer &pPeer = peer_of(event);
    const short wl = inventory_count(pPeer, 242);
    ::create_dialog dialog;
    dialog
        .set_default_color("`o")
        .add_label_with_icon("big", "`wTrade-Scan``", 13816)
        .add_spacer("small")
        .add_textbox("How many Trade-Scan Credits do you want to buy?")
        .add_spacer("small")
        .add_text_input("credits_user_input", "", "", 4)
        .add_spacer("small")
        .add_textbox("1 World Lock = 5 Trade-Scan Credits``.")
        .add_spacer("small")
        .add_textbox(std::format("You have `4{} World Lock``.", wl))
        .add_spacer("small")
        .add_custom_button("back", "textLabel:Back;middle_colour:80543231;border_colour:80543231;")
        .add_custom_button(
            "buy_credit",
            "textLabel:Buy Trade-Scan Credit;anchor:_button_back;left:1;margin:60,0;state:disabled;opacity:0.3;"
        )
        .add_quick_exit();
    send_varlist(event.peer, {
        "OnDialogRequest",
        dialog.end_dialog("buy_credits_ui", "", "")
    });
}

void trade_scan(ENetEvent &event, const ::hPipe &hPipe)
{
    ::peer &pPeer = peer_of(event);
    const std::string &dialog = hPipe["dialog_name"];
    const std::string &clicked = hPipe["buttonClicked"];

    if (dialog == "trade_scan_main_ui")
    {
        if (clicked == "item_search")
        {
            send_trade_scan_search(event);
            return;
        }
        if (clicked == "buy_credits")
        {
            send_trade_scan_buy_credits(event);
            return;
        }
        if (!hPipe["picked_item"].empty())
        {
            pPeer.trade_scan_selected_item = static_cast<short>(atoi(hPipe["picked_item"].c_str()));
            send_trade_scan_price_check(event);
            return;
        }
        return;
    }

    if (dialog == "search_list_ui")
    {
        if (clicked == "back")
        {
            send_trade_scan_main(event);
            return;
        }
        constexpr std::string_view prefix = "searchableItemListButton_";
        if (clicked.starts_with(prefix))
        {
            const std::string rest = clicked.substr(prefix.size());
            const auto end = rest.find('_');
            const short id = static_cast<short>(atoi(
                (end == std::string::npos ? rest : rest.substr(0, end)).c_str()
            ));
            if (id_to_item(id).id == id)
            {
                pPeer.trade_scan_selected_item = id;
                send_trade_scan_price_check(event);
            }
            return;
        }
        return;
    }

    if (dialog == "trade_scan_price_check_ui")
    {
        if (clicked == "buy_credits")
        {
            send_trade_scan_buy_credits(event);
            return;
        }
        if (clicked == "back")
        {
            send_trade_scan_main(event);
            return;
        }
        return; // check_price disabled / uncaptured success path
    }

    if (dialog == "buy_credits_ui")
    {
        if (clicked == "back")
        {
            if (pPeer.trade_scan_selected_item != 0)
                send_trade_scan_price_check(event);
            else
                send_trade_scan_main(event);
            return;
        }
        return; // buy_credit success path not captured
    }
}
