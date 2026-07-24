#include "pch.hpp"
#include "tools/create_dialog.hpp"
#include "database/server_config.hpp"
#include "trash.hpp"

namespace
{
    /* official: max = floor(rarity/4)+1; WL/DL/BGL never pay recycle gems */
    int recycle_max_gems(const ::item &item)
    {
        if (item.id == 242 || item.id == 1796 || item.id == 7188)
            return 0;

        const int base = static_cast<int>(item.rarity / 4) + 1;
        return std::max(0, static_cast<int>(std::lround(
            static_cast<double>(base) * gServer_config.gem_drop_multiplier
        )));
    }

    std::string recycle_gems_label(int max_gems)
    {
        if (max_gems <= 0)
            return "You will get 0 gems per item.";
        return std::format("You will get 0-{} gems per item.", max_gems);
    }
}

void action::trash(ENetEvent& event, const std::string& header)
{
    const std::vector<std::string> pipes = readch(header, '|');
    if (pipes.size() <= 4ull) return; // @note malformed packet, nothing to recycle

    const ::item &item = id_to_item(atoi(pipes[4].c_str()));

    if (item.type == type::FIST || item.type == type::WRENCH)
    {
        send_varlist(event.peer, { "OnTextOverlay", "You'd be sorry if you lost that!" });
        return;
    }
    // @todo add confirm message on untradeables
    
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    for (const ::slot &slot : pPeer->slots)
        if (slot.id == item.id)
        {
            send_varlist(event.peer, {
                "OnDialogRequest",
                create_dialog()
                    .set_default_color("`o")
                    .add_label_with_icon("big", std::format("`4Recycle`` `w{}``", item.raw_name), slot.id)
                    .add_label("small", recycle_gems_label(recycle_max_gems(item)))
                    .add_textbox(std::format("How many to `4destroy``? (you have {})", slot.count))
                    .add_text_input("count", "", 0, 5)
                    .embed_data("itemID", slot.id)
                    .end_dialog("trash_item")
            });
            return;
        }
}
