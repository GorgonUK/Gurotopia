#include "pch.hpp"
#include "on/ConsoleMessage.hpp"
#include "tools/create_dialog.hpp"

#include "world_category.hpp"

void show_world_category_dialog(ENetEvent &event)
{
    ::peer &peer = peer_of(event);
    ::world *world = current_world(peer);
    if (world == nullptr || (!peer.role && peer.user_id != world->owner))
        return;

    create_dialog dialog;
    dialog
        .set_default_color("`o")
        .add_label_with_icon("big", "`wSet World Category``", 242)
        .add_smalltext(std::format(
            "Current category: `w{}``. Choose where this world should appear.",
            world_category_name(world->category)
        ))
        .add_spacer("small")
        .add_button("world_category_0", "None");

    for (const ::world_category &category : WORLD_CATEGORIES)
        if (category.id != 0)
            dialog.add_button(
                std::format("world_category_{}", category.id),
                std::string{category.name}
            );

    send_varlist(event.peer, {
        "OnDialogRequest",
        dialog.end_dialog_with_quick_exit("world_category", "Cancel", "")
    });
}

void world_category_select(ENetEvent &event, const ::hPipe &hPipe)
{
    ::peer &peer = peer_of(event);
    ::world *world = current_world(peer);
    if (world == nullptr || (!peer.role && peer.user_id != world->owner))
        return;

    constexpr std::string_view prefix{"world_category_"};
    const std::string &clicked = hPipe["buttonClicked"];
    if (!clicked.starts_with(prefix))
        return;

    const int requested = atoi(clicked.c_str() + prefix.size());
    const auto category = std::ranges::find(
        WORLD_CATEGORIES, static_cast<u_char>(requested), &::world_category::id
    );
    if (requested < 0 || requested > 15 || category == WORLD_CATEGORIES.end())
        return;

    world->category = category->id;
    world->mark_dirty();
    on::ConsoleMessage(event.peer, std::format(
        "`2World category set to `w{}``.", category->name
    ));
}
