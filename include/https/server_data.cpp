#include "pch.hpp"
#include <fstream>
#include <cstdlib>

#include "server_data.hpp"

::server_data gServer_data{};

::server_data init_server_data()
{
    ::server_data server_data{};
    {
        std::ifstream file("server_data.php");
        if (!file.is_open())
        {
            std::ofstream write("server_data.php");
            write << 
                std::format(
                    "server|{}\n"
                    "port|{}\n"
                    "type|{}\n"
                    "type2|{}\n"
                    "#maint|{}\n"
                    "loginurl|{}\n"
                    "meta|{}\n"
                    "RTENDMARKERBS1001", 
                    server_data.server, server_data.port, server_data.type, server_data.type2, server_data.maint, server_data.loginurl, server_data.meta
                );
        } // @note close write
        else
        {
            /* @note key-based, comment-tolerant parse. The old positional pipes[1..13]
               indexing threw std::out_of_range / std::stoi on any comment or missing line
               (e.g. the header comments in server_data.php.example) and killed startup.
               Unset keys keep their defaults from ::server_data. The 'maint' line is a
               comment (#maint|…) by convention, so maintenance text stays the default. */
            for (std::string line; std::getline(file, line); ) 
            {
                if (!line.empty() && line.back() == '\r') line.pop_back(); // @note Windows CRLF on Linux
                if (line.empty() || line.front() == '#') continue;

                auto kv = readch(line, '|');
                if (kv.size() < 2ull) continue;

                if      (kv[0] == "server")   server_data.server   = kv[1];
                else if (kv[0] == "port")     { int v = std::atoi(kv[1].c_str()); if (v > 0 && v <= 65535) server_data.port = static_cast<u_short>(v); }
                else if (kv[0] == "type")     server_data.type     = static_cast<u_char>(std::atoi(kv[1].c_str()));
                else if (kv[0] == "type2")    server_data.type2    = static_cast<u_char>(std::atoi(kv[1].c_str()));
                else if (kv[0] == "maint")    server_data.maint    = kv[1];
                else if (kv[0] == "loginurl") server_data.loginurl = kv[1];
                else if (kv[0] == "meta")     server_data.meta     = kv[1];
            }
        } // @note delete str, pipes
    } // @note close file
    return server_data;
}