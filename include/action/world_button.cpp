#include "pch.hpp"
#include "on/RequestWorldSelectMenu.hpp"
#include "on/ConsoleMessage.hpp"
#include "database/database.hpp"
#include "join_request.hpp"

#include <cstring>

#include "world_button.hpp"

namespace
{
    std::string random_owned_world()
    {
        ::hStmt stmt{
            "SELECT name FROM world WHERE owner <> 0 ORDER BY RAND() LIMIT 1"
        };
        if (mysql_stmt_execute(stmt.pStmt) || mysql_stmt_store_result(stmt.pStmt))
            return {};

        char name_buf[33]{};
        unsigned long name_len = 0;
        MYSQL_BIND result{};
        result.buffer_type = MYSQL_TYPE_STRING;
        result.buffer = name_buf;
        result.buffer_length = sizeof(name_buf);
        result.length = &name_len;
        if (mysql_stmt_bind_result(stmt.pStmt, &result))
            return {};
        if (mysql_stmt_fetch(stmt.pStmt) != 0)
            return {};
        if (name_len >= sizeof(name_buf))
            name_len = sizeof(name_buf) - 1;
        return {name_buf, name_len};
    }

    void show_owned_worlds(ENetEvent &event)
    {
        ::peer &peer = peer_of(event);
        std::vector<std::string> names;
        for (const ::world &world : worlds)
            if (world.owner == peer.user_id)
                names.push_back(world.name);

        {
            ::hStmt stmt{
                "SELECT name FROM world WHERE owner = ? ORDER BY name ASC LIMIT 32"
            };
            MYSQL_BIND param = make_bind_in(peer.user_id);
            if (!mysql_stmt_bind_param(stmt.pStmt, &param) &&
                !mysql_stmt_execute(stmt.pStmt) &&
                !mysql_stmt_store_result(stmt.pStmt))
            {
                char name_buf[33]{};
                unsigned long name_len = 0;
                MYSQL_BIND result{};
                result.buffer_type = MYSQL_TYPE_STRING;
                result.buffer = name_buf;
                result.buffer_length = sizeof(name_buf);
                result.length = &name_len;
                if (!mysql_stmt_bind_result(stmt.pStmt, &result))
                {
                    while (mysql_stmt_fetch(stmt.pStmt) == 0)
                    {
                        if (name_len >= sizeof(name_buf))
                            name_len = sizeof(name_buf) - 1;
                        const std::string name{name_buf, name_len};
                        if (std::ranges::find(names, name) == names.end())
                            names.push_back(name);
                        ::memset(name_buf, 0, sizeof(name_buf));
                        name_len = 0;
                    }
                }
            }
        }

        std::ranges::sort(names);
        std::string owned;
        for (std::size_t i = 0; i < names.size() && i < 6; ++i)
            owned.append(std::format(
                "add_floater|{}|-{}|0.5|2147418367\n", names[i], i + 1
            ));

        std::string recent;
        for (const std::string &name : peer.recent_worlds)
            if (!name.empty())
                recent.append(std::format("add_floater|{}|0|0.5|3417414143\n", name));

        send_varlist(event.peer, {
            "OnRequestWorldSelectMenu",
            std::format(
                "default|START\n"
                "add_filter|\n"
                "add_heading|Your Worlds|\n"
                "set_max_rows|4|\n"
                "{}"
                "set_max_rows|-1|\n"
                "add_heading|Recently Visited Worlds<CR>|\n"
                "set_max_rows|6|\n"
                "{}"
                "set_max_rows|-1|\n",
                owned, recent
            )
        });
    }
}

void action::world_button(ENetEvent &event, const std::string &header)
{
    // Official category / shortcut buttons use IDs like `_3` (Farm).
    // Filter World Categories sends name|_catselect_.
    ::hPipe pipe{header};
    const std::string &name = pipe["name"];

    if (name.empty() || name == "filter" || name == "_filter" || name == "_catselect_")
    {
        on::RequestWorldCategoryMenu(event);
        return;
    }
    if (name == "_-5100" || name == "_0")
    {
        on::RequestWorldSelectMenu(event);
        return;
    }
    if (name == "_-5000")
    {
        const std::string world = random_owned_world();
        if (world.empty())
        {
            on::ConsoleMessage(event.peer, "`4No worlds available for Random.``");
            on::RequestWorldSelectMenu(event);
            return;
        }
        action::join_request(event, "", world);
        return;
    }
    if (name == "_16")
    {
        show_owned_worlds(event);
        return;
    }
    if (name == "_-4122")
    {
        // Favorite worlds need a separate capture/storage; show an empty shell for now.
        std::string recent;
        for (const std::string &world : peer_of(event).recent_worlds)
            if (!world.empty())
                recent.append(std::format("add_floater|{}|0|0.5|3417414143\n", world));

        send_varlist(event.peer, {
            "OnRequestWorldSelectMenu",
            std::format(
                "default|START\n"
                "add_filter|\n"
                "add_heading|Favorite Worlds|\n"
                "set_max_rows|4|\n"
                "set_max_rows|-1|\n"
                "add_heading|Recently Visited Worlds<CR>|\n"
                "set_max_rows|6|\n"
                "{}"
                "set_max_rows|-1|\n",
                recent
            )
        });
        return;
    }

    if (name.size() >= 2 && name.front() == '_')
    {
        const int category = atoi(name.c_str() + 1);
        if (category >= 1 && category <= 15)
        {
            on::RequestWorldCategory(event, static_cast<u_char>(category));
            return;
        }
    }
}
