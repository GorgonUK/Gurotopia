#pragma once

/* wrench dialog for mailbox / bulletin board / donation box tiles */
extern void letter_box_dialog(ENetEvent &event, const ::state &state, ::world &world, const ::item &item);

/* dialog_return handler for mailbox_edit / bulletin_edit / donation_edit */
extern void letter_box(ENetEvent &event, const ::hPipe &hPipe);
