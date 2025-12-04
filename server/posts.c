#include "common.h"
#include "posts.h"
#include "storage.h"
#include "models.h"   // pentru enum post_visibility, enum user_vis, FRIEND_CLOSE

// g_db și db_mutex vin din storage.c prin storage.h

int posts_add(int author_id, int visibility, const char *content)
{
    const char *sql =
        "INSERT INTO posts(author_id, visibility, content, created_at) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[posts] prepare failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
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
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    int new_id = (int)sqlite3_last_insert_rowid(g_db);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return new_id;
}

int posts_get_public(struct Post *out_array, int max_size)
{
    if (max_size <= 0) return 0;

    const char *sql =
        "SELECT p.id, p.author_id, p.visibility, p.content "
        "FROM posts p "
        "JOIN users u ON u.id = p.author_id "
        "WHERE p.visibility = ? "
        "  AND u.vis = ? "
        "ORDER BY p.created_at DESC "
        "LIMIT ?;";

    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[posts] prepare failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    int idx = 1;
    sqlite3_bind_int(stmt, idx++, VIS_PUBLIC);
    sqlite3_bind_int(stmt, idx++, USER_PUBLIC);
    sqlite3_bind_int(stmt, idx++, max_size);

    int count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size) {
        out_array[count].id        = sqlite3_column_int(stmt, 0);
        out_array[count].author_id = sqlite3_column_int(stmt, 1);
        out_array[count].vis       = (enum post_visibility)sqlite3_column_int(stmt, 2);

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
    pthread_mutex_unlock(&db_mutex);
    return count;
}

/*
 * Feed pentru user_id:
 *  - propriile postări
 *  - postări VIS_PUBLIC:
 *        * de la useri PUBLICI → vizibile
 *        * de la useri PRIVATI → DOAR dacă sunt prieteni
 *  - postări VIS_FRIENDS → DOAR prieteni
 *  - postări VIS_CLOSE_FRIENDS → DOAR prieteni FRIEND_CLOSE
 */
int posts_get_feed_for_user(int user_id, struct Post *out_array, int max_size)
{
    if (max_size <= 0) return 0;

    const char *sql =
        "SELECT p.id, p.author_id, p.visibility, p.content "
        "FROM posts p "
        "JOIN users u ON u.id = p.author_id "
        "LEFT JOIN friends f "
        "  ON f.user_id = ? AND f.friend_id = p.author_id "
        "WHERE "
        // propriile postări – întotdeauna vizibile
        "      p.author_id = ? "
        // postări publice:
        //  - dacă autorul e PUBLIC → oricine
        //  - dacă autorul e PRIVATE → doar prieteni (f.user_id NOT NULL)
        "   OR (p.visibility = ? AND (u.vis = ? OR f.user_id IS NOT NULL)) "
        // postări vizibile prietenilor
        "   OR (p.visibility = ? AND f.user_id IS NOT NULL) "
        // postări doar pentru prieteni CLOSE
        "   OR (p.visibility = ? AND f.type = ?) "
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

    // f.user_id = ?
    sqlite3_bind_int(stmt, idx++, user_id);

    // p.author_id = ?
    sqlite3_bind_int(stmt, idx++, user_id);

    // p.visibility = VIS_PUBLIC
    sqlite3_bind_int(stmt, idx++, VIS_PUBLIC);

    // u.vis = USER_PUBLIC
    sqlite3_bind_int(stmt, idx++, USER_PUBLIC);

    // p.visibility = VIS_FRIENDS
    sqlite3_bind_int(stmt, idx++, VIS_FRIENDS);

    // p.visibility = VIS_CLOSE_FRIENDS
    sqlite3_bind_int(stmt, idx++, VIS_CLOSE_FRIENDS);

    // f.type = FRIEND_CLOSE
    sqlite3_bind_int(stmt, idx++, FRIEND_CLOSE);

    // LIMIT ?
    sqlite3_bind_int(stmt, idx++, max_size);

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
