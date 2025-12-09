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

    char *errmsg = NULL;

    /* ----------------------------------------------------
       1) Tabela POSTS
       ---------------------------------------------------- */
    const char *sql_posts =
        "CREATE TABLE IF NOT EXISTS posts ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  author_id  INTEGER NOT NULL,"
        "  visibility INTEGER NOT NULL,"
        "  content    TEXT    NOT NULL,"
        "  created_at INTEGER NOT NULL"
        ");";

    rc = sqlite3_exec(g_db, sql_posts, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[storage] Cannot create posts table: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    /* ----------------------------------------------------
       2) Tabela CONVERSATIONS (DM + grupuri)
       ---------------------------------------------------- */
    const char *sql_conversations =
        "CREATE TABLE IF NOT EXISTS conversations ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  title       TEXT NOT NULL,"              /* nume grup sau "" pentru DM */
        "  is_group    INTEGER NOT NULL,"          /* 0 = DM, 1 = group */
        "  visibility  INTEGER NOT NULL,"          /* doar pentru grupuri: 0=public,1=private */
        "  created_by  INTEGER NOT NULL,"
        "  created_at  INTEGER NOT NULL"
        ");";

    rc = sqlite3_exec(g_db, sql_conversations, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[storage] Cannot create conversations: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    /* ----------------------------------------------------
       3) Tabela CONVERSATION_MEMBERS (membri DM & grup)
       ---------------------------------------------------- */
    const char *sql_members =
        "CREATE TABLE IF NOT EXISTS conversation_members ("
        "  conversation_id  INTEGER NOT NULL,"
        "  user_id          INTEGER NOT NULL,"
        "  is_admin         INTEGER NOT NULL DEFAULT 0,"
        "  joined_at        INTEGER NOT NULL,"
        "  PRIMARY KEY (conversation_id, user_id)"
        ");";

    rc = sqlite3_exec(g_db, sql_members, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[storage] Cannot create conversation_members: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    /* ----------------------------------------------------
       4) Tabela GROUP_JOIN_REQUESTS (doar pentru grupuri private)
       ---------------------------------------------------- */
    const char *sql_join_requests =
        "CREATE TABLE IF NOT EXISTS group_join_requests ("
        "  conversation_id  INTEGER NOT NULL,"
        "  user_id          INTEGER NOT NULL,"
        "  requested_at     INTEGER NOT NULL,"
        "  PRIMARY KEY (conversation_id, user_id)"
        ");";

    rc = sqlite3_exec(g_db, sql_join_requests, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[storage] Cannot create join_requests: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    /* ----------------------------------------------------
       5) Tabela MESSAGES (DM și GRUP)
       ---------------------------------------------------- */
    const char *sql_messages =
        "CREATE TABLE IF NOT EXISTS messages ("
        "  id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  conversation_id INTEGER NOT NULL,"
        "  sender_id       INTEGER NOT NULL,"
        "  content         TEXT NOT NULL,"
        "  created_at      INTEGER NOT NULL"
        ");";

    rc = sqlite3_exec(g_db, sql_messages, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[storage] Cannot create messages: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    const char *sql_friends =
        "CREATE TABLE IF NOT EXISTS friends ("
        "  user_id   INTEGER NOT NULL,"
        "  friend_id INTEGER NOT NULL,"
        "  type      INTEGER NOT NULL,"
        "  PRIMARY KEY (user_id, friend_id)"
        ");";

    rc = sqlite3_exec(g_db, sql_friends, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[storage] Cannot create friends: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    /* ----------------------------------------------------
       TOTUL E OK
       ---------------------------------------------------- */
    printf("[storage] Database initialized successfully.\n");
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
