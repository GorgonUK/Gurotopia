#pragma once

extern void combiner_edit(ENetEvent &event, const ::hPipe &hPipe);
extern void combiner_dialog(ENetEvent &event, const ::state &state, ::world &world, const ::item &item);

/* punch on a chemical combiner: open<->close, swallowing/cooking drops on its tile */
extern void combiner_toggle(ENetEvent &event, const ::state &state, ::block &block, ::world &world);
