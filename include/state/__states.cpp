#include "pch.hpp"
#include "movement.hpp"
#include "tile_change.hpp"
#include "tile_activate.hpp"
#include "item_activate.hpp"
#include "item_activate_object.hpp"
#include "ping_reply.hpp"
#include "disconnect.hpp"
#include "__states.hpp"

std::unordered_map<u_char, std::function<void(ENetEvent&, state)>> state_pool
{
    {packet::STATE,                        &movement},
    {packet::TILE_CHANGE_REQUEST,          &tile_change},
    {packet::TILE_ACTIVATE_REQUEST,        &tile_activate},
    {packet::ITEM_ACTIVATE_REQUEST,        &item_activate},
    {packet::ITEM_ACTIVATE_OBJECT_REQUEST, &item_activate_object},
    {packet::PING_REPLY,                   &ping_reply},
    {packet::DISCONNECT,                   &disconnect}
};