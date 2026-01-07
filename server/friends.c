#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sqlite3.h>

#include "friends.h"
#include "storage.h"
#include "auth.h"

static int friends_upsert_one(int user_id, int friend_id, enum friend_type type)
{
    const char *sql =
        "INSERT INTO friends(user_id, friend_id, type) "
        "VALUES (?, ?, ?) "
        "ON CONFLICT(user_id, friend_id) "
        "DO UPDATE SET type = excluded.type;";

    sqlite3_stmt *stmt = NULL;

    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[friends] prepare upsert failed: %s\n", sqlite3_errmsg(g_db));
        return -1;
    }

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, friend_id);
    sqlite3_bind_int(stmt, 3, (int)type);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[friends] upsert failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        return -1;
    }

    sqlite3_finalize(stmt);
    return 0;
}

int friends_add(int user_id, int friend_id, enum friend_type type)
{
    if (user_id == friend_id)
        return 0;

    pthread_mutex_lock(&db_mutex);
    int rc1 = friends_upsert_one(user_id, friend_id, type);
    pthread_mutex_unlock(&db_mutex);

    if (rc1 < 0)
        return -1;
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
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[friends] prepare list failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, max_size);

    int count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size)
    {
        out_array[count].user_id_1 = sqlite3_column_int(stmt, 0);
        out_array[count].user_id_2 = sqlite3_column_int(stmt, 1);
        out_array[count].type      = (enum friend_type)sqlite3_column_int(stmt, 2);
        count++;
    }

    if (rc != SQLITE_DONE)
        fprintf(stderr, "[friends] list select error: %s\n", sqlite3_errmsg(g_db));

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return count;
}

static const char *friend_type_to_string(enum friend_type t)
{
    switch (t)
    {
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

    int user_id_2 = auth_get_user_id_by_name(friend_username);
    if (user_id_2 <= 0)
        return -2;

    const char *sql =
        "DELETE FROM friends "
        "WHERE (user_id = ? AND friend_id = ?) "
        "   OR (user_id = ? AND friend_id = ?);";

    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
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
    if (rc != SQLITE_DONE)
    {
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
        return 0;
    return 1;
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
    if (ok != SQLITE_OK)
    {
        fprintf(stderr, "[friends] change_status prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, new_type);
    sqlite3_bind_int(stmt, 2, user_id);
    sqlite3_bind_int(stmt, 3, friend_id);

    ok = sqlite3_step(stmt);
    if (ok != SQLITE_DONE)
    {
        fprintf(stderr, "[friends] change_status step failed: %s\n",
                sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    int changes = sqlite3_changes(g_db);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    if (changes == 0)
        return 0;
    return 1;
}

int friends_are_mutual(int a, int b)
{
    const char *sql =
        "SELECT "
        " (SELECT COUNT(*) FROM friends WHERE user_id=? AND friend_id=?) AS c1, "
        " (SELECT COUNT(*) FROM friends WHERE user_id=? AND friend_id=?) AS c2;";

    sqlite3_stmt *stmt = NULL;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, a);
    sqlite3_bind_int(stmt, 2, b);
    sqlite3_bind_int(stmt, 3, b);
    sqlite3_bind_int(stmt, 4, a);

    rc = sqlite3_step(stmt);
    int ok = 0;
    if (rc == SQLITE_ROW) {
        int c1 = sqlite3_column_int(stmt, 0);
        int c2 = sqlite3_column_int(stmt, 1);
        ok = (c1 > 0 && c2 > 0);
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return ok;
}

static int friends_request_exists(int from_id, int to_id)
{
    const char *sql =
        "SELECT 1 FROM friend_requests WHERE from_id=? AND to_id=? LIMIT 1;";
    sqlite3_stmt *stmt = NULL;
    int rc, found = 0;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&db_mutex); return -1; }

    sqlite3_bind_int(stmt, 1, from_id);
    sqlite3_bind_int(stmt, 2, to_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) found = 1;

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return found;
}

int friends_request_send(int from_id, int to_id)
{
    if (from_id <= 0 || to_id <= 0 || from_id == to_id) return -1;

    int mutual = friends_are_mutual(from_id, to_id);
    if (mutual == 1) return 1;
    if (mutual < 0) return -1;

    int same = friends_request_exists(from_id, to_id);
    if (same < 0) return -1;
    if (same == 1) return 2;

    int rev = friends_request_exists(to_id, from_id);
    if (rev < 0) return -1;
    if (rev == 1) return 3;

    const char *sql =
        "INSERT INTO friend_requests(from_id, to_id, created_at) VALUES(?,?,?);";
    sqlite3_stmt *stmt = NULL;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&db_mutex); return -1; }

    sqlite3_bind_int(stmt, 1, from_id);
    sqlite3_bind_int(stmt, 2, to_id);
    sqlite3_bind_int(stmt, 3, (int)time(NULL));

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

int friends_request_list(int to_id, struct FriendRequestInfo *out, int max)
{
    if (max <= 0) return 0;

    const char *sql =
        "SELECT r.from_id, u.name, r.created_at "
        "FROM friend_requests r "
        "JOIN users u ON u.id = r.from_id "
        "WHERE r.to_id=? "
        "ORDER BY r.created_at DESC "
        "LIMIT ?;";

    sqlite3_stmt *stmt = NULL;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&db_mutex); return -1; }

    sqlite3_bind_int(stmt, 1, to_id);
    sqlite3_bind_int(stmt, 2, max);

    int count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max)
    {
        out[count].from_id = sqlite3_column_int(stmt, 0);
        const char *name = (const char*)sqlite3_column_text(stmt, 1);
        out[count].created_at = sqlite3_column_int(stmt, 2);

        strncpy(out[count].from_name, name ? name : "", sizeof(out[count].from_name)-1);
        out[count].from_name[sizeof(out[count].from_name)-1] = '\0';
        count++;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return count;
}

int friends_request_reject(int me_id, const char *from_username)
{
    int from_id = auth_get_user_id_by_name(from_username);
    if (from_id <= 0) return -2;

    const char *sql =
        "DELETE FROM friend_requests WHERE from_id=? AND to_id=?;";

    sqlite3_stmt *stmt = NULL;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&db_mutex); return -1; }

    sqlite3_bind_int(stmt, 1, from_id);
    sqlite3_bind_int(stmt, 2, me_id);

    rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(g_db);

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    if (rc != SQLITE_DONE) return -1;
    return (changes > 0) ? 1 : 0;
}

int friends_request_accept_by_ids(int me_id, int from_id, enum friend_type my_type, enum friend_type other_type)
{
    if (me_id <= 0 || from_id <= 0 || me_id == from_id) return -1;

    int mutual = friends_are_mutual(me_id, from_id);
    if (mutual == 1) return 2;
    if (mutual < 0) return -1;

    int exists = friends_request_exists(from_id, me_id);
    if (exists < 0) return -1;
    if (exists == 0) return 0;

    if (friends_add(me_id, from_id, my_type) < 0) return -1;
    if (friends_add(from_id, me_id, other_type) < 0) return -1;

    const char *sql = "DELETE FROM friend_requests WHERE from_id=? AND to_id=?;";
    sqlite3_stmt *stmt = NULL;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) { pthread_mutex_unlock(&db_mutex); return -1; }

    sqlite3_bind_int(stmt, 1, from_id);
    sqlite3_bind_int(stmt, 2, me_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return (rc == SQLITE_DONE) ? 1 : -1;
}

int friends_request_accept(int me_id, const char *from_username)
{
    int from_id = auth_get_user_id_by_name(from_username);
    if (from_id <= 0) return -2;

    return friends_request_accept_by_ids(me_id, from_id, FRIEND_NORMAL, FRIEND_NORMAL);
}

