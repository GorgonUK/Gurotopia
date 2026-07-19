#include "pch.hpp"
#include "on/ConsoleMessage.hpp"
#include "database/database.hpp"

#include <cstring>

#include "RequestWorldSelectMenu.hpp"

namespace
{
    std::vector<std::string> db_active_worlds(std::size_t limit)
    {
        std::vector<std::string> names;
        ::hStmt stmt{
            "SELECT name FROM world WHERE owner <> 0 ORDER BY name ASC LIMIT ?"
        };
        unsigned lim = static_cast<unsigned>(limit);
        MYSQL_BIND param = make_bind_in(lim);
        if (mysql_stmt_bind_param(stmt.pStmt, &param))
            return names;
        if (mysql_stmt_execute(stmt.pStmt) || mysql_stmt_store_result(stmt.pStmt))
            return names;

        char name_buf[32]{};
        unsigned long name_len = 0;
        MYSQL_BIND result{};
        result.buffer_type = MYSQL_TYPE_STRING;
        result.buffer = name_buf;
        result.buffer_length = sizeof(name_buf);
        result.length = &name_len;
        if (mysql_stmt_bind_result(stmt.pStmt, &result))
            return names;

        while (mysql_stmt_fetch(stmt.pStmt) == 0)
        {
            if (name_len >= sizeof(name_buf)) name_len = sizeof(name_buf) - 1;
            names.emplace_back(name_buf, name_len);
            std::memset(name_buf, 0, sizeof(name_buf));
            name_len = 0;
        }
        return names;
    }
}

void on::RequestWorldSelectMenu(ENetEvent& event) 
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    
    auto section = [](const auto& range, const std::string &color) 
    {
        std::string result;
        for (const auto &name : range) 
            if (!name.empty()) 
            {
                auto world = std::ranges::find(worlds, name, &::world::name);
                result.append((world != worlds.end()) ? 
                    std::format("add_floater|{}|{}|0.5|{}\n", name, world->visitors, color) :
                    std::format("add_floater|{}|0|0.5|{}\n", name, color));
            }
        return result;
    };

    // top populated worlds currently in memory, sorted by visitors
    std::vector<std::pair<u_char, std::string>> populated;
    for (const ::world &world : worlds)
        if (world.visitors > 0)
            populated.emplace_back(world.visitors, world.name);
    std::ranges::sort(populated, std::greater<>{}, &std::pair<u_char, std::string>::first);

    std::vector<std::string> popular_names;
    for (std::size_t i = 0; i < populated.size() && i < 12ull; ++i)
        popular_names.push_back(populated[i].second);

    // fill remaining slots from locked worlds in DB so the list isn't empty offline
    if (popular_names.size() < 12ull)
    {
        for (const std::string &name : db_active_worlds(24))
        {
            if (std::ranges::find(popular_names, name) != popular_names.end()) continue;
            popular_names.push_back(name);
            if (popular_names.size() >= 12ull) break;
        }
    }

    send_varlist(event.peer, { 
        "OnRequestWorldSelectMenu", 
        std::format(
            "add_filter|\n"
            "add_heading|Top Worlds<ROW2>|\n{}{}"
            "add_heading|My Worlds<CR>|\n{}"
            "add_heading|Recently Visited Worlds<CR>|\n{}",
            "add_floater|wotd_world|\u013B WOTD|0|0.5|3529161471\n", 
            section(popular_names, "3529161471"), 
            section(pPeer->my_worlds, "2147418367"), 
            section(pPeer->recent_worlds, "3417414143")
        ), 
        1
    });
    on::ConsoleMessage(event.peer, std::format("Where would you like to go? (`w{}`` online)", peers().size()));
}
