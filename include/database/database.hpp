#pragma once

#include <mysql/mysql.h>
#include <vector>

extern MYSQL *db;

extern void mysql_connect();

extern bool mysql_begin();
extern bool mysql_commit();
extern void mysql_rollback();

class hStmt
{
public:
    hStmt(const std::string &query);
   ~hStmt();

    hStmt           (const hStmt&) = delete;
    hStmt& operator=(const hStmt&) = delete;

    MYSQL_STMT *pStmt;
};

extern MYSQL_BIND make_bind_in(const signed &buffer);
extern MYSQL_BIND make_bind_in(const unsigned &buffer);
extern MYSQL_BIND make_bind_in(const long &buffer);
extern MYSQL_BIND make_bind_in(const long long &buffer);
extern MYSQL_BIND make_bind_in(const float &buffer);
extern MYSQL_BIND make_bind_in(const std::string &buffer);
extern MYSQL_BIND make_bind_in_blob(const std::vector<u_char> &buffer);

extern MYSQL_BIND make_bind_out(signed &buffer);
extern MYSQL_BIND make_bind_out(unsigned &buffer);
extern MYSQL_BIND make_bind_out(long &buffer);
extern MYSQL_BIND make_bind_out(long long &buffer);
extern MYSQL_BIND make_bind_out(float &buffer);
extern MYSQL_BIND make_bind_out(std::string &buffer);
extern MYSQL_BIND make_bind_out_blob(std::vector<u_char> &buffer, unsigned long &length);

/* resize buffer to the actual fetched length after mysql_stmt_fetch */
extern void trim_blob(std::vector<u_char> &buffer, unsigned long length);
