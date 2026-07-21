#include "pch.hpp"
#include "connect.hpp"
#include "disconnect.hpp"
#include "receive.hpp"
#include "__event_type.hpp"

std::unordered_map<ENetEventType, std::function<void(ENetEvent&)>> event_pool
{
    {::ENET_EVENT_TYPE_CONNECT, &_connect},
    {::ENET_EVENT_TYPE_DISCONNECT, &disconnect},
    {::ENET_EVENT_TYPE_RECEIVE, &receive},
};
