#include <sqlite3.h>
#include <time.h>
#include <pthread.h>
#include "storage.h"
#include "common.h"

sqlite3 *g_db = NULL;
pthread_mutex_t db_mutex;

int storage_init(const char *path)
{
    int rc = sqlite3_open(path, &g_db);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[storage] Cannot open DB: %s\n", sqlite3_errmsg(g_db));
        return -1;
    }

    if (pthread_mutex_init(&db_mutex, NULL) != 0)
    {
        fprintf(stderr, "[storage] Cannot init db_mutex\n");
        sqlite3_close(g_db);
        g_db = NULL;
        return -1;
    }

    char *errmsg = NULL;
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

    const char *sql_conversations =
        "CREATE TABLE IF NOT EXISTS conversations ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  title       TEXT NOT NULL,"
        "  is_group    INTEGER NOT NULL,"
        "  visibility  INTEGER NOT NULL,"
        "  created_by  INTEGER NOT NULL,"
        "  created_at  INTEGER NOT NULL"
        ");";

    rc = sqlite3_exec(g_db, sql_conversations, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[storage] Cannot create conversations table: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    const char *sql_members =
        "CREATE TABLE IF NOT EXISTS conversation_members ("
        "  conversation_id  INTEGER NOT NULL,"
        "  user_id          INTEGER NOT NULL,"
        "  is_admin         INTEGER NOT NULL DEFAULT 0,"
        "  joined_at        INTEGER NOT NULL,"
        "  PRIMARY KEY (conversation_id, user_id)"
        ");";

    rc = sqlite3_exec(g_db, sql_members, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[storage] Cannot create conversation_members table: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    const char *sql_messages =
        "CREATE TABLE IF NOT EXISTS messages ("
        "  id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  conversation_id INTEGER NOT NULL,"
        "  sender_id       INTEGER NOT NULL,"
        "  content         TEXT NOT NULL,"
        "  created_at      INTEGER NOT NULL"
        ");";

    rc = sqlite3_exec(g_db, sql_messages, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[storage] Cannot create messages table: %s\n", errmsg);
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
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[storage] Cannot create friends table: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    const char *sql_groups =
        "CREATE TABLE IF NOT EXISTS groups ("
        "  id        INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name      TEXT UNIQUE NOT NULL,"
        "  owner_id  INTEGER NOT NULL,"
        "  is_public INTEGER NOT NULL"
        ");";

    rc = sqlite3_exec(g_db, sql_groups, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[storage] Cannot create groups table: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    const char *sql_group_members =
        "CREATE TABLE IF NOT EXISTS group_members ("
        "  group_id INTEGER NOT NULL,"
        "  user_id  INTEGER NOT NULL,"
        "  role     INTEGER NOT NULL,"
        "  PRIMARY KEY (group_id, user_id)"
        ");";

    rc = sqlite3_exec(g_db, sql_group_members, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[storage] Cannot create group_members table: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    const char *sql_group_requests =
        "CREATE TABLE IF NOT EXISTS group_requests ("
        "  group_id INTEGER NOT NULL,"
        "  user_id  INTEGER NOT NULL,"
        "  PRIMARY KEY (group_id, user_id)"
        ");";

    rc = sqlite3_exec(g_db, sql_group_requests, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[storage] Cannot create group_requests table: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    const char *sql_group_messages =
        "CREATE TABLE IF NOT EXISTS group_messages ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  group_id   INTEGER NOT NULL,"
        "  sender_id  INTEGER NOT NULL,"
        "  content    TEXT    NOT NULL,"
        "  created_at INTEGER NOT NULL"
        ");";

    rc = sqlite3_exec(g_db, sql_group_messages, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[storage] Cannot create group_messages table: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    const char *sql_notifications =
        "CREATE TABLE IF NOT EXISTS notifications ("
        "id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "user_id    INTEGER NOT NULL,"
        "type       TEXT    NOT NULL,"
        "payload    TEXT    NOT NULL,"
        "created_at INTEGER NOT NULL,"
        "deleted    INTEGER NOT NULL DEFAULT 0,"
        "FOREIGN KEY(user_id) REFERENCES users(id)"
        ");";

    rc = sqlite3_exec(g_db, sql_notifications, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[storage] Cannot create notifications table: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    const char *sql_friend_requests =
    "CREATE TABLE IF NOT EXISTS friend_requests ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  from_id INTEGER NOT NULL,"
    "  to_id INTEGER NOT NULL,"
    "  created_at INTEGER NOT NULL,"
    "  UNIQUE(from_id, to_id)"
    ");";

    rc = sqlite3_exec(g_db, sql_friend_requests, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[storage] Cannot create friend_requests table: %s\n", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }

    printf("[storage] Database initialized successfully.\n");
    return 0;
}

void storage_close(void)
{
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
    pthread_mutex_destroy(&db_mutex);
}
