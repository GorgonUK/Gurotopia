#pragma once

/*
    Central home for Growtopia wire-protocol constants.

    These values used to live as bare integer literals at every send/receive site,
    each re-documented with a `// @note PACKET_*` / `// @note NET_MESSAGE_*` comment.
    Keep them here so the protocol is described once and misuse is a name error.

    Plain (unscoped) enums on purpose: `::state::type` and the ENet packet byte are
    ints, and several senders OR payload bits into `type` (e.g. `(count << 24) | ...`),
    so implicit conversion to int keeps every existing call site a drop-in.
*/
namespace packet
{
    /* ENetPacket first byte — ENet "channel" message class. */
    enum net_message : u_char
    {
        GENERIC_TEXT = 2, // @note pipe-delimited text, no game-packet header
        GAME_MESSAGE = 3, // @note action|... text
        GAME_PACKET  = 4  // @note binary ::state tank packet
    };

    /*
        Tank packet type (::state::type low byte).

        Inbound names mirror the handler each type dispatches to in state/__states.cpp;
        outbound names match the PACKET_* comments already present at the send sites.
    */
    enum tank : u_char
    {
        /* inbound (registered in state_pool) */
        STATE                       = 0x00, // movement
        TILE_CHANGE_REQUEST         = 0x03, // tile_change
        TILE_ACTIVATE_REQUEST       = 0x07, // tile_activate
        ITEM_ACTIVATE_REQUEST       = 0x0a, // item_activate
        ITEM_ACTIVATE_OBJECT_REQUEST= 0x0b, // item_activate_object
        PING_REPLY                  = 0x15, // ping_reply
        DISCONNECT                  = 0x1a, // disconnect

        /* outbound (server -> client) */
        CALL_FUNCTION               = 0x01, // VariantList (On*)
        SEND_MAP_DATA               = 0x04, // full world blob (join_request)
        SEND_TILE_UPDATE_DATA       = 0x05, // single tile refresh
        TILE_APPLY_DAMAGE           = 0x08,
        SEND_INVENTORY_STATE        = 0x09,
        MODIFY_ITEM_INVENTORY       = 0x0d,
        ITEM_CHANGE_OBJECT          = 0x0e, // spawn / move / remove a world drop
        SEND_LOCK                   = 0x0f,
        SEND_ITEM_DATABASE_DATA     = 0x10, // items.dat
        SEND_PARTICLE_EFFECT        = 0x11
    };
}
