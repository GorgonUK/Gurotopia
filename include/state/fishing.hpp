#pragma once

/*
* @brief cast a line: Fishing Rod (2912) equipped + bait (e.g. Wiggly Worm 2914)
* used on / punched onto a water tile. Consumes 1 bait.
* @return true when the action was consumed by fishing (skip normal tile logic).
*/
extern bool try_fishing(ENetEvent &event, const ::state &state, ::block &block, ::world &world);
