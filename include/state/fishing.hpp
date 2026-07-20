#pragma once

/*
* @brief timed fishing (real GT flow):
*   1) Fishing Rod equipped + bait used/punched onto water → cast (consumes bait)
*   2) wait for splash (fish_bite)
*   3) punch while bite is up → catch; punch too early / move → miss or cancel
* @return true when the action was consumed by fishing (skip normal tile logic).
*/
extern bool try_fishing(ENetEvent &event, const ::state &state, ::block &block, ::world &world);

/* end an active cast (movement / leaving world). bait already spent. */
extern void fishing_cancel(ENetEvent &event, const char *reason = "`oYou stopped fishing.``");

/* called ~once per second from the main loop — rolls splashes / expires bite windows */
extern void fishing_tick();
