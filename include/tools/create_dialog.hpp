#pragma once

class create_dialog
{
    std::string _d{};

public:
    create_dialog& set_default_color(std::string code);
    create_dialog& add_label(std::string size, std::string label);
    create_dialog& add_label(std::string size, std::string label, std::string align);
    create_dialog& add_label_with_icon(std::string size, std::string label, int icon);
    create_dialog& add_label_with_ele_icon(std::string size, std::string label, int icon, u_char element);
    create_dialog& add_label_with_icon_button(std::string size, std::string label, int icon);
    create_dialog& add_textbox(std::string label);
    create_dialog& add_text_input(std::string id, std::string label, short set_value, short length);
    create_dialog& add_text_input(std::string id, std::string label, std::string set_value, short length);
    create_dialog& add_checkbox(std::string id, std::string label, bool checked);
    create_dialog& add_smalltext(std::string label);
    create_dialog& add_smalltext_forced(std::string label);
    create_dialog& add_smalltext_forced_alpha(std::string label, float alpha);
    create_dialog& embed_data(std::string id, std::string data);
    create_dialog& embed_data(std::string id, int data);
    create_dialog& add_spacer(std::string size);
    create_dialog& start_custom_tabs();
    create_dialog& end_custom_tabs();
    create_dialog& add_dialog_width_anchor(short min_width);
    create_dialog& reset_placement_x();
    create_dialog& disable_resize();
    create_dialog& set_custom_spacing(short x, short y);
    create_dialog& add_layout_spacer(std::string layout);
    create_dialog& add_button(std::string btn_id, std::string btn_name);
    create_dialog& add_image_button(std::string btn_id, std::string image, std::string layout, std::string link);
    create_dialog& add_image_button(
        std::string btn_id, std::string image, std::string layout, std::string link,
        std::string target, std::string offset
    );
    create_dialog& add_custom_button(std::string btn_id, std::string image);
    create_dialog& add_custom_label(std::string label, std::string pos);
    create_dialog& add_custom_break();
    create_dialog& add_custom_margin(short x, short y);
    create_dialog& add_custom_textbox(std::string label, std::string size);
    create_dialog& add_achieve(std::string name, std::string desc, int icon);
    create_dialog& add_achieve(std::string total); // wrench summary stub
    create_dialog& add_quick_exit();
    create_dialog& add_popup_name(std::string popup_name);
    create_dialog& add_player_info(std::string label, std::string progress_bar_name, int progress, int total_progress);
    create_dialog& add_item_picker(std::string id, std::string label, std::string selection_prompt);
    create_dialog& add_searchable_item_list(std::string options, std::string field_id);

    std::string end_dialog(std::string btn_id, std::string btn_close = "Cancel", std::string btn_return = "OK");
    std::string end_dialog_with_quick_exit(
        std::string btn_id, std::string btn_close = "Cancel", std::string btn_return = "OK"
    );
};
