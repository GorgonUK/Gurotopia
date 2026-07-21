#include "pch.hpp"
#include "tools/create_dialog.hpp"
#include "action/wrench.hpp"

#include "collection_quests.hpp"

namespace
{
    struct mission_row
    {
        std::string_view title;
        std::string_view claim_id;
    };

    constexpr mission_row myths[]{
        { "Myths and Legends : Finding the Swamp Monster!", "claim_myth_1" },
        { "Myths and Legends : Here come the Shady Agents", "claim_myth_3" },
        { "Myths and Legends : The Search for Nessie", "claim_myth_2" },
        { "Myths and Legends : Mothman Rising", "claim_myth_4" },
        { "Myths and Legends : The Menace of the Mini Minokawa", "claim_myth_5" },
        { "Myths and Legends : The Eye of the Heavens", "claim_myth_6" },
    };

    constexpr mission_row seas[]{
        { "The Seven Seas : A Tale of Tentacles!", "claim_seven_seas_1" },
        { "The Seven Seas : The Ray of the Manta!", "claim_seven_seas_2" },
        { "The Seven Seas : The Crust of the Crab!", "claim_seven_seas_3" },
        { "The Seven Seas : The Curse of the Ghost Pirate!", "claim_seven_seas_4" },
        { "The Seven Seas : The Wings of Atlantis!", "claim_seven_seas_5" },
        { "The Seven Seas : The Heart of the Ocean!", "claim_seven_seas_6" },
    };

    void send_item_info(ENetEvent &event, short item_id)
    {
        const ::item &item = id_to_item(item_id);
        if (item.id == 0) return;

        ::create_dialog dialog;
        dialog
            .set_default_color("`o")
            .add_label_with_ele_icon(
                "big", std::format("`wAbout {}``", item.raw_name), item.id, 4
            )
            .add_spacer("small")
            .add_textbox(item.info.empty()
                ? "Crawl around the world with these exotic tentacles, leaving a trail of slime wherever you go!"
                : item.info)
            .add_spacer("small")
            .add_textbox("<CR>This item can't be spliced.")
            .add_spacer("small")
            .embed_data("itemID", item.id);
        send_varlist(event.peer, {
            "OnDialogRequest",
            dialog.end_dialog("collectionQuests", "Close", "Back")
        });
    }
}

void send_collection_quests(ENetEvent &event, int tab)
{
    ::peer &pPeer = peer_of(event);
    pPeer.collection_quests_tab = static_cast<u_char>(tab);

    ::create_dialog dialog;
    dialog
        .start_custom_tabs()
        .add_custom_button("tab_1", std::format(
            "image:interface/large/btn_mmtabs.rttex;image_size:228,92;frame:{};width:0.15;min_width:60;",
            tab == 1 ? "1,1" : "0,1"
        ))
        .add_custom_button("tab_2", std::format(
            "image:interface/large/btn_mmtabs.rttex;image_size:228,92;frame:{};width:0.15;min_width:60;",
            tab == 2 ? "1,2" : "0,2"
        ))
        .end_custom_tabs()
        .add_label_with_icon("big", "`wMarvelous Missions``", 982)
        .add_spacer("small")
        .add_textbox("Start your mission towards some awesome rewards. Rewards can only be claimed once!")
        .add_spacer("small");

    const auto &rows = (tab == 2) ? std::span{ seas } : std::span{ myths };
    for (const auto &row : rows)
    {
        dialog
            .add_textbox(std::string{ row.title })
            .add_button(std::string{ row.claim_id }, "Claim")
            .add_spacer("small");
    }

    dialog
        .add_textbox("More missions coming soon! Check back for some more surprises!")
        .add_button("back", "Back");
    send_varlist(event.peer, {
        "OnDialogRequest",
        dialog.end_dialog_with_quick_exit("collectionQuests", "", "")
    });
}

void collection_quests(ENetEvent &event, const ::hPipe &hPipe)
{
    ::peer &pPeer = peer_of(event);
    const std::string &clicked = hPipe["buttonClicked"];

    if (clicked == "tab_1")
    {
        send_collection_quests(event, 1);
        return;
    }
    if (clicked == "tab_2")
    {
        send_collection_quests(event, 2);
        return;
    }
    if (clicked == "back")
    {
        action::send_self_wrench_menu(event, pPeer.netid);
        return;
    }
    if (clicked.starts_with("info_"))
    {
        const short id = static_cast<short>(atoi(clicked.substr(5).c_str()));
        send_item_info(event, id);
        return;
    }
    if (clicked.starts_with("claim_"))
        return; // claim success path not captured

    if (!hPipe["itemID"].empty() && clicked.empty())
    {
        send_collection_quests(event, pPeer.collection_quests_tab == 2 ? 2 : 1);
        return;
    }
}
