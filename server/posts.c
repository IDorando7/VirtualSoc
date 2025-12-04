// posts.c
#include <sqlite3.h>
#include <time.h>
#include <string.h>
#include "posts.h"
#include "storage.h"

extern sqlite3 *g_db;

int posts_add(int author_id, int visibility, const char *content)
{
    const char *sql =
        "INSERT INTO posts(author_id, visibility, content, created_at) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[posts] prepare failed: %s\n", sqlite3_errmsg(g_db));
        return -1;
    }

    sqlite3_bind_int(stmt, 1, author_id);
    sqlite3_bind_int(stmt, 2, visibility);
    sqlite3_bind_text(stmt, 3, content, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, (int)time(NULL));

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[posts] insert failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        return -1;
    }

    int new_id = (int)sqlite3_last_insert_rowid(g_db);
    sqlite3_finalize(stmt);
    return new_id;
}

int posts_get_public(struct Post *out_array, int max_size)
{
    const char *sql =
        "SELECT id, author_id, visibility, content "
        "FROM posts "
        "WHERE visibility = ? "
        "ORDER BY created_at DESC "
        "LIMIT ?;";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[posts] prepare failed: %s\n", sqlite3_errmsg(g_db));
        return -1;
    }

    sqlite3_bind_int(stmt, 1, VIS_PUBLIC);
    sqlite3_bind_int(stmt, 2, max_size);

    int count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size) {
        out_array[count].id        = sqlite3_column_int(stmt, 0);
        out_array[count].author_id = sqlite3_column_int(stmt, 1);
        out_array[count].vis       = sqlite3_column_int(stmt, 2);

        const unsigned char *txt = sqlite3_column_text(stmt, 3);
        strncpy(out_array[count].content,
                txt ? (const char *)txt : "",
                sizeof(out_array[count].content) - 1);
        out_array[count].content[sizeof(out_array[count].content)-1] = '\0';

        count++;
    }

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[posts] select failed: %s\n", sqlite3_errmsg(g_db));
    }

    sqlite3_finalize(stmt);
    return count;
}

int posts_get_feed_for_user(int user_id, struct Post *out_array, int max_size)
{
    if (max_size <= 0) return 0;

    const char *sql =
        "SELECT p.id, p.author_id, p.visibility, p.content "
        "FROM posts p "
        "LEFT JOIN friends f "
        "  ON f.user_id = ? AND f.friend_id = p.author_id "
        "WHERE "
        "      p.visibility = ? "                         // VIS_PUBLIC
        "   OR p.author_id = ? "                         // propriile postări
        "   OR (p.visibility = ? AND f.user_id IS NOT NULL) "              // VIS_FRIENDS
        "   OR (p.visibility = ? AND f.type = ?) "                        // VIS_CLOSE_FRIENDS + FRIEND_CLOSE
        "ORDER BY p.created_at DESC "
        "LIMIT ?;";

    sqlite3_stmt *stmt = NULL;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[posts] prepare feed failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    int idx = 1;

    sqlite3_bind_int(stmt, idx++, user_id);            // f.user_id = ?
    sqlite3_bind_int(stmt, idx++, VIS_PUBLIC);         // p.visibility = VIS_PUBLIC
    sqlite3_bind_int(stmt, idx++, user_id);            // p.author_id = user_id
    sqlite3_bind_int(stmt, idx++, VIS_FRIENDS);        // p.visibility = VIS_FRIENDS
    sqlite3_bind_int(stmt, idx++, VIS_CLOSE_FRIENDS);  // p.visibility = VIS_CLOSE_FRIENDS
    sqlite3_bind_int(stmt, idx++, FRIEND_CLOSE);       // f.type = FRIEND_CLOSE
    sqlite3_bind_int(stmt, idx++, max_size);           // LIMIT ?

    int count = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size) {
        out_array[count].id        = sqlite3_column_int(stmt, 0);
        out_array[count].author_id = sqlite3_column_int(stmt, 1);
        out_array[count].vis       = (enum post_visibility)sqlite3_column_int(stmt, 2);

        const unsigned char *txt = sqlite3_column_text(stmt, 3);
        if (txt) {
            strncpy(out_array[count].content,
                    (const char *)txt,
                    sizeof(out_array[count].content) - 1);
            out_array[count].content[sizeof(out_array[count].content) - 1] = '\0';
        } else {
            out_array[count].content[0] = '\0';
        }

        count++;
    }

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[posts] feed select error: %s\n", sqlite3_errmsg(g_db));
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return count;
}