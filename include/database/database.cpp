#include "pch.hpp"

#include "database.hpp"
#include "database_config.hpp"

MYSQL *db;

static void run_query(const char *query, bool silent_dup = false)
{
    if (mysql_query(db, query))
    {
        unsigned err = mysql_errno(db);
        // 1050 = table exists, 1060 = duplicate column, 1061 = duplicate key name
        if (silent_dup && (err == 1050 || err == 1060 || err == 1061))
            return;
        fprintf(stderr, "[sql] %s\n", mysql_error(db));
    }
}

void create_table_if_not_exist()
{
    run_query(R"(
        CREATE TABLE IF NOT EXISTS peer (
            uid INT AUTO_INCREMENT PRIMARY KEY,
            growid VARCHAR(18) UNIQUE,
            password VARCHAR(128),
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )");

    // migration: widen password column for PBKDF2 hashes (was VARCHAR(18))
    if (mysql_query(db, "ALTER TABLE peer MODIFY COLUMN password VARCHAR(128)"))
    {
        if (mysql_errno(db) != 1054)
            fprintf(stderr, "[migrate] %s\n", mysql_error(db));
    }

    run_query(R"(
        CREATE TABLE IF NOT EXISTS peer_state (
            uid INT PRIMARY KEY,
            gems INT NOT NULL DEFAULT 0,
            level SMALLINT UNSIGNED NOT NULL DEFAULT 1,
            xp SMALLINT UNSIGNED NOT NULL DEFAULT 0,
            slot_size SMALLINT NOT NULL DEFAULT 16,
            clothing VARBINARY(40) NOT NULL,
            fav BLOB NOT NULL,
            role TINYINT UNSIGNED NOT NULL DEFAULT 0,
            skin_color INT UNSIGNED NOT NULL DEFAULT 2527912447,
            hair_color INT UNSIGNED NOT NULL DEFAULT 16777215,
            country VARCHAR(8) NOT NULL DEFAULT '',
            fires_removed SMALLINT UNSIGNED NOT NULL DEFAULT 0,
            gbc_pity SMALLINT UNSIGNED NOT NULL DEFAULT 0,
            initialized TINYINT NOT NULL DEFAULT 0,
            recent_worlds BLOB NOT NULL,
            my_worlds BLOB NOT NULL,
            achievements BLOB NOT NULL,
            quest BLOB NOT NULL,
            CONSTRAINT fk_peer_state_uid FOREIGN KEY (uid) REFERENCES peer(uid) ON DELETE CASCADE
        )
    )");

    // migration: add recent/my worlds columns to pre-existing peer_state tables
    run_query("ALTER TABLE peer_state ADD COLUMN recent_worlds BLOB NOT NULL", /*silent_dup=*/true);
    run_query("ALTER TABLE peer_state ADD COLUMN my_worlds BLOB NOT NULL", /*silent_dup=*/true);

    // migration: achievement counters (u_int per ach::, see database/achievements.hpp)
    run_query("ALTER TABLE peer_state ADD COLUMN achievements BLOB NOT NULL", /*silent_dup=*/true);

    // migration: active daily quest {goal, progress, target, reward_gems} as 4 u_ints
    run_query("ALTER TABLE peer_state ADD COLUMN quest BLOB NOT NULL", /*silent_dup=*/true);

    run_query(R"(
        CREATE TABLE IF NOT EXISTS peer_inventory (
            uid INT NOT NULL,
            item_id SMALLINT NOT NULL,
            count SMALLINT NOT NULL,
            PRIMARY KEY (uid, item_id),
            CONSTRAINT fk_peer_inv_uid FOREIGN KEY (uid) REFERENCES peer(uid) ON DELETE CASCADE
        )
    )");

    run_query(R"(
        CREATE TABLE IF NOT EXISTS world (
            name VARCHAR(32) PRIMARY KEY,
            owner INT NOT NULL DEFAULT 0,
            is_public TINYINT NOT NULL DEFAULT 0,
            lock_state TINYINT UNSIGNED NOT NULL DEFAULT 0,
            minimum_entry_level TINYINT UNSIGNED NOT NULL DEFAULT 1,
            weather_x FLOAT NOT NULL DEFAULT 0,
            weather_y FLOAT NOT NULL DEFAULT 0,
            last_object_uid INT UNSIGNED NOT NULL DEFAULT 0,
            data MEDIUMBLOB NOT NULL,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
        )
    )");
}

bool mysql_begin()
{
    if (mysql_query(db, "START TRANSACTION"))
    {
        fprintf(stderr, "[sql] begin: %s\n", mysql_error(db));
        return false;
    }
    return true;
}

bool mysql_commit()
{
    if (mysql_query(db, "COMMIT"))
    {
        fprintf(stderr, "[sql] commit: %s\n", mysql_error(db));
        return false;
    }
    return true;
}

void mysql_rollback()
{
    if (mysql_query(db, "ROLLBACK"))
        fprintf(stderr, "[sql] rollback: %s\n", mysql_error(db));
}

void mysql_connect()
{
    db = mysql_init(NULL);

    if (!mysql_real_connect(db, gDb_config.host.c_str(), gDb_config.user.c_str(), (gDb_config.password.empty()) ? NULL : gDb_config.password.c_str(), NULL, 3306u, NULL, 0u)) 
    {
        fprintf(stderr, "%s\n", mysql_error(db));
    }
    else printf("connected to MariaDB server on %s:%d\n", db->host, db->port);

    mysql_query(db, "CREATE DATABASE IF NOT EXISTS gurotopia");
    mysql_select_db(db, "gurotopia");

    create_table_if_not_exist();
}

hStmt::hStmt(const std::string &query)
{
    this->pStmt = mysql_stmt_init(db);
    if (!pStmt) 
    {
        fprintf(stderr, "%s\n", mysql_error(db));
    }
    if (mysql_stmt_prepare(pStmt, query.c_str(), (u_long)query.size()))
    {
        fprintf(stderr, "%s\n", mysql_error(db));
    }
}
hStmt::~hStmt() 
{
    if (mysql_stmt_close(pStmt))
    {
        fprintf(stderr, "%s\n", mysql_error(db));
    }
}


MYSQL_BIND make_bind_in(const signed &buffer)
{
    return { .buffer = (void*)&buffer, .buffer_type = MYSQL_TYPE_LONG };
}
MYSQL_BIND make_bind_in(const unsigned &buffer)
{
    return { .buffer = (void*)&buffer, .buffer_type = MYSQL_TYPE_LONG, .is_unsigned = true };
}
MYSQL_BIND make_bind_in(const long &buffer)
{
    return { .buffer = (void*)&buffer, .buffer_type = MYSQL_TYPE_LONG };
}
MYSQL_BIND make_bind_in(const long long &buffer)
{
    return { .buffer = (void*)&buffer, .buffer_type = MYSQL_TYPE_LONGLONG };
}
MYSQL_BIND make_bind_in(const float &buffer)
{
    return { .buffer = (void*)&buffer, .buffer_type = MYSQL_TYPE_FLOAT };
}
MYSQL_BIND make_bind_in(const std::string &buffer)
{
    return { .buffer = (void*)buffer.c_str(), .buffer_length = (u_long)buffer.size(), .buffer_type = MYSQL_TYPE_STRING };
}
MYSQL_BIND make_bind_in_blob(const std::vector<u_char> &buffer)
{
    static u_char empty_blob = 0; // @note non-null so MariaDB stores '' not NULL

    MYSQL_BIND bind{};
    bind.buffer = buffer.empty() ? (void*)&empty_blob : (void*)buffer.data();
    bind.buffer_length = (u_long)buffer.size();
    bind.buffer_type = MYSQL_TYPE_BLOB;
    return bind;
}

MYSQL_BIND make_bind_out(signed &buffer)
{
    return { .buffer = &buffer, .buffer_type = MYSQL_TYPE_LONG };
}
MYSQL_BIND make_bind_out(unsigned &buffer)
{
    return { .buffer = &buffer, .buffer_type = MYSQL_TYPE_LONG, .is_unsigned = true };
}
MYSQL_BIND make_bind_out(long &buffer)
{
    return { .buffer = &buffer, .buffer_type = MYSQL_TYPE_LONG };
}
MYSQL_BIND make_bind_out(long long &buffer)
{
    return { .buffer = &buffer, .buffer_type = MYSQL_TYPE_LONGLONG };
}
MYSQL_BIND make_bind_out(float &buffer)
{
    return { .buffer = &buffer, .buffer_type = MYSQL_TYPE_FLOAT };
}
MYSQL_BIND make_bind_out(std::string &buffer)
{
    buffer.resize(1024, '\0');

    return { .buffer = buffer.data(), .buffer_length = (u_long)buffer.size(), .buffer_type = MYSQL_TYPE_STRING };
}
MYSQL_BIND make_bind_out_blob(std::vector<u_char> &buffer, unsigned long &length)
{
    if (buffer.empty())
        buffer.resize(1 << 20); // 1 MiB default; worlds fit comfortably

    MYSQL_BIND bind{};
    bind.buffer = buffer.data();
    bind.buffer_length = (u_long)buffer.size();
    bind.length = &length;
    bind.buffer_type = MYSQL_TYPE_BLOB;
    return bind;
}

void trim_blob(std::vector<u_char> &buffer, unsigned long length)
{
    if (length > buffer.size())
    {
        fprintf(stderr, "[sql] blob truncated: need %lu have %zu\n", length, buffer.size());
        return;
    }
    buffer.resize(length);
}
