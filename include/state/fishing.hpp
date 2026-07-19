#pragma once

/*
* @brief punching water with a Fishing Rod (2912) in hand casts a line.
* @return true when the punch was consumed by fishing (skip normal tile logic).
*/
extern bool try_fishing(ENetEvent &event, const ::state &state, ::block &block, ::world &world);
