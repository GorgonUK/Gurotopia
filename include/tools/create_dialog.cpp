#include "pch.hpp"
#include "create_dialog.hpp"

create_dialog& create_dialog::set_default_color(std::string code)
{
    _d.append(std::format("set_default_color|{}\n", code));
    return *this;
}
create_dialog& create_dialog::add_label(std::string size, std::string label)
{
    return add_label(std::move(size), std::move(label), "left");
}
create_dialog& create_dialog::add_label(std::string size, std::string label, std::string align)
{
    _d.append(std::format("add_label|{}|{}|{}|\n", size, label, align));
    return *this;
}
create_dialog& create_dialog::add_label_with_icon(std::string size, std::string label, int icon)
{
    _d.append(std::format("add_label_with_icon|{}|{}|left|{}|\n", size, label, icon));
    return *this;
}
create_dialog& create_dialog::add_label_with_ele_icon(std::string size, std::string label, int icon, u_char element)
{
    _d.append(std::format("add_label_with_ele_icon|{}|{}|left|{}|{}|\n", size, label, icon, element));
    return *this;
}
create_dialog& create_dialog::add_label_with_icon_button(std::string size, std::string label, int icon)
{
    _d.append(std::format("add_label_with_icon_button|{}|{}|left|{}|||\n", size, label, icon));
    return *this;
}
create_dialog& create_dialog::add_textbox(std::string label)
{
    _d.append(std::format("add_textbox|{}|left|\n", label));
    return *this;
}
create_dialog& create_dialog::add_text_input(std::string id, std::string label, short set_value, short length)
{
    _d.append(std::format("add_text_input|{}|{}|{}|{}|\n", id, label, set_value, length));
    return *this;
}
create_dialog& create_dialog::add_text_input(std::string id, std::string label, std::string set_value, short length)
{
    _d.append(std::format("add_text_input|{}|{}|{}|{}|\n", id, label, set_value, length));
    return *this;
}
create_dialog& create_dialog::add_checkbox(std::string id, std::string label, bool checked)
{
    _d.append(std::format("add_checkbox|{}|{}|{}|\n", id, label, to_char(checked)));
    return *this;
}
create_dialog& create_dialog::add_smalltext(std::string label)
{
    _d.append(std::format("add_smalltext|{}|left|\n", label));
    return *this;
}
create_dialog& create_dialog::add_smalltext_forced(std::string label)
{
    _d.append(std::format("add_smalltext_forced|{}|left|\n", label));
    return *this;
}
create_dialog& create_dialog::add_smalltext_forced_alpha(std::string label, float alpha)
{
    _d.append(std::format("add_smalltext_forced_alpha|{}|{}|left|\n", label, alpha));
    return *this;
}
create_dialog& create_dialog::embed_data(std::string id, std::string data)
{
    _d.append(std::format("embed_data|{}|{}\n", id, data));
    return *this;
}
create_dialog& create_dialog::embed_data(std::string id, int data)
{
    _d.append(std::format("embed_data|{}|{}\n", id, data));
    return *this;
}
create_dialog& create_dialog::add_spacer(std::string size)
{
    _d.append(std::format("add_spacer|{}|\n", size));
    return *this;
}
create_dialog& create_dialog::start_custom_tabs()
{
    _d.append("start_custom_tabs|\n");
    return *this;
}
create_dialog& create_dialog::end_custom_tabs()
{
    _d.append("end_custom_tabs|\n");
    return *this;
}
create_dialog& create_dialog::add_dialog_width_anchor(short min_width)
{
    _d.append(std::format(
        "add_custom_button|_dialog_width_anchor|"
        "state:disabled;opacity:0;display:block;height:0;min_width:{};|\n",
        min_width
    ));
    return *this;
}
create_dialog& create_dialog::reset_placement_x()
{
    _d.append("reset_placement_x|\n");
    return *this;
}
create_dialog& create_dialog::disable_resize()
{
    _d.append("disable_resize|\n");
    return *this;
}
create_dialog& create_dialog::set_custom_spacing(short x, short y)
{
    _d.append(std::format("set_custom_spacing|x:{};y:{}|\n", x, y));
    return *this;
}
create_dialog& create_dialog::add_layout_spacer(std::string layout)
{
    _d.append(std::format("add_layout_spacer|{}|\n", layout));
    return *this;
}
create_dialog& create_dialog::add_button(std::string btn_id, std::string btn_name)
{
    _d.append(std::format("add_button|{}|{}|noflags|0|0|\n", btn_id, btn_name));
    return *this;
}
create_dialog& create_dialog::add_image_button(std::string btn_id, std::string image, std::string layout, std::string link)
{
    _d.append(std::format("add_image_button|{}|{}|{}|{}|\n", btn_id, image, layout, link));
    return *this;
}
create_dialog& create_dialog::add_image_button(
    std::string btn_id, std::string image, std::string layout, std::string link,
    std::string target, std::string offset
)
{
    _d.append(std::format(
        "add_image_button|{}|{}|{}|{}|{}|{}|\n", btn_id, image, layout, link, target, offset
    ));
    return *this;
}
create_dialog& create_dialog::add_custom_button(std::string btn_id, std::string image)
{
    _d.append(std::format("add_custom_button|{}|{}|\n", btn_id, image));
    return *this;
}
create_dialog& create_dialog::add_custom_label(std::string btn_id, std::string pos)
{
    _d.append(std::format("add_custom_label|{}|{}|\n", btn_id, pos));
    return *this;
}
create_dialog& create_dialog::add_custom_break()
{
    _d.append("add_custom_break|\n");
    return *this;
}
create_dialog& create_dialog::add_custom_margin(short x, short y)
{
    _d.append(std::format("add_custom_margin|x:{};y:{}|\n", x, y));
    return *this;
}
create_dialog& create_dialog::add_custom_textbox(std::string label, std::string size)
{
    _d.append(std::format("add_custom_textbox|{}|size:{}|\n", label, size));
    return *this;
}
create_dialog& create_dialog::add_achieve(std::string name, std::string desc, int icon)
{
    _d.append(std::format("add_achieve|{}|{}|left|{}|\n", name, desc, icon));
    return *this;
}
create_dialog& create_dialog::add_achieve(std::string total)
{
    _d.append(std::format("add_achieve|||left|{}|\n", total)); // wrench peer summary
    return *this;
}
create_dialog& create_dialog::add_quick_exit()
{
    _d.append("add_quick_exit|\n");
    return *this;
}
create_dialog& create_dialog::add_popup_name(std::string popup_name)
{
    _d.append(std::format("add_popup_name|{}|\n", popup_name));
    return *this;
}
create_dialog& create_dialog::add_player_info(std::string label, std::string progress_bar_name, int progress, int total_progress)
{
    _d.append(std::format("add_player_info|{}|{}|{}|{}|\n", label, progress_bar_name, progress, total_progress));
    return *this;
}
create_dialog& create_dialog::add_item_picker(std::string id, std::string label, std::string selection_prompt)
{
    _d.append(std::format("add_item_picker|{}|{}|{}|\n", id, label, selection_prompt));
    return *this;
}
create_dialog& create_dialog::add_searchable_item_list(std::string options, std::string field_id)
{
    _d.append(std::format("add_searchable_item_list||{}|{}|\n", options, field_id));
    return *this;
}

std::string create_dialog::end_dialog(std::string btn_id, std::string btn_close, std::string btn_return) 
{ 
    _d.append(std::format("end_dialog|{}|{}|{}|\n", btn_id, btn_close, btn_return));
    return _d; 
}

std::string create_dialog::end_dialog_with_quick_exit(
    std::string btn_id, std::string btn_close, std::string btn_return
)
{
    _d.append(std::format(
        "end_dialog|{}|{}|{}|\nadd_quick_exit|\n", btn_id, btn_close, btn_return
    ));
    return _d;
}