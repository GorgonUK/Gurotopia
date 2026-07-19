#include "pch.hpp"
#include "on/ConsoleMessage.hpp"
#include "tools/create_dialog.hpp"

#include "letter_box.hpp"

static constexpr std::size_t MAX_TILE_LETTERS = 10ull;

/* letters that belong to one tile */
static std::vector<::letter*> tile_letters(::world &world, const ::pos &pos)
{
    std::vector<::letter*> found{};
    for (::letter &letter : world.letters)
        if (letter.pos == pos) found.push_back(&letter);

    return found;
}

void letter_box_dialog(ENetEvent &event, const ::state &state, ::world &world, const ::item &item)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    const bool is_owner = (world.owner == pPeer->user_id || pPeer->role);
    const std::vector<::letter*> letters = tile_letters(world, state.punch);

    ::create_dialog dialog;
    dialog
        .set_default_color("`o")
        .add_label_with_icon("big", std::format("`w{}``", item.raw_name), item.id)
        .embed_data("tilex", state.punch.x_int())
        .embed_data("tiley", state.punch.y_int())
        .add_spacer("small");

    switch (item.type)
    {
        case type::MAILBOX:
        {
            if (is_owner)
            {
                if (letters.empty()) dialog.add_textbox("Your mailbox is empty.");
                for (const ::letter *letter : letters)
                    dialog.add_textbox(std::format("`2{}``: {}", letter->from, letter->message));

                dialog.add_spacer("small");
                if (!letters.empty()) dialog.add_button("clear_letters", "`wEmpty Mailbox``");
                send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("mailbox_edit", "Close", "") });
            }
            else
            {
                if (letters.size() >= MAX_TILE_LETTERS)
                {
                    send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "This mailbox is full!", 0u, 1u });
                    return;
                }
                dialog
                    .add_textbox("Leave a message for the owner of this world!")
                    .add_text_input("message", "", "", 100);
                send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("mailbox_edit", "Cancel", "Send") });
            }
            break;
        }
        case type::BULLETIN:
        {
            if (letters.empty()) dialog.add_textbox("No notes have been posted yet.");
            for (const ::letter *letter : letters)
                dialog.add_textbox(std::format("`2{}``: {}", letter->from, letter->message));

            dialog.add_spacer("small");
            if (letters.size() < MAX_TILE_LETTERS)
                dialog
                    .add_textbox("Post a note for everyone to see!")
                    .add_text_input("message", "", "", 100);
            if (is_owner && !letters.empty()) dialog.add_button("clear_letters", "`wClear Board``");
            send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("bulletin_edit", "Cancel", "Post") });
            break;
        }
        case type::DONATION_BOX:
        {
            if (is_owner)
            {
                if (letters.empty()) dialog.add_textbox("Nobody has donated anything yet.");
                for (const ::letter *letter : letters)
                    dialog.add_label_with_icon("small", 
                        std::format("`2{}`` x{} from `2{}``{}", id_to_item(letter->im.id).raw_name, letter->im.count, 
                            letter->from, letter->message.empty() ? "" : std::format(": {}", letter->message)), 
                        letter->im.id);

                dialog.add_spacer("small");
                if (!letters.empty()) dialog.add_button("collect_gifts", "`wCollect Donations``");
                send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("donation_edit", "Close", "") });
            }
            else
            {
                if (letters.size() >= MAX_TILE_LETTERS)
                {
                    send_varlist(event.peer, { "OnTalkBubble", pPeer->netid, "This donation box is full!", 0u, 1u });
                    return;
                }
                dialog
                    .add_textbox("Donate an item to the owner of this world!")
                    .add_item_picker("donate_item", "`wPick an item``", "Choose an item to donate!")
                    .add_text_input("donate_count", "Amount:", 1, 3)
                    .add_text_input("message", "Message:", "", 100);
                send_varlist(event.peer, { "OnDialogRequest", dialog.end_dialog("donation_edit", "Cancel", "Donate") });
            }
            break;
        }
    }
}

void letter_box(ENetEvent &event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    auto world = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
    if (world == worlds.end()) return;

    const ::pos pos{ atoi(hPipe["tilex"].c_str()), atoi(hPipe["tiley"].c_str()) };
    const ::item &item = id_to_item(world->blocks[cord(pos.x_int(), pos.y_int())].fg);
    const bool is_owner = (world->owner == pPeer->user_id || pPeer->role);

    if (hPipe["buttonClicked"] == "clear_letters")
    {
        if (!is_owner) return;

        std::erase_if(world->letters, [&pos](const ::letter &letter) { return letter.pos == pos; });
        world->mark_dirty();
        on::ConsoleMessage(event.peer, std::format("`oYou emptied the `w{}``.``", item.raw_name));
        return;
    }
    if (hPipe["buttonClicked"] == "collect_gifts")
    {
        if (!is_owner) return;

        std::erase_if(world->letters, [&pos, &event](::letter &letter)
        {
            if (letter.pos != pos || letter.im.id == 0) return false;

            const u_short nokori = modify_item_inventory(event, letter.im);
            if (nokori == 0) return true; // @note fully collected

            letter.im.count = nokori; // @note backpack full: keep what didn't fit in the box
            return false;
        });
        world->mark_dirty();
        on::ConsoleMessage(event.peer, "`oYou collected the donations.``");
        return;
    }

    /* posting a letter / note / donation */
    if (tile_letters(*world, pos).size() >= MAX_TILE_LETTERS) return;

    if (item.type == type::DONATION_BOX)
    {
        const short donate_id = atoi(hPipe["donate_item"].c_str());
        const short donate_count = std::clamp(atoi(hPipe["donate_count"].c_str()), 0, 200);
        if (donate_id == 0 || donate_count <= 0) return;

        const ::item &donated = id_to_item(donate_id);
        if (donated.id == 0 || (donated.cat & CAT_UNTRADEABLE)) return;

        const auto slot = std::ranges::find(pPeer->slots, donate_id, &::slot::id);
        if (slot == pPeer->slots.end() || slot->count < donate_count) return;

        modify_item_inventory(event, ::slot(donate_id, -donate_count));
        world->letters.emplace_back(pPeer->user_id, pPeer->growid, hPipe["message"], pos, ::slot{donate_id, donate_count});
        world->mark_dirty();
        on::ConsoleMessage(event.peer, std::format("`oYou donated `w{}`` x{}. Thank you!``", donated.raw_name, donate_count));
        return;
    }

    const std::string message = hPipe["message"];
    if (message.empty()) return;

    world->letters.emplace_back(pPeer->user_id, pPeer->growid, message, pos);
    world->mark_dirty();
    on::ConsoleMessage(event.peer, (item.type == type::MAILBOX) ? "`oYour mail has been sent!``" : "`oYour note has been posted!``");
}
