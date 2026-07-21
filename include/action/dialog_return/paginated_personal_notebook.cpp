#include "pch.hpp"

#include "paginated_personal_notebook.hpp"

static int clamp_page(int page_num)
{
    if (page_num < 0) return 0;
    if (page_num > 4) return 4;
    return page_num;
}

void send_notebook_view(ENetEvent &event, int page_num)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    page_num = clamp_page(page_num);

    std::string tabs = "start_custom_tabs|\n";
    for (int i = 0; i < 5; ++i)
    {
        tabs.append(std::format(
            "add_custom_button|viewPnPage_{0}|image:interface/large/btn_tabs1.rttex;image_size:228,92;frame:{1},{0};width:0.15;|\n",
            i,
            (i == page_num) ? 1 : 0
        ));
    }
    tabs.append(
        "end_custom_tabs|\n"
        "add_custom_button|_dialog_width_anchor|"
        "state:disabled;opacity:0;display:block;height:0;min_width:650;|\n"
    );

    const std::string &note = pPeer->notebook_pages[static_cast<std::size_t>(page_num)];
    const std::string body = note.empty() ? "`4No saved note to display.``" : note;

    send_varlist(event.peer, {
        "OnDialogRequest",
        std::format(
            "set_default_color|`o\n"
            "{}"
            "add_label|big|Notebook|left|0|\n"
            "add_spacer|small|\n"
            "add_textbox|{}|left|\n"
            "add_spacer|small|\n"
            "add_button|editPnPage|Edit|noflags|0|0|\n"
            "add_button|back|Back|noflags|0|0|\n"
            "embed_data|pageNum|{}\n"
            "end_dialog|paginated_personal_notebook_view|||\n"
            "add_quick_exit|\n",
            tabs,
            body,
            page_num
        )
    });
}

void send_notebook_edit(ENetEvent &event, int page_num, const std::string &prefill)
{
    page_num = clamp_page(page_num);
    std::string text = prefill;
    if (text.size() > 256ull)
        text.resize(256ull);

    send_varlist(event.peer, {
        "OnDialogRequest",
        std::format(
            "set_default_color|`o\n"
            "add_label|big|Notebook|left|0|\n"
            "add_spacer|small|\n"
            "add_textbox|Page {}|left|\n"
            "add_spacer|small|\n"
            "add_text_box_input|personal_note||{}|256|7|\n"
            "add_smalltext|* Max `5256`` characters!|left|\n"
            "add_smalltext|* Type `5<`5CR``>`` to add a new line.|left|\n"
            "add_spacer|small|\n"
            "add_button|save|Save|noflags|0|0|\n"
            "add_button|clear|Clear|noflags|0|0|\n"
            "add_button|back|Cancel|noflags|0|0|\n"
            "embed_data|pageNum|{}\n"
            "end_dialog|paginated_personal_notebook_edit||\n"
            "add_quick_exit|\n",
            page_num + 1,
            text,
            page_num
        )
    });
}

void paginated_personal_notebook_view(ENetEvent &event, const ::hPipe &hPipe)
{
    const int page_num = clamp_page(atoi(hPipe["pageNum"].c_str()));
    const std::string &clicked = hPipe["buttonClicked"];

    if (clicked.starts_with("viewPnPage_"))
    {
        send_notebook_view(event, atoi(clicked.substr(sizeof("viewPnPage_") - 1).c_str()));
        return;
    }
    if (clicked == "editPnPage")
    {
        ::peer *pPeer = static_cast<::peer*>(event.peer->data);
        send_notebook_edit(event, page_num, pPeer->notebook_pages[static_cast<std::size_t>(page_num)]);
        return;
    }
    // back / empty — close
}

void paginated_personal_notebook_edit(ENetEvent &event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    const int page_num = clamp_page(atoi(hPipe["pageNum"].c_str()));
    const std::string &clicked = hPipe["buttonClicked"];
    std::string note = hPipe["personal_note"];
    if (note.size() > 256ull)
        note.resize(256ull);

    if (clicked == "save")
    {
        pPeer->notebook_pages[static_cast<std::size_t>(page_num)] = note;
        pPeer->mark_dirty();
        send_notebook_view(event, page_num);
        return;
    }
    if (clicked == "clear")
    {
        // UI-only: reopen empty editor; do not wipe stored page.
        send_notebook_edit(event, page_num, "");
        return;
    }
    if (clicked == "back")
    {
        send_notebook_view(event, page_num);
        return;
    }
}
