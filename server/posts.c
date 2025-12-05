// posts.c

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sqlite3.h>

#include "models.h"
#include "posts.h"
#include "storage.h"   // g_db, db_mutex

/*
 * Presupunem existența tabelelor:
 *
 *  users(
 *      id            INTEGER PRIMARY KEY AUTOINCREMENT,
 *      name          TEXT UNIQUE NOT NULL,
 *      password_hash TEXT NOT NULL,   -- nu e folosit aici direct
 *      type          INTEGER NOT NULL, -- USER_NORMAL / USER_ADMIN
 *      vis           INTEGER NOT NULL  -- USER_PUBLIC / USER_PRIVATE
 *  );
 *
 *  posts(
 *      id         INTEGER PRIMARY KEY AUTOINCREMENT,
 *      author_id  INTEGER NOT NULL REFERENCES users(id),
 *      visibility INTEGER NOT NULL,    -- VIS_PUBLIC / VIS_FRIENDS / VIS_CLOSE_FRIENDS
 *      content    TEXT NOT NULL,
 *      created_at INTEGER NOT NULL
 *  );
 *
 *  friends(
 *      user_id   INTEGER NOT NULL,
 *      friend_id INTEGER NOT NULL,
 *      type      INTEGER NOT NULL,     -- FRIEND_CLOSE / FRIEND_NORMAL
 *      PRIMARY KEY(user_id, friend_id)
 *  );
 */

/*
 * Adaugă o postare nouă.
 *  - author_id  -> id-ul userului care postează
 *  - visibility -> VIS_PUBLIC / VIS_FRIENDS / VIS_CLOSE_FRIENDS
 *  - content    -> textul postării
 *
 * Returnează:
 *  - id-ul postării noi (>= 1) la succes
 *  - -1 la eroare
 */
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
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[posts_add] prepare failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, author_id);
    sqlite3_bind_int(stmt, 2, visibility);
    sqlite3_bind_text(stmt, 3, content, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, (int)time(NULL));

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
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

/*
 * posts_get_public:
 *  - întoarce postări PUBLIC ale userilor PUBLICI.
 *  - out_array: buffer unde punem rezultatele
 *  - max_size: numărul maxim de postări
 *
 * Returnează:
 *  - >= 0 -> numărul de postări întoarse
 *  - -1   -> eroare
 */
int posts_get_public(struct Post *out_array, int max_size)
{
    if (max_size <= 0) return 0;

    const char *sql =
        "SELECT p.id, p.author_id, u.name, p.visibility, p.content "
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
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[posts_get_public] prepare failed: %s\n", sqlite3_errmsg(g_db));
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

        const unsigned char *uname = sqlite3_column_text(stmt, 2);
        if (uname) {
            strncpy(out_array[count].author_name,
                    (const char*)uname,
                    sizeof(out_array[count].author_name) - 1);
            out_array[count].author_name[sizeof(out_array[count].author_name) - 1] = '\0';
        } else {
            out_array[count].author_name[0] = '\0';
        }

        out_array[count].vis = (enum post_visibility)sqlite3_column_int(stmt, 3);

        const unsigned char *txt = sqlite3_column_text(stmt, 4);
        if (txt) {
            strncpy(out_array[count].content,
                    (const char*)txt,
                    sizeof(out_array[count].content) - 1);
            out_array[count].content[sizeof(out_array[count].content) - 1] = '\0';
        } else {
            out_array[count].content[0] = '\0';
        }

        count++;
    }

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[posts_get_public] select error: %s\n", sqlite3_errmsg(g_db));
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return count;
}

/*
 * posts_get_feed_for_user:
 *  - feed personalizat pentru user_id:
 *    * toate postările lui (indiferent de visibility)
 *    * postări VIS_PUBLIC:
 *         - de la useri PUBLICI -> oricine
 *         - de la useri PRIVATI -> doar dacă sunt prieteni
 *    * postări VIS_FRIENDS -> doar dacă sunt prieteni
 *    * postări VIS_CLOSE_FRIENDS -> doar dacă sunt prieteni de tip FRIEND_CLOSE
 *
 * Returnează:
 *  - >=0 -> numărul de postări găsite
 *  - -1  -> eroare
 */
int posts_get_feed_for_user(int user_id, struct Post *out_array, int max_size)
{
    if (max_size <= 0) return 0;

    const char *sql =
        "SELECT p.id, p.author_id, u.name, p.visibility, p.content "
        "FROM posts p "
        "JOIN users u ON u.id = p.author_id "
        "LEFT JOIN friends f "
        "       ON f.user_id = ? "
        "      AND f.friend_id = p.author_id "
        "WHERE "
        // propriile postări – întotdeauna vizibile
        "      p.author_id = ? "
        // postări VIS_PUBLIC:
        //  - autor PUBLIC -> toată lumea
        //  - autor PRIVATE -> doar dacă sunt prieteni (f.user_id NOT NULL)
        "   OR (p.visibility = ? AND (u.vis = ? OR f.user_id IS NOT NULL)) "
        // postări DOAR pentru prieteni
        "   OR (p.visibility = ? AND f.user_id IS NOT NULL) "
        // postări DOAR pentru prieteni CLOSE
        "   OR (p.visibility = ? AND f.type = ?) "
        "ORDER BY p.created_at DESC "
        "LIMIT ?;";

    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[posts_get_feed_for_user] prepare failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    int idx = 1;

    // f.user_id = ?
    sqlite3_bind_int(stmt, idx++, user_id);

    // p.author_id = ?
    sqlite3_bind_int(stmt, idx++, user_id);

    // (p.visibility = VIS_PUBLIC AND (u.vis = USER_PUBLIC OR f.user_id NOT NULL))
    sqlite3_bind_int(stmt, idx++, VIS_PUBLIC);
    sqlite3_bind_int(stmt, idx++, USER_PUBLIC);

    // p.visibility = VIS_FRIENDS
    sqlite3_bind_int(stmt, idx++, VIS_FRIENDS);

    // p.visibility = VIS_CLOSE_FRIENDS AND f.type = FRIEND_CLOSE
    sqlite3_bind_int(stmt, idx++, VIS_CLOSE_FRIENDS);
    sqlite3_bind_int(stmt, idx++, FRIEND_CLOSE);

    // LIMIT ?
    sqlite3_bind_int(stmt, idx++, max_size);

    int count = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size) {
        out_array[count].id        = sqlite3_column_int(stmt, 0);
        out_array[count].author_id = sqlite3_column_int(stmt, 1);

        const unsigned char *uname = sqlite3_column_text(stmt, 2);
        if (uname) {
            strncpy(out_array[count].author_name,
                    (const char*)uname,
                    sizeof(out_array[count].author_name) - 1);
            out_array[count].author_name[sizeof(out_array[count].author_name) - 1] = '\0';
        } else {
            out_array[count].author_name[0] = '\0';
        }

        out_array[count].vis = (enum post_visibility)sqlite3_column_int(stmt, 3);

        const unsigned char *txt = sqlite3_column_text(stmt, 4);
        if (txt) {
            strncpy(out_array[count].content,
                    (const char*)txt,
                    sizeof(out_array[count].content) - 1);
            out_array[count].content[sizeof(out_array[count].content) - 1] = '\0';
        } else {
            out_array[count].content[0] = '\0';
        }

        count++;
    }

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[posts_get_feed_for_user] select error: %s\n", sqlite3_errmsg(g_db));
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

void format_posts_for_client(char *buf, size_t buf_size,
                             struct Post *posts, int count)
{
    if (buf_size == 0)
        return;

    buf[0] = '\0';
    int offset = 0;

    // Header
    offset += snprintf(buf + offset, buf_size - offset,
                       "\033[32mOK\033[0m\nPOSTS %d\n\n", count);

    for (int i = 0; i < count; i++)
    {
        if (offset >= (int)buf_size - 1)
            break;

        const char *vis_str = visibility_to_string(posts[i].vis);

        offset += snprintf(buf + offset, buf_size - offset,
            "\033[90m========== Post #%d ==========\033[0m\n"
            "\033[35mID:\033[0m %d\n"
            "\033[35mAuthor:\033[0m \033[36m%s\033[0m\n"
            "\033[35mVisibility:\033[0m \033[33m%s\033[0m\n"
            "\033[35mContent:\033[0m\n%s\n\n",
            posts[i].id,
            posts[i].id,
            posts[i].author_name,
            vis_str,
            posts[i].content);
    }
}

