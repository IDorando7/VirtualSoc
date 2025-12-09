#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sqlite3.h>

#include "friends.h"
#include "storage.h"   // pentru g_db și db_mutex
#include "auth.h"

/*
 * Schema tabelă friends (folosită aici):
 *
 * CREATE TABLE IF NOT EXISTS friends (
 *   user_id   INTEGER NOT NULL,
 *   friend_id INTEGER NOT NULL,
 *   type      INTEGER NOT NULL,   -- 0=FRIEND_CLOSE, 1=FRIEND_NORMAL
 *   PRIMARY KEY (user_id, friend_id)
 * );
 */

// int friends_init(void)
// {
//     const char *sql =
//         "CREATE TABLE IF NOT EXISTS friends ("
//         "  user_id   INTEGER NOT NULL,"
//         "  friend_id INTEGER NOT NULL,"
//         "  type      INTEGER NOT NULL,"
//         "  PRIMARY KEY (user_id, friend_id)"
//         ");";
//
//     char *errmsg = NULL;
//
//     pthread_mutex_lock(&db_mutex);
//
//     int rc = sqlite3_exec(g_db, sql, NULL, NULL, &errmsg);
//     if (rc != SQLITE_OK) {
//         fprintf(stderr, "[friends] Cannot create table: %s\n", errmsg);
//         sqlite3_free(errmsg);
//         pthread_mutex_unlock(&db_mutex);
//         return -1;
//     }
//
//     pthread_mutex_unlock(&db_mutex);
//     return 0;
// }

/*
 * Funcție internă: inserează/actualizează un singur sens (user_id -> friend_id).
 */
static int friends_upsert_one(int user_id, int friend_id, enum friend_type type)
{
    const char *sql =
        "INSERT INTO friends(user_id, friend_id, type) "
        "VALUES (?, ?, ?) "
        "ON CONFLICT(user_id, friend_id) "
        "DO UPDATE SET type = excluded.type;";

    sqlite3_stmt *stmt = NULL;

    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[friends] prepare upsert failed: %s\n", sqlite3_errmsg(g_db));
        return -1;
    }

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, friend_id);
    sqlite3_bind_int(stmt, 3, (int)type);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[friends] upsert failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        return -1;
    }

    sqlite3_finalize(stmt);
    return 0;
}

int friends_add(int user_id, int friend_id, enum friend_type type)
{
    if (user_id == friend_id) {
        // opțional: nu permiți prietenie cu tine însuți
        return 0;
    }

    pthread_mutex_lock(&db_mutex);

    int rc1 = friends_upsert_one(user_id, friend_id, type);
    //int rc2 = friends_upsert_one(friend_id, user_id, type);

    pthread_mutex_unlock(&db_mutex);

    if (rc1 < 0) {
        return -1;
    }

    return 0;
}

int friends_list_for_user(int user_id, struct Friendship *out_array, int max_size)
{
    if (max_size <= 0) return 0;

    const char *sql =
        "SELECT user_id, friend_id, type "
        "FROM friends "
        "WHERE user_id = ? "
        "ORDER BY friend_id ASC "
        "LIMIT ?;";

    sqlite3_stmt *stmt = NULL;

    pthread_mutex_lock(&db_mutex);

    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[friends] prepare list failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, max_size);

    int count = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size) {
        out_array[count].user_id_1 = sqlite3_column_int(stmt, 0);
        out_array[count].user_id_2 = sqlite3_column_int(stmt, 1);
        out_array[count].type      = (enum friend_type)sqlite3_column_int(stmt, 2);
        count++;
    }

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[friends] list select error: %s\n", sqlite3_errmsg(g_db));
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return count;
}

static const char *friend_type_to_string(enum friend_type t)
{
    switch (t) {
        case FRIEND_CLOSE:  return "Close";
        case FRIEND_NORMAL: return "Normal";
        default:            return "Unknown";
    }
}

void format_friends_for_client(char *buf, size_t buf_size,
                               struct Friendship *friends, int count,
                               int current_user_id)
{
    if (buf_size == 0) return;

    buf[0] = '\0';
    int offset = 0;

    offset += snprintf(buf + offset, buf_size - offset,
                       "\033[32mOK\033[0m\nFRIENDS %d\n\n", count);

    for (int i = 0; i < count && offset < (int)buf_size - 1; i++)
    {
        struct Friendship *fr = &friends[i];

        // Determinăm care este "celălalt" user
        int other_id =
            (fr->user_id_1 == current_user_id ? fr->user_id_2 : fr->user_id_1);

        char other_name[64];
        auth_get_username_by_id(other_id, other_name, sizeof(other_name));

        const char *type_str = friend_type_to_string(fr->type);

        offset += snprintf(buf + offset, buf_size - offset,
            "\033[90m========== Friend #%d ==========\033[0m\n"
            "\033[35mUsername:\033[0m %s\n"
            "\033[35mUser ID:\033[0m %d\n"
            "\033[35mType:\033[0m \033[33m%s\033[0m\n\n",
            i + 1,
            other_name,
            other_id,
            type_str
        );
    }
}

int friends_delete(int user_id_1, const char *friend_username)
{
    if (user_id_1 <= 0 || friend_username == NULL || friend_username[0] == '\0')
        return -1;

    /* 1. Obținem id-ul prietenului B */
    int user_id_2 = auth_get_user_id_by_name(friend_username);
    if (user_id_2 <= 0) {
        return -2; // friend not found
    }

    /* 2. Construim SQL pentru a șterge ambele direcții */
    const char *sql =
        "DELETE FROM friends "
        "WHERE (user_id = ? AND friend_id = ?) "
        "   OR (user_id = ? AND friend_id = ?);";

    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[friends] delete prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, user_id_1);
    sqlite3_bind_int(stmt, 2, user_id_2);
    sqlite3_bind_int(stmt, 3, user_id_2);
    sqlite3_bind_int(stmt, 4, user_id_1);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[friends] delete step failed: %s\n",
                sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    int changes = sqlite3_changes(g_db);

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    if (changes == 0)
        return 0; // nu exista prietenia

    return 1; // succes - șterse ambele direcții
}

int friends_change_status(int user_id, int friend_id, enum friend_type new_type)
{
    if (user_id == friend_id)
        return 0;

    const char *sql =
        "UPDATE friends "
        "SET type = ? "
        "WHERE user_id = ? AND friend_id = ?;";

    sqlite3_stmt *stmt;
    pthread_mutex_lock(&db_mutex);

    int ok = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (ok != SQLITE_OK) {
        fprintf(stderr, "[friends] change_status prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, new_type);
    sqlite3_bind_int(stmt, 2, user_id);
    sqlite3_bind_int(stmt, 3, friend_id);

    ok = sqlite3_step(stmt);
    if (ok != SQLITE_DONE) {
        fprintf(stderr, "[friends] change_status step failed: %s\n",
                sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    int changes = sqlite3_changes(g_db);

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    if (changes == 0) {
        // nu exista încă relația user_id -> friend_id
        return 0;
    }

    return 1; // status schimbat
}

