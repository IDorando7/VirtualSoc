// storage.c
#include <sqlite3.h>
#include <time.h>
#include "storage.h"
#include "common.h"

sqlite3 *g_db = NULL;
pthread_mutex_t db_mutex;

int storage_init(const char *path)
{
    int rc = sqlite3_open(path, &g_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[storage] Cannot open DB: %s\n", sqlite3_errmsg(g_db));
        return -1;
    }

    const char *sql_posts =
        "CREATE TABLE IF NOT EXISTS posts ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  author_id  INTEGER NOT NULL,"
        "  visibility INTEGER NOT NULL,"
        "  content    TEXT    NOT NULL,"
        "  created_at INTEGER NOT NULL"
        ");";

    char *errmsg = NULL;
    rc = sqlite3_exec(g_db, sql_posts, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[storage] Cannot create posts table: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    return 0;
}

void storage_close(void)
{
    if (g_db)
    {
        sqlite3_close(g_db);
        g_db = NULL;
    }
}
