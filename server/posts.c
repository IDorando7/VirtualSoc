// posts.c

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sqlite3.h>
#include <stdbool.h>

#include "models.h"
#include "posts.h"
#include "auth.h"
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
    if (max_size <= 0)
        return 0;

    const char *sql =
        "SELECT p.id, p.author_id, u.name, p.visibility, p.content "
        "FROM posts p "
        "JOIN users u ON u.id = p.author_id "
        "WHERE p.visibility = ? "
        "  AND u.vis = ? "          /* doar useri cu profil PUBLIC */
        "ORDER BY p.created_at DESC "
        "LIMIT ?;";

    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[posts_get_public] prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    int idx = 1;
    sqlite3_bind_int(stmt, idx++, VIS_PUBLIC);   // p.visibility = VIS_PUBLIC
    sqlite3_bind_int(stmt, idx++, USER_PUBLIC);  // u.vis = USER_PUBLIC
    sqlite3_bind_int(stmt, idx++, max_size);     // LIMIT ?

    int count = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size) {

        out_array[count].id        = sqlite3_column_int(stmt, 0);
        out_array[count].author_id = sqlite3_column_int(stmt, 1);
        out_array[count].vis       = (enum post_visibility)sqlite3_column_int(stmt, 3);

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

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[posts_get_public] select error: %s\n",
                sqlite3_errmsg(g_db));
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
        "LEFT JOIN friends f1 "
        "       ON f1.user_id = ? "                /* viewer -> author */
        "      AND f1.friend_id = p.author_id "
        "LEFT JOIN friends f2 "
        "       ON f2.user_id = p.author_id "      /* author -> viewer */
        "      AND f2.friend_id = ? "
        "WHERE "
        "      p.author_id = ? "                   /* always see own posts */
        "   OR (p.visibility = ? "                 /* PUBLIC posts: */
        "       AND (u.vis = ? "                   /*  - author has public profile */
        "            OR (f1.user_id IS NOT NULL AND f2.user_id IS NOT NULL))) " /*  - or mutual friends */
        "   OR (p.visibility = ? "                 /* FRIENDS posts: mutual friends only */
        "       AND (f1.user_id IS NOT NULL AND f2.user_id IS NOT NULL)) "
        "   OR (p.visibility = ? "                 /* CLOSE_FRIENDS posts: author marked viewer as CLOSE */
        "       AND (f2.user_id IS NOT NULL AND f2.type = ?)) "
        "ORDER BY p.created_at DESC "
        "LIMIT ?;";

    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[posts_get_feed_for_user] prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    int idx = 1;
    sqlite3_bind_int(stmt, idx++, user_id);   /* f1.user_id (viewer) */
    sqlite3_bind_int(stmt, idx++, user_id);   /* f2.friend_id (viewer) */
    sqlite3_bind_int(stmt, idx++, user_id);   /* own posts: p.author_id = viewer */
    sqlite3_bind_int(stmt, idx++, VIS_PUBLIC);
    sqlite3_bind_int(stmt, idx++, USER_PUBLIC);
    sqlite3_bind_int(stmt, idx++, VIS_FRIENDS);
    sqlite3_bind_int(stmt, idx++, VIS_CLOSE_FRIENDS);
    sqlite3_bind_int(stmt, idx++, FRIEND_CLOSE);
    sqlite3_bind_int(stmt, idx++, max_size);

    int count = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size) {
        out_array[count].id        = sqlite3_column_int(stmt, 0);
        out_array[count].author_id = sqlite3_column_int(stmt, 1);
        out_array[count].vis       = (enum post_visibility)sqlite3_column_int(stmt, 3);

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

    if (rc != SQLITE_DONE) {
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

void format_posts_for_client(char *buf, int buf_size,
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

    /* 1. Aflăm autorul postării (cu lock pe DB) */
    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql_get_author, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[posts] prepare get author failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, post_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        author_id = sqlite3_column_int(stmt, 0);
    } else if (rc == SQLITE_DONE) {
        /* nu există postarea */
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return 0;
    } else {
        fprintf(stderr, "[posts] get author error: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    /* 2. Verificăm permisiunea ÎN AFARA lock-ului pe db_mutex:
     *    - dacă requester == author -> OK
     *    - altfel, dacă e admin -> OK
     *    - altfel -> FORBIDDEN
     */
    if (requester_id != author_id) {
        int is_admin = auth_is_admin(requester_id);  // aceasta își ia singură db_mutex
        if (is_admin <= 0) {
            return -2; // nu are voie
        }
    }

    /* 3. Ștergem postarea (iar luăm mutex-ul) */
    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql_delete, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[posts] prepare delete failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, post_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[posts] delete failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    int changes = sqlite3_changes(g_db);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    if (changes == 0)
        return 0;   // nimic șters (poate a șters altcineva între timp)
    return 1;       // șters cu succes
}


int posts_get_for_user(int viewer_id, int target_user_id,
                       struct Post *out_array, int max_size)
{
    if (max_size <= 0 || target_user_id <= 0)
        return 0;

    /* aflam daca viewer-ul este admin (in afara lock-ului ca sa nu blocam db_mutex de 2 ori) */
    int is_admin = 0;
    if (viewer_id > 0) {
        is_admin = (auth_is_admin(viewer_id) == 1);
    }

    sqlite3_stmt *stmt = NULL;
    int rc;
    int count = 0;

    pthread_mutex_lock(&db_mutex);

    /* 1. aflam vizibilitatea profilului target-ului */
    int target_vis = USER_PUBLIC;

    const char *sql_vis =
        "SELECT vis FROM users WHERE id = ? LIMIT 1;";

    rc = sqlite3_prepare_v2(g_db, sql_vis, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[posts_get_for_user] prepare vis failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, target_user_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        target_vis = sqlite3_column_int(stmt, 0);
    } else {
        /* user-ul target nu exista */
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return 0;
    }

    sqlite3_finalize(stmt);

    /* 2. admin sau self -> vede TOT */
    if (is_admin || (viewer_id > 0 && viewer_id == target_user_id)) {
        const char *sql_all =
            "SELECT p.id, p.author_id, u.name, p.visibility, p.content "
            "FROM posts p "
            "JOIN users u ON u.id = p.author_id "
            "WHERE p.author_id = ? "
            "ORDER BY p.created_at DESC "
            "LIMIT ?;";

        rc = sqlite3_prepare_v2(g_db, sql_all, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[posts_get_for_user] prepare all failed: %s\n",
                    sqlite3_errmsg(g_db));
            pthread_mutex_unlock(&db_mutex);
            return -1;
        }

        sqlite3_bind_int(stmt, 1, target_user_id);
        sqlite3_bind_int(stmt, 2, max_size);

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size) {
            out_array[count].id        = sqlite3_column_int(stmt, 0);
            out_array[count].author_id = sqlite3_column_int(stmt, 1);
            out_array[count].vis       = (enum post_visibility)sqlite3_column_int(stmt, 3);

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

    /* 3. viewer neautentificat -> doar public, si doar daca profilul e public */
    if (viewer_id <= 0) {
        if (target_vis != USER_PUBLIC) {
            pthread_mutex_unlock(&db_mutex);
            return 0;
        }

        const char *sql_public =
            "SELECT p.id, p.author_id, u.name, p.visibility, p.content "
            "FROM posts p "
            "JOIN users u ON u.id = p.author_id "
            "WHERE p.author_id = ? AND p.visibility = ? "
            "ORDER BY p.created_at DESC "
            "LIMIT ?;";

        rc = sqlite3_prepare_v2(g_db, sql_public, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[posts_get_for_user] prepare public failed: %s\n",
                    sqlite3_errmsg(g_db));
            pthread_mutex_unlock(&db_mutex);
            return -1;
        }

        sqlite3_bind_int(stmt, 1, target_user_id);
        sqlite3_bind_int(stmt, 2, VIS_PUBLIC);
        sqlite3_bind_int(stmt, 3, max_size);

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size) {
            out_array[count].id        = sqlite3_column_int(stmt, 0);
            out_array[count].author_id = sqlite3_column_int(stmt, 1);
            out_array[count].vis       = (enum post_visibility)sqlite3_column_int(stmt, 3);

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

    /* 4. viewer logat, nu admin, nu self:
     *    avem nevoie de prietenie MUTUALA;
     *    pentru CLOSE conteaza daca TARGET l-a marcat pe VIEWER ca FRIEND_CLOSE
     */

    bool has_1 = false, has_2 = false;
    enum friend_type t1 = FRIEND_NORMAL, t2 = FRIEND_NORMAL;

    /* viewer -> target */
    const char *sql_f1 =
        "SELECT type FROM friends WHERE user_id = ? AND friend_id = ? LIMIT 1;";

    rc = sqlite3_prepare_v2(g_db, sql_f1, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[posts_get_for_user] prepare f1 failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, viewer_id);
    sqlite3_bind_int(stmt, 2, target_user_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        has_1 = true;
        t1 = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    /* target -> viewer */
    const char *sql_f2 =
        "SELECT type FROM friends WHERE user_id = ? AND friend_id = ? LIMIT 1;";

    rc = sqlite3_prepare_v2(g_db, sql_f2, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[posts_get_for_user] prepare f2 failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, target_user_id);
    sqlite3_bind_int(stmt, 2, viewer_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        has_2 = true;
        t2 = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    bool are_friends = (has_1 && has_2);
    /* close = mutual friends + target (author) l-a marcat pe viewer ca FRIEND_CLOSE */
    bool is_close_from_target = (has_2 && t2 == FRIEND_CLOSE);
    bool are_close            = (are_friends && is_close_from_target);

    /* daca profilul este privat si nu sunt prieteni mutuali -> nu vede nimic */
    if (target_vis == USER_PRIVATE && !are_friends) {
        pthread_mutex_unlock(&db_mutex);
        return 0;
    }

    int allow_public  = 0;
    int allow_friend  = 0;
    int allow_close   = 0;

    if (target_vis == USER_PUBLIC) {
        /* profil public -> PUBLIC vizibil chiar daca nu sunt prieteni */
        allow_public = 1;
    } else {
        /* profil privat -> PUBLIC doar pentru prieteni */
        allow_public = are_friends ? 1 : 0;
    }

    allow_friend = are_friends ? 1 : 0;
    allow_close  = are_close   ? 1 : 0;

    if (!allow_public && !allow_friend && !allow_close) {
        pthread_mutex_unlock(&db_mutex);
        return 0;
    }

    const char *sql_sel =
        "SELECT p.id, p.author_id, u.name, p.visibility, p.content "
        "FROM posts p "
        "JOIN users u ON u.id = p.author_id "
        "WHERE p.author_id = ? AND ( "
        "      (? AND p.visibility = ?) "        /* allow_public  -> VIS_PUBLIC */
        "   OR (? AND p.visibility = ?) "        /* allow_friend  -> VIS_FRIENDS */
        "   OR (? AND p.visibility = ?) "        /* allow_close   -> VIS_CLOSE_FRIENDS */
        ") "
        "ORDER BY p.created_at DESC "
        "LIMIT ?;";

    rc = sqlite3_prepare_v2(g_db, sql_sel, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
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

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size) {
        out_array[count].id        = sqlite3_column_int(stmt, 0);
        out_array[count].author_id = sqlite3_column_int(stmt, 1);
        out_array[count].vis       = (enum post_visibility)sqlite3_column_int(stmt, 3);

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

