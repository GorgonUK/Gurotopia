#include "pch.hpp"
#include "on/ConsoleMessage.hpp"
#include "database/database.hpp"

#include <cstring>

#include "RequestWorldSelectMenu.hpp"

namespace
{
    struct ranked_world
    {
        std::string name;
        long long visits{};
    };

    std::vector<ranked_world> db_top_worlds()
    {
        std::vector<ranked_world> entries;
        ::hStmt stmt{
            "SELECT name, total_visits FROM world WHERE owner <> 0 "
            "ORDER BY total_visits DESC, name ASC LIMIT 24"
        };
        if (mysql_stmt_execute(stmt.pStmt) || mysql_stmt_store_result(stmt.pStmt))
            return entries;

        char name_buf[33]{};
        unsigned long name_len = 0;
        long long visits = 0;
        MYSQL_BIND results[2]{};
        results[0].buffer_type = MYSQL_TYPE_STRING;
        results[0].buffer = name_buf;
        results[0].buffer_length = sizeof(name_buf);
        results[0].length = &name_len;
        results[1] = make_bind_out(visits);
        if (mysql_stmt_bind_result(stmt.pStmt, results))
            return entries;

        while (mysql_stmt_fetch(stmt.pStmt) == 0)
        {
            if (name_len >= sizeof(name_buf)) name_len = sizeof(name_buf) - 1;
            entries.push_back({std::string{name_buf, name_len}, visits});
            std::memset(name_buf, 0, sizeof(name_buf));
            name_len = 0;
            visits = 0;
        }
        return entries;
    }

    std::vector<std::string> db_owned_worlds(int owner)
    {
        std::vector<std::string> names;
        ::hStmt stmt{
            "SELECT name FROM world WHERE owner = ? ORDER BY name ASC LIMIT 6"
        };
        MYSQL_BIND param = make_bind_in(owner);
        if (mysql_stmt_bind_param(stmt.pStmt, &param) ||
            mysql_stmt_execute(stmt.pStmt) || mysql_stmt_store_result(stmt.pStmt))
            return names;

        char name_buf[33]{};
        unsigned long name_len = 0;
        MYSQL_BIND result{};
        result.buffer_type = MYSQL_TYPE_STRING;
        result.buffer = name_buf;
        result.buffer_length = sizeof(name_buf);
        result.length = &name_len;
        if (mysql_stmt_bind_result(stmt.pStmt, &result))
            return names;

        while (names.size() < 6 && mysql_stmt_fetch(stmt.pStmt) == 0)
        {
            if (name_len >= sizeof(name_buf)) name_len = sizeof(name_buf) - 1;
            names.emplace_back(name_buf, name_len);
            std::memset(name_buf, 0, sizeof(name_buf));
            name_len = 0;
        }
        return names;
    }

    std::vector<std::string> db_category_worlds(u_char category)
    {
        std::vector<std::string> names;
        ::hStmt stmt{
            "SELECT name FROM world WHERE owner <> 0 AND category = ? "
            "ORDER BY total_visits DESC, name ASC LIMIT 5"
        };
        unsigned category_id = category;
        MYSQL_BIND param = make_bind_in(category_id);
        if (mysql_stmt_bind_param(stmt.pStmt, &param) ||
            mysql_stmt_execute(stmt.pStmt) || mysql_stmt_store_result(stmt.pStmt))
            return names;

        char name_buf[33]{};
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

    std::string recent_floaters(const ::peer &peer)
    {
        std::string result;
        for (const std::string &name : peer.recent_worlds)
            if (!name.empty())
                result.append(std::format("add_floater|{}|0|0.5|3417414143\n", name));
        return result;
    }
}

void on::RequestWorldSelectMenu(ENetEvent& event) 
{
    ::peer &peer = peer_of(event);
    
    auto section = [](const auto& range, const std::string &color)
    {
        std::string result;
        std::size_t count = 0;
        for (const auto &name : range)
            if (!name.empty()) 
            {
                auto world = std::ranges::find(worlds, name, &::world::name);
                result.append((world != worlds.end()) ?
                    std::format("add_floater|{}|{}|0.5|{}\n", name, world->visitors, color) :
                    std::format("add_floater|{}|0|0.5|{}\n", name, color));
                if (++count >= 6)
                    break;
            }
        return result;
    };

    // Rank by cumulative successful entries, not current online occupants.
    std::vector<ranked_world> ranked = db_top_worlds();
    for (const ::world &world : worlds)
    {
        if (world.owner == 0)
            continue;
        auto existing = std::ranges::find(ranked, world.name, &ranked_world::name);
        if (existing == ranked.end())
            ranked.push_back({world.name, world.total_visits});
        else
            existing->visits = world.total_visits;
    }
    std::ranges::sort(ranked, [](const ranked_world &left, const ranked_world &right) {
        return left.visits != right.visits ? left.visits > right.visits : left.name < right.name;
    });

    std::vector<std::string> popular_names;
    for (const ranked_world &entry : ranked)
    {
        popular_names.push_back(entry.name);
        if (popular_names.size() >= 6)
            break;
    }

    std::vector<std::string> owned_names = db_owned_worlds(peer.user_id);
    for (const ::world &world : worlds)
        if (world.owner == peer.user_id &&
            std::ranges::find(owned_names, world.name) == owned_names.end())
            owned_names.push_back(world.name);
    std::ranges::sort(owned_names);
    if (owned_names.size() > 6)
        owned_names.resize(6);

    send_varlist(event.peer, { 
        "OnRequestWorldSelectMenu", 
        std::format(
            "default|START\n"
            "add_filter|\n"
            "set_max_rows|6|\n"
            "add_heading|Top Worlds<ROW2>|\n{}{}"
            "add_heading|My Worlds<CR>|\n{}"
            "add_heading|Recently Visited Worlds<CR>|\n{}"
            "set_max_rows|-1|\n",
            "add_floater|wotd_world|\u013B WOTD|0|0.5|3529161471\n", 
            section(popular_names, "3529161471"), 
            section(owned_names, "2147418367"),
            section(peer.recent_worlds, "3417414143")
        ), 
        1
    });
    on::ConsoleMessage(event.peer, std::format("Where would you like to go? (`w{}`` online)", peers().size()));
}

void on::RequestWorldCategoryMenu(ENetEvent &event)
{
    send_varlist(event.peer, {
        "OnRequestWorldSelectMenu",
        "add_button|Default|_-5100|0.7|3529161471|\n"
        "add_button|Top Worlds|_0|0.7|3529161471|\n"
        "add_button|Random|_-5000|0.7|3529161471|\n"
        "add_button|Your Worlds|_16|0.7|3529161471|\n"
        "add_button|Favorite|_-4122|0.7|3529161471|\n"
        "add_button|Adventure|_1|0.7|3529161471|\n"
        "add_button|Art|_2|0.7|3529161471|\n"
        "add_button|Farm|_3|0.7|3529161471|\n"
        "add_button|Game|_4|0.7|3529161471|\n"
        "add_button|Guild|_13|0.7|3529161471|\n"
        "add_button|Information|_5|0.7|3529161471|\n"
        "add_button|Music|_15|0.7|3529161471|\n"
        "add_button|Parkour|_6|0.7|3529161471|\n"
        "add_button|Puzzle|_14|0.7|3529161471|\n"
        "add_button|Roleplay|_7|0.7|3529161471|\n"
        "add_button|Shop|_8|0.7|3529161471|\n"
        "add_button|Social|_9|0.7|3529161471|\n"
        "add_button|Story|_11|0.7|3529161471|\n"
        "add_button|Trade|_12|0.7|3529161471|\n"
    });
}

void on::RequestWorldCategory(ENetEvent &event, u_char category)
{
    const auto definition = std::ranges::find(
        WORLD_CATEGORIES, category, &::world_category::id
    );
    if (definition == WORLD_CATEGORIES.end() || category == 0)
        return;

    const std::vector<std::string> names = db_category_worlds(category);
    std::string ranked;
    for (std::size_t i = 0; i < names.size(); ++i)
        ranked.append(std::format(
            "add_floater|{}|-{}|0.5|3417414143\n", names[i], i + 1
        ));

    send_varlist(event.peer, {
        "OnRequestWorldSelectMenu",
        std::format(
            "default|START\n"
            "add_filter|\n"
            "add_heading|{} Worlds|\n"
            "set_max_rows|4|\n"
            "{}"
            "set_max_rows|-1|\n"
            "add_heading|Recently Visited Worlds<CR>|\n"
            "set_max_rows|6|\n"
            "{}"
            "set_max_rows|-1|\n",
            definition->name, ranked, recent_floaters(peer_of(event))
        )
    });
}
