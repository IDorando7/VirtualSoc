#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sqlite3.h>
#include <stdbool.h>

#include "models.h"
#include "posts.h"
#include "auth.h"
#include "storage.h"

int posts_add(int author_id, int visibility, const char *content)
{
    const char *sql =
        "INSERT INTO posts(author_id, visibility, content, created_at) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    int rc;
    int new_id = -1;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[posts_add] prepare failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, author_id);
    sqlite3_bind_int(stmt, 2, visibility);
    sqlite3_bind_text(stmt, 3, content, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, (int)time(NULL));

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[posts_add] insert failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    new_id = (int)sqlite3_last_insert_rowid(g_db);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return new_id;
}


int posts_get_public(struct Post *out_array, int max_size)
{
    if (max_size <= 0)
        return 0;

    const char *sql =
        "SELECT p.id, p.author_id, u.name, p.visibility, p.content, p.created_at "
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
        fprintf(stderr, "[posts_get_public] prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    int idx = 1;
    sqlite3_bind_int(stmt, idx++, VIS_PUBLIC);
    sqlite3_bind_int(stmt, idx++, USER_PUBLIC);
    sqlite3_bind_int(stmt, idx++, max_size);

    int count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size)
    {
        out_array[count].id         = sqlite3_column_int(stmt, 0);
        out_array[count].author_id  = sqlite3_column_int(stmt, 1);
        out_array[count].vis        = (enum post_visibility)sqlite3_column_int(stmt, 3);
        out_array[count].created_at = sqlite3_column_int(stmt, 5);

        const char *uname = (const char *)sqlite3_column_text(stmt, 2);
        const char *txt   = (const char *)sqlite3_column_text(stmt, 4);

        strncpy(out_array[count].author_name, uname ? uname : "",
                sizeof(out_array[count].author_name) - 1);
        out_array[count].author_name[sizeof(out_array[count].author_name) - 1] = '\0';

        strncpy(out_array[count].content, txt ? txt : "",
                sizeof(out_array[count].content) - 1);
        out_array[count].content[sizeof(out_array[count].content) - 1] = '\0';

        count++;
    }

    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[posts_get_public] select error: %s\n",
                sqlite3_errmsg(g_db));
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return count;
}

int posts_get_feed_for_user(int user_id, struct Post *out_array, int max_size)
{
    if (max_size <= 0) return 0;

    const char *sql =
        "SELECT p.id, p.author_id, u.name, p.visibility, p.content, p.created_at "
        "FROM posts p "
        "JOIN users u ON u.id = p.author_id "
        "LEFT JOIN friends f1 "
        "       ON f1.user_id = ? "
        "      AND f1.friend_id = p.author_id "
        "LEFT JOIN friends f2 "
        "       ON f2.user_id = p.author_id "
        "      AND f2.friend_id = ? "
        "WHERE "
        "      p.author_id = ? "
        "   OR (p.visibility = ? "
        "       AND (u.vis = ? "
        "            OR (f1.user_id IS NOT NULL AND f2.user_id IS NOT NULL))) "
        "   OR (p.visibility = ? "
        "       AND (f1.user_id IS NOT NULL AND f2.user_id IS NOT NULL)) "
        "   OR (p.visibility = ? "
        "       AND (f2.user_id IS NOT NULL AND f2.type = ?)) "
        "ORDER BY p.created_at DESC "
        "LIMIT ?;";

    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[posts_get_feed_for_user] prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    int idx = 1;
    sqlite3_bind_int(stmt, idx++, user_id);
    sqlite3_bind_int(stmt, idx++, user_id);
    sqlite3_bind_int(stmt, idx++, user_id);
    sqlite3_bind_int(stmt, idx++, VIS_PUBLIC);
    sqlite3_bind_int(stmt, idx++, USER_PUBLIC);
    sqlite3_bind_int(stmt, idx++, VIS_FRIENDS);
    sqlite3_bind_int(stmt, idx++, VIS_CLOSE_FRIENDS);
    sqlite3_bind_int(stmt, idx++, FRIEND_CLOSE);
    sqlite3_bind_int(stmt, idx++, max_size);

    int count = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size)
    {
        out_array[count].id         = sqlite3_column_int(stmt, 0);
        out_array[count].author_id  = sqlite3_column_int(stmt, 1);
        out_array[count].vis        = (enum post_visibility)sqlite3_column_int(stmt, 3);
        out_array[count].created_at = sqlite3_column_int(stmt, 5);

        const char *uname = (const char *)sqlite3_column_text(stmt, 2);
        const char *txt   = (const char *)sqlite3_column_text(stmt, 4);

        strncpy(out_array[count].author_name, uname ? uname : "",
                sizeof(out_array[count].author_name) - 1);
        out_array[count].author_name[sizeof(out_array[count].author_name) - 1] = '\0';

        strncpy(out_array[count].content, txt ? txt : "",
                sizeof(out_array[count].content) - 1);
        out_array[count].content[sizeof(out_array[count].content) - 1] = '\0';

        count++;
    }

    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[posts_get_feed_for_user] select error: %s\n",
                sqlite3_errmsg(g_db));
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return count;
}


static const char* visibility_to_string(enum post_visibility v)
{
    switch (v)
    {
        case VIS_PUBLIC:        return "Public";
        case VIS_FRIENDS:       return "Friends";
        case VIS_CLOSE_FRIENDS: return "Close";
        default:                return "Unknown";
    }
}

void format_posts_for_client(char *buf, int buf_size, struct Post *posts, int count)
{
    if (buf_size == 0)
        return;

    buf[0] = '\0';
    int offset = 0;

    offset += snprintf(buf + offset, buf_size - offset,
                       "\033[32mOK\033[0m\nPOSTS %d\n\n", count);

    for (int i = 0; i < count; i++)
    {
        if (offset >= (int)buf_size - 1)
            break;

        const char *vis_str = visibility_to_string(posts[i].vis);

        char timebuf[64] = {0};
        time_t t = (time_t)posts[i].created_at;
        struct tm tm_info;
        localtime_r(&t, &tm_info);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm_info);

        offset += snprintf(buf + offset, buf_size - offset,
            "\033[90m========== Post #%d ==========\033[0m\n"
            "\033[35mID:\033[0m %d\n"
            "\033[35mAuthor:\033[0m \033[36m%s\033[0m\n"
            "\033[35mTime:\033[0m \033[34m%s\033[0m\n"
            "\033[35mVisibility:\033[0m \033[33m%s\033[0m\n"
            "\033[35mContent:\033[0m\n%s\n\n",
            posts[i].id,
            posts[i].id,
            posts[i].author_name,
            timebuf,
            vis_str,
            posts[i].content);
    }
}

static void write_all(int fd, const char *buf, size_t len)
{
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(fd, buf + off, len - off);
        if (n <= 0) return;
        off += (size_t)n;
    }
}

void posts_send_for_client(int client_fd, struct Post *posts, int count)
{
    char out[2048];
    int off = 0;

    off = snprintf(out, sizeof(out), "\033[32mOK\033[0m\nPOSTS %d\n\n", count);
    write_all(client_fd, out, (size_t)off);

    for (int i = 0; i < count; i++) {
        const char *vis_str = visibility_to_string(posts[i].vis);

        char timebuf[64] = {0};
        time_t t = (time_t)posts[i].created_at;
        struct tm tm_info;
        localtime_r(&t, &tm_info);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm_info);

        off = snprintf(out, sizeof(out),
        "\033[90m========== Post #%d ==========\033[0m\n"
        "\033[35mID:\033[0m %d\n"
        "\033[35mAuthor:\033[0m \033[36m%s\033[0m\n"
        "\033[35mTime:\033[0m \033[34m%s\033[0m\n"
        "\033[35mVisibility:\033[0m \033[33m%s\033[0m\n"
        "\033[35mContent:\033[0m\n%s\n\n",
            posts[i].id,
            posts[i].id,
            posts[i].author_name,
            timebuf,
            vis_str,
            posts[i].content
        );

        write_all(client_fd, out, (size_t)off);
    }

    write_all(client_fd, "END\n", 4);
}

int posts_delete(int requester_id, int post_id)
{
    if (requester_id <= 0 || post_id <= 0)
        return -1;

    const char *sql_get_author =
        "SELECT author_id FROM posts WHERE id = ?;";

    const char *sql_delete =
        "DELETE FROM posts WHERE id = ?;";

    sqlite3_stmt *stmt = NULL;
    int rc;
    int author_id = -1;

    pthread_mutex_lock(&db_mutex);
    rc = sqlite3_prepare_v2(g_db, sql_get_author, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[posts] prepare get author failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, post_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        author_id = sqlite3_column_int(stmt, 0);
    } else if (rc == SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return 0;
    }
    else
    {
        fprintf(stderr, "[posts] get author error: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    if (requester_id != author_id)
    {
        int is_admin = auth_is_admin(requester_id);
        if (is_admin <= 0)
        {
            return -2;
        }
    }

    pthread_mutex_lock(&db_mutex);
    rc = sqlite3_prepare_v2(g_db, sql_delete, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[posts] prepare delete failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, post_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[posts] delete failed: %s\n", sqlite3_errmsg(g_db));
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

int posts_get_for_user(int viewer_id, int target_user_id,
                       struct Post *out_array, int max_size)
{
    if (max_size <= 0 || target_user_id <= 0)
        return 0;

    int is_admin = 0;
    if (viewer_id > 0)
    {
        is_admin = (auth_is_admin(viewer_id) == 1);
    }

    sqlite3_stmt *stmt = NULL;
    int rc;
    int count = 0;

    pthread_mutex_lock(&db_mutex);

    int target_vis = USER_PUBLIC;

    const char *sql_vis =
        "SELECT vis FROM users WHERE id = ? LIMIT 1;";

    rc = sqlite3_prepare_v2(g_db, sql_vis, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[posts_get_for_user] prepare vis failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, target_user_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        target_vis = sqlite3_column_int(stmt, 0);
    }
    else
    {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return 0;
    }

    sqlite3_finalize(stmt);
    if (is_admin || (viewer_id > 0 && viewer_id == target_user_id))
    {
        const char *sql_all =
            "SELECT p.id, p.author_id, u.name, p.visibility, p.content, p.created_at "
            "FROM posts p "
            "JOIN users u ON u.id = p.author_id "
            "WHERE p.author_id = ? "
            "ORDER BY p.created_at DESC "
            "LIMIT ?;";

        rc = sqlite3_prepare_v2(g_db, sql_all, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
        {
            fprintf(stderr, "[posts_get_for_user] prepare all failed: %s\n",
                    sqlite3_errmsg(g_db));
            pthread_mutex_unlock(&db_mutex);
            return -1;
        }

        sqlite3_bind_int(stmt, 1, target_user_id);
        sqlite3_bind_int(stmt, 2, max_size);

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size)
        {
            out_array[count].id         = sqlite3_column_int(stmt, 0);
            out_array[count].author_id  = sqlite3_column_int(stmt, 1);
            out_array[count].vis        = (enum post_visibility)sqlite3_column_int(stmt, 3);
            out_array[count].created_at = sqlite3_column_int(stmt, 5);

            const char *name = (const char *)sqlite3_column_text(stmt, 2);
            const char *txt  = (const char *)sqlite3_column_text(stmt, 4);

            strncpy(out_array[count].author_name, name ? name : "",
                    sizeof(out_array[count].author_name) - 1);
            out_array[count].author_name[sizeof(out_array[count].author_name) - 1] = '\0';

            strncpy(out_array[count].content, txt ? txt : "",
                    sizeof(out_array[count].content) - 1);
            out_array[count].content[sizeof(out_array[count].content) - 1] = '\0';

            count++;
        }

        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return count;
    }

    if (viewer_id <= 0)
    {
        if (target_vis != USER_PUBLIC)
        {
            pthread_mutex_unlock(&db_mutex);
            return 0;
        }

        const char *sql_public =
            "SELECT p.id, p.author_id, u.name, p.visibility, p.content, p.created_at "
            "FROM posts p "
            "JOIN users u ON u.id = p.author_id "
            "WHERE p.author_id = ? AND p.visibility = ? "
            "ORDER BY p.created_at DESC "
            "LIMIT ?;";

        rc = sqlite3_prepare_v2(g_db, sql_public, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
        {
            fprintf(stderr, "[posts_get_for_user] prepare public failed: %s\n",
                    sqlite3_errmsg(g_db));
            pthread_mutex_unlock(&db_mutex);
            return -1;
        }

        sqlite3_bind_int(stmt, 1, target_user_id);
        sqlite3_bind_int(stmt, 2, VIS_PUBLIC);
        sqlite3_bind_int(stmt, 3, max_size);

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size)
        {
            out_array[count].id         = sqlite3_column_int(stmt, 0);
            out_array[count].author_id  = sqlite3_column_int(stmt, 1);
            out_array[count].vis        = (enum post_visibility)sqlite3_column_int(stmt, 3);
            out_array[count].created_at = sqlite3_column_int(stmt, 5);

            const char *name = (const char *)sqlite3_column_text(stmt, 2);
            const char *txt  = (const char *)sqlite3_column_text(stmt, 4);

            strncpy(out_array[count].author_name, name ? name : "",
                    sizeof(out_array[count].author_name) - 1);
            out_array[count].author_name[sizeof(out_array[count].author_name) - 1] = '\0';

            strncpy(out_array[count].content, txt ? txt : "",
                    sizeof(out_array[count].content) - 1);
            out_array[count].content[sizeof(out_array[count].content) - 1] = '\0';

            count++;
        }

        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return count;
    }

    bool has_1 = false, has_2 = false;
    enum friend_type t1 = FRIEND_NORMAL, t2 = FRIEND_NORMAL;

    const char *sql_f1 =
        "SELECT type FROM friends WHERE user_id = ? AND friend_id = ? LIMIT 1;";

    rc = sqlite3_prepare_v2(g_db, sql_f1, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[posts_get_for_user] prepare f1 failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, viewer_id);
    sqlite3_bind_int(stmt, 2, target_user_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        has_1 = true;
        t1 = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    const char *sql_f2 =
        "SELECT type FROM friends WHERE user_id = ? AND friend_id = ? LIMIT 1;";

    rc = sqlite3_prepare_v2(g_db, sql_f2, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[posts_get_for_user] prepare f2 failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, target_user_id);
    sqlite3_bind_int(stmt, 2, viewer_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        has_2 = true;
        t2 = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    bool are_friends = (has_1 && has_2);
    bool is_close_from_target = (has_2 && t2 == FRIEND_CLOSE);
    bool are_close            = (are_friends && is_close_from_target);

    if (target_vis == USER_PRIVATE && !are_friends)
    {
        pthread_mutex_unlock(&db_mutex);
        return 0;
    }

    int allow_public  = 0;
    int allow_friend  = 0;
    int allow_close   = 0;

    if (target_vis == USER_PUBLIC) {
        allow_public = 1;
    } else {
        allow_public = are_friends ? 1 : 0;
    }

    allow_friend = are_friends ? 1 : 0;
    allow_close  = are_close   ? 1 : 0;

    if (!allow_public && !allow_friend && !allow_close) {
        pthread_mutex_unlock(&db_mutex);
        return 0;
    }

    const char *sql_sel =
        "SELECT p.id, p.author_id, u.name, p.visibility, p.content, p.created_at "
        "FROM posts p "
        "JOIN users u ON u.id = p.author_id "
        "WHERE p.author_id = ? AND ( "
        "      (? AND p.visibility = ?) "
        "   OR (? AND p.visibility = ?) "
        "   OR (? AND p.visibility = ?) "
        ") "
        "ORDER BY p.created_at DESC "
        "LIMIT ?;";

    rc = sqlite3_prepare_v2(g_db, sql_sel, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[posts_get_for_user] prepare filtered failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    int idx = 1;
    sqlite3_bind_int(stmt, idx++, target_user_id);
    sqlite3_bind_int(stmt, idx++, allow_public);
    sqlite3_bind_int(stmt, idx++, VIS_PUBLIC);
    sqlite3_bind_int(stmt, idx++, allow_friend);
    sqlite3_bind_int(stmt, idx++, VIS_FRIENDS);
    sqlite3_bind_int(stmt, idx++, allow_close);
    sqlite3_bind_int(stmt, idx++, VIS_CLOSE_FRIENDS);
    sqlite3_bind_int(stmt, idx++, max_size);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size)
    {
        out_array[count].id         = sqlite3_column_int(stmt, 0);
        out_array[count].author_id  = sqlite3_column_int(stmt, 1);
        out_array[count].vis        = (enum post_visibility)sqlite3_column_int(stmt, 3);
        out_array[count].created_at = sqlite3_column_int(stmt, 5);

        const char *name = (const char *)sqlite3_column_text(stmt, 2);
        const char *txt  = (const char *)sqlite3_column_text(stmt, 4);

        strncpy(out_array[count].author_name, name ? name : "",
                sizeof(out_array[count].author_name) - 1);
        out_array[count].author_name[sizeof(out_array[count].author_name) - 1] = '\0';

        strncpy(out_array[count].content, txt ? txt : "",
                sizeof(out_array[count].content) - 1);
        out_array[count].content[sizeof(out_array[count].content) - 1] = '\0';

        count++;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return count;
}

