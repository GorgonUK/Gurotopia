#pragma once

/* in-memory world bans (survive world unload, reset on server restart) */

/* ban uid from world for 10 minutes */
extern void world_ban_add(const std::string &world, int uid);

/* true while uid still has an active ban in world */
extern bool world_banned(const std::string &world, int uid);
