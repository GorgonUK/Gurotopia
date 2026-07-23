#include "pch.hpp"
#include "action/__action.hpp"
#include "state/__states.hpp"
#include "receive.hpp"

void receive(ENetEvent& event) 
{
    std::span<const enet_uint8> data{event.packet->data, event.packet->dataLength};

    // @note drop empty packets, and packets from peers that never got a ::peer allocated
    // (connections rejected on overload keep peer->data == nullptr — see connect.cpp).
    if (data.empty() || !event.peer->data)
    {
        enet_packet_destroy(event.packet);
        return;
    }

    try // @note one malformed packet must never terminate the whole server.
    {
        switch (data[0ull])
        {
            case packet::GENERIC_TEXT: case packet::GAME_MESSAGE:
            {
                if (data.size() < 5) break; // @note 4-byte type prefix + trailing byte

                std::string header{data.begin() + 4, data.end() - 1};
                puts(header.c_str());

                std::ranges::replace(header, '\n', '|');
                const std::vector<std::string> pipes = readch(header, '|');
                if (pipes.size() < 2) break;

                std::string action{};
                if (pipes[0ull] == "protocol" || pipes[0ull] == "tankIDName")
                {
                    action = pipes[0ull];
                }
                else action = std::format("{}|{}", pipes[0ull], pipes[1ull]);

                if (const auto i = action_pool.find(action); i != action_pool.end())
                    i->second(event, header);
                break;
            }
            case packet::GAME_PACKET:
            {
                if (event.packet->dataLength < sizeof(::state)) break;

                ::state state = get_state({event.packet->data, event.packet->data + (event.packet->dataLength)});

                if (const auto i = state_pool.find(state.type); i != state_pool.end())
                    i->second(event, std::move(state));
                break;
            }
        }
    }
    catch (const std::exception &e)
    {
        fprintf(stderr, "[receive] dropped packet: %s\n", e.what());
    }

    enet_packet_destroy(event.packet);
}

