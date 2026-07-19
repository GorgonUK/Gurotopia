#include "pch.hpp"
#include <fstream>
#include <cstdlib>

#include "server_config.hpp"

::server_config gServer_config{};

::server_config load_server_config()
{
    ::server_config cfg{};
    {
        std::ifstream file("server.cfg");
        if (!file.is_open())
        {
            std::ofstream write("server.cfg");
            write <<
                std::format(
                    /* @note this is read via std::getline() into readch(), pipe-delimited format */
                        "growth_speed|{}\n"
                        "gem_drop_multiplier|{}\n",
                        cfg.growth_speed,
                        cfg.gem_drop_multiplier
                );
        }
        else
        {
            for (std::string line; std::getline(file, line); )
            {
                if (line.empty() || line.front() == '#')
                    continue;

                auto pipe_pair = readch(line, '|');
                if (pipe_pair.size() < 2ull)
                    continue;

                if (pipe_pair[0] == "growth_speed")
                {
                    float value = std::strtof(pipe_pair[1].c_str(), nullptr);
                    if (value > 0.0f)
                        cfg.growth_speed = value;
                }
                else if (pipe_pair[0] == "gem_drop_multiplier")
                {
                    float value = std::strtof(pipe_pair[1].c_str(), nullptr);
                    if (value > 0.0f)
                        cfg.gem_drop_multiplier = value;
                }
            }
        }
    }
    return cfg;
}
