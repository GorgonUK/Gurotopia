#include "pch.hpp"
#include <fstream>

#include "database_config.hpp"

::database_config gDb_config{};

::database_config load_database_config()
{
    ::database_config db_config{};
    {
        std::ifstream file("database.cfg");
        if (!file.is_open())
        {
            std::ofstream write("database.cfg");
            write << 
                std::format(
                    /* @note this is read via std::getline() into readch(), pipe-delimited format */
                    "host|{}\n"
                    "user|{}\n"
                    "password|{}\n",
                    db_config.host, db_config.user, db_config.password
                );
        } // @note close write
        else
        {
            /* @note key-based, comment-tolerant parse (see server_config.cpp). The old
               positional pipes[1]/[3]/[5] indexing broke on any comment/blank line —
               including the ones in database.cfg.example — and silently set host="host". */
            for (std::string line; std::getline(file, line); ) 
            {
                if (!line.empty() && line.back() == '\r') line.pop_back(); // @note Windows CRLF on Linux
                if (line.empty() || line.front() == '#') continue;

                auto kv = readch(line, '|');
                if (kv.size() < 2ull) continue;

                if      (kv[0] == "host")     db_config.host     = kv[1];
                else if (kv[0] == "user")     db_config.user     = kv[1];
                else if (kv[0] == "password") db_config.password = kv[1];
            }
        } // @note delete pipes
    } // @note close file
    return db_config;
}