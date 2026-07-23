/*
    @copyright gurotopia (c) 2024-05-25
    @version parent SHA: 724e9771acd6c3f56bb9ec5b274683738d3edd05 2026-7-18
*/
#include "include/pch.hpp"
#include "include/event_type/__event_type.hpp"

#include "include/database/shouhin.hpp" // @note init_shouhin_tachi()
#include "include/https/https.hpp" // @note https::listener()
#include "include/https/server_data.hpp" // @note gServer_data
#include "include/database/database.hpp" // @note mysql_connect()
#include "include/database/database_config.hpp" // @note load_database_config(), gDatabase_config
#include "include/database/server_config.hpp" // @note load_server_config(), gServer_config
#include "include/automate/holiday.hpp" // @note holiday
#include "include/state/fishing.hpp" // @note fishing_tick()
#include <csignal>

namespace
{
    volatile std::sig_atomic_t gSignal = 0;
}
static void signal_handler(int signal) { gSignal = signal; }

int main()
{
    /* !! please press Ctrl + C when restarting or stopping server !! */
    std::signal(SIGINT, signal_handler);
#ifdef SIGHUP // @note unix
    std::signal(SIGHUP, signal_handler); // @note PuTTY, SSH problems
#endif

    /* libary version checker */
    std::printf("ZTzTopia/enet %d.%d.%d\n", ENET_VERSION_MAJOR, ENET_VERSION_MINOR, ENET_VERSION_PATCH);
    std::printf("openssl/openssl %s\n", OpenSSL_version(OPENSSL_VERSION_STRING));

    enet_initialize();
    {
        gServer_data = init_server_data();
        ENetAddress address{
            .type = ENET_ADDRESS_TYPE_IPV4, 
            .port = gServer_data.port
        };

        host = enet_host_create (ENET_ADDRESS_TYPE_IPV4, &address, 50ull/* max peer count */, 2ull, 0, 0);
        if (!host)
        {
            std::fprintf(stderr, "failed to create ENet host on port %hu (already in use?)\n", gServer_data.port);
            enet_deinitialize();
            return EXIT_FAILURE;
        }
        std::thread(&https::listener).detach();
    } // @note delete address
    host->usingNewPacketForServer = true;
    host->checksum = enet_crc32;
    enet_host_compress_with_range_coder(host);

    gDb_config = load_database_config();
    gServer_config = load_server_config();
    printf("growth_speed=%.2fx\n", gServer_config.growth_speed);
    mysql_connect();
    decode_items();      // @note reads items.dat into legible class members (id, item name, ect)
    parse_store();       // @todo thread loop this so the store can update without restarting server (stored in .\resource\store.txt)
    check_for_holiday(); // @note check for any holidays using local time (your VPS or local time) - @todo thread loop so it can change the holiday without restarting

    ENetEvent event{};
    auto last_autosave = std::chrono::steady_clock::now();
    while (!gSignal)
    {
        while (enet_host_service(host, &event, 1000/*ms*/) > 0)
            if (const auto i = event_pool.find(event.type); i != event_pool.end())
                i->second(event);

        fishing_tick(); // @note splash rolls / bite-window expiry (~1s when idle)

        if (std::chrono::steady_clock::now() - last_autosave >= std::chrono::seconds(30))
        {
            autosave_peers();
            autosave_worlds();
            last_autosave = std::chrono::steady_clock::now();
        }
    }

    safe_disconnect_peers(gSignal);
    mysql_close(db);
    return 0;
}