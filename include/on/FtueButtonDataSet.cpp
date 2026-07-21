#include "pch.hpp"
#include "FtueButtonDataSet.hpp"

#include <fstream>

namespace
{
    constexpr std::size_t k_ftue_payload_size = 3004;
    constexpr char k_ftue_path[] = "resources/ftue/tutorials.msgpack";

    const std::string &ftue_payload()
    {
        static const std::string payload = []() -> std::string {
            std::ifstream file(k_ftue_path, std::ios::binary | std::ios::ate);
            if (!file)
            {
                fprintf(stderr, "[ftue] missing %s\n", k_ftue_path);
                return {};
            }

            const auto size = static_cast<std::size_t>(file.tellg());
            if (size != k_ftue_payload_size)
            {
                fprintf(stderr, "[ftue] unexpected size %zu (want %zu)\n", size, k_ftue_payload_size);
                return {};
            }

            file.seekg(0, std::ios::beg);
            std::string bytes(size, '\0');
            if (!file.read(bytes.data(), static_cast<std::streamsize>(size)))
            {
                fprintf(stderr, "[ftue] failed to read %s\n", k_ftue_path);
                return {};
            }

            // MessagePack: fixarray(7), true, true, false, -1
            const unsigned char *raw = reinterpret_cast<const unsigned char *>(bytes.data());
            if (raw[0] != 0x97 || raw[1] != 0xc3 || raw[2] != 0xc3 || raw[3] != 0xc2 || raw[4] != 0xff)
            {
                fprintf(stderr, "[ftue] invalid MessagePack header\n");
                return {};
            }

            return bytes;
        }();
        return payload;
    }
}

void on::FtueButtonDataSet(ENetEvent &event)
{
    const std::string &payload = ftue_payload();
    if (payload.empty())
        return;

    send_varlist(event.peer, { "OnFtueButtonDataSet", payload });
}
