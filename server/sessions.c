#include <stdio.h>
#include <pthread.h>
#include <sqlite3.h>

#include "sessions.h"
#include "storage.h"

int sessions_init()
{
    const char *sql_create =
        "CREATE TABLE IF NOT EXISTS sessions ("
        "  id        INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  client_fd INTEGER UNIQUE NOT NULL,"
        "  user_id   INTEGER NOT NULL"
        ");";

    const char *sql_clear =
        "DELETE FROM sessions;";

    char *errmsg = NULL;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_exec(g_db, sql_create, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[sessions] Cannot create sessions table: %s\n", errmsg);
        sqlite3_free(errmsg);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    rc = sqlite3_exec(g_db, sql_clear, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[sessions] Cannot clear sessions: %s\n", errmsg);
        sqlite3_free(errmsg);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    pthread_mutex_unlock(&db_mutex);
    return 0;
}

int sessions_set(int client_fd, int user_id)
{
    const char *sql =
        "INSERT INTO sessions(client_fd, user_id) "
        "VALUES (?, ?) "
        "ON CONFLICT(client_fd) DO UPDATE SET user_id = excluded.user_id;";

    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&db_mutex);
    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[sessions] prepare upsert failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, client_fd);
    sqlite3_bind_int(stmt, 2, user_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[sessions] upsert failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return 0;
}

int sessions_clear(int client_fd)
{
    const char *sql =
        "DELETE FROM sessions WHERE client_fd = ?;";

    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[sessions] prepare delete failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, client_fd);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[sessions] delete failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return 0;
}

int sessions_get_user_id(int client_fd)
{
    const char *sql =
        "SELECT user_id FROM sessions WHERE client_fd = ? LIMIT 1;";

    sqlite3_stmt *stmt;
    int rc;
    int user_id = -1;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[sessions] prepare get user_id failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, client_fd);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        user_id = sqlite3_column_int(stmt, 0);
    } else if (rc != SQLITE_DONE) {
        fprintf(stderr, "[sessions] get user_id error: %s\n", sqlite3_errmsg(g_db));
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return user_id;
}

int sessions_find_fd_by_user_id(int user_id)
{
    const char *sql =
        "SELECT client_fd FROM sessions WHERE user_id = ? LIMIT 1;";

    sqlite3_stmt *stmt;
    int rc;
    int client_fd = -1;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[sessions] prepare find fd failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, user_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        client_fd = sqlite3_column_int(stmt, 0);
    } else if (rc != SQLITE_DONE) {
        fprintf(stderr, "[sessions] find fd error: %s\n", sqlite3_errmsg(g_db));
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return client_fd;
}
