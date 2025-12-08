#include <stdio.h>
#include <string.h>
#include <time.h>

#include <sqlite3.h>
#include <pthread.h>

#include "group_chat.h"
#include "storage.h"
#include "auth.h"
#include "sessions.h"
#include "models.h"

extern sqlite3 *g_db;
extern pthread_mutex_t db_mutex;

/* Helper: găsim un grup după nume */
static int find_group_by_name(const char *name, int *out_conv_id,
                              int *out_visibility)
{
    const char *sql =
        "SELECT id, visibility FROM conversations "
        "WHERE is_group = 1 AND title = ? "
        "LIMIT 1;";

    sqlite3_stmt *stmt;
    int rc;

    *out_conv_id = -1;
    if (out_visibility) *out_visibility = GROUP_PUBLIC;

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[group] find_group_by_name prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        *out_conv_id = sqlite3_column_int(stmt, 0);
        if (out_visibility) {
            *out_visibility = sqlite3_column_int(stmt, 1);
        }
    }

    sqlite3_finalize(stmt);

    if (*out_conv_id == -1)
        return 0; // not found

    return 1; // found
}

/* Creează grup + setează creatorul ca admin & membru */
int group_create(const char *name, int creator_id,
                 enum group_visibility vis,
                 int *out_conversation_id)
{
    if (!name || name[0] == '\0' || creator_id <= 0)
        return -1;

    const char *sql_check =
        "SELECT id FROM conversations "
        "WHERE is_group = 1 AND title = ? "
        "LIMIT 1;";

    const char *sql_insert_conv =
        "INSERT INTO conversations(title, is_group, visibility, created_by, created_at) "
        "VALUES (?, 1, ?, ?, ?);";

    const char *sql_insert_member =
        "INSERT INTO conversation_members(conversation_id, user_id, is_admin, joined_at) "
        "VALUES (?, ?, 1, ?);";

    sqlite3_stmt *stmt;
    int rc;
    int conv_id = -1;
    int now_ts = (int)time(NULL);

    pthread_mutex_lock(&db_mutex);

    /* 1. verificăm dacă există deja un grup cu numele ăsta */
    rc = sqlite3_prepare_v2(g_db, sql_check, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[group_create] check prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        /* deja există */
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -2;
    }
    sqlite3_finalize(stmt);

    /* 2. inserăm conversația (grupul) */
    rc = sqlite3_prepare_v2(g_db, sql_insert_conv, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[group_create] insert conv prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, vis);
    sqlite3_bind_int(stmt, 3, creator_id);
    sqlite3_bind_int(stmt, 4, now_ts);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[group_create] insert conv failed: %s\n",
                sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    conv_id = (int)sqlite3_last_insert_rowid(g_db);
    sqlite3_finalize(stmt);

    /* 3. inserăm creatorul ca membru + admin */
    rc = sqlite3_prepare_v2(g_db, sql_insert_member, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[group_create] insert member prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, conv_id);
    sqlite3_bind_int(stmt, 2, creator_id);
    sqlite3_bind_int(stmt, 3, now_ts);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[group_create] insert member failed: %s\n",
                sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    if (out_conversation_id)
        *out_conversation_id = conv_id;

    return 0;
}

/* helper: verifică dacă user este deja membru în grup */
static int is_member_of_group(int conversation_id, int user_id)
{
    const char *sql =
        "SELECT 1 FROM conversation_members "
        "WHERE conversation_id = ? AND user_id = ? "
        "LIMIT 1;";

    sqlite3_stmt *stmt;
    int rc;
    int found = 0;

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[group] is_member prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        return -1;
    }

    sqlite3_bind_int(stmt, 1, conversation_id);
    sqlite3_bind_int(stmt, 2, user_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
        found = 1;

    sqlite3_finalize(stmt);
    return found;
}

/* helper: inserăm cerere de join (dacă nu există) */
static int insert_join_request(int conversation_id, int user_id)
{
    const char *sql_check =
        "SELECT 1 FROM group_join_requests "
        "WHERE conversation_id = ? AND user_id = ? "
        "LIMIT 1;";

    const char *sql_ins =
        "INSERT INTO group_join_requests(conversation_id, user_id, requested_at) "
        "VALUES (?, ?, ?);";

    sqlite3_stmt *stmt;
    int rc;
    int now_ts = (int)time(NULL);

    /* verificăm dacă există deja o cerere */
    rc = sqlite3_prepare_v2(g_db, sql_check, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[group] join_req check prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        return -1;
    }

    sqlite3_bind_int(stmt, 1, conversation_id);
    sqlite3_bind_int(stmt, 2, user_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return 1; // already requested
    }
    sqlite3_finalize(stmt);

    /* inserăm cerere nouă */
    rc = sqlite3_prepare_v2(g_db, sql_ins, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[group] join_req insert prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        return -1;
    }

    sqlite3_bind_int(stmt, 1, conversation_id);
    sqlite3_bind_int(stmt, 2, user_id);
    sqlite3_bind_int(stmt, 3, now_ts);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[group] join_req insert failed: %s\n",
                sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        return -1;
    }

    sqlite3_finalize(stmt);
    return 0;
}

/* JOIN_GROUP: comportament combinat pt. public / privat */
int group_join(const char *name, int user_id)
{
    if (!name || name[0] == '\0' || user_id <= 0)
        return -1;

    int conv_id = -1;
    int vis = GROUP_PUBLIC;

    pthread_mutex_lock(&db_mutex);

    int f = find_group_by_name(name, &conv_id, &vis);
    if (f <= 0) {
        pthread_mutex_unlock(&db_mutex);
        return -2; // no group
    }

    int member = is_member_of_group(conv_id, user_id);
    if (member < 0) {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    } else if (member == 1) {
        pthread_mutex_unlock(&db_mutex);
        return -3; // already member
    }

    if (vis == GROUP_PUBLIC) {
        /* intră direct */
        const char *sql_ins =
            "INSERT INTO conversation_members(conversation_id, user_id, is_admin, joined_at) "
            "VALUES (?, ?, 0, ?);";

        sqlite3_stmt *stmt;
        int rc;

        rc = sqlite3_prepare_v2(g_db, sql_ins, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "[group_join] insert member prepare failed: %s\n",
                    sqlite3_errmsg(g_db));
            pthread_mutex_unlock(&db_mutex);
            return -1;
        }

        sqlite3_bind_int(stmt, 1, conv_id);
        sqlite3_bind_int(stmt, 2, user_id);
        sqlite3_bind_int(stmt, 3, (int)time(NULL));

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "[group_join] insert member failed: %s\n",
                    sqlite3_errmsg(g_db));
            sqlite3_finalize(stmt);
            pthread_mutex_unlock(&db_mutex);
            return -1;
        }

        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return 0; // joined
    } else {
        /* privat -> cerere de join */
        int rc = insert_join_request(conv_id, user_id);
        pthread_mutex_unlock(&db_mutex);

        if (rc == 1)
            return -5; // already requested
        if (rc < 0)
            return -1;
        return 0; // request created
    }
}

int group_request_join(const char *name, int user_id)
{
    if (!name || name[0] == '\0' || user_id <= 0)
        return -1;

    int conv_id = -1;
    int vis = GROUP_PUBLIC;

    pthread_mutex_lock(&db_mutex);

    int f = find_group_by_name(name, &conv_id, &vis);
    if (f <= 0) {
        pthread_mutex_unlock(&db_mutex);
        return -2; // group not found
    }

    if (vis != GROUP_PRIVATE) {
        pthread_mutex_unlock(&db_mutex);
        return -3; // not private
    }

    int member = is_member_of_group(conv_id, user_id);
    if (member < 0) {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    } else if (member == 1) {
        pthread_mutex_unlock(&db_mutex);
        return -4; // already member
    }

    int rc = insert_join_request(conv_id, user_id);
    pthread_mutex_unlock(&db_mutex);

    if (rc == 1)
        return -5; // already requested
    if (rc < 0)
        return -1;
    return 0;
}

/* helper: verificăm dacă user este admin în grup */
static int is_admin_in_group(int conversation_id, int user_id)
{
    const char *sql =
        "SELECT is_admin FROM conversation_members "
        "WHERE conversation_id = ? AND user_id = ? "
        "LIMIT 1;";

    sqlite3_stmt *stmt;
    int rc;
    int is_admin = 0;

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[group] is_admin prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        return -1;
    }

    sqlite3_bind_int(stmt, 1, conversation_id);
    sqlite3_bind_int(stmt, 2, user_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        is_admin = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return is_admin;
}

/* Admin aprobă cererea unui user */
int group_approve_member(const char *group_name, int admin_id,
                         const char *username)
{
    if (!group_name || !username || group_name[0] == '\0' || username[0] == '\0')
        return -1;

    int user_id = auth_get_user_id_by_name(username);
    if (user_id <= 0)
        return -4; // user not found (poți schimba codul de eroare dacă vrei altceva)

    int conv_id = -1;
    int vis = GROUP_PUBLIC;

    pthread_mutex_lock(&db_mutex);

    int f = find_group_by_name(group_name, &conv_id, &vis);
    if (f <= 0) {
        pthread_mutex_unlock(&db_mutex);
        return -2; // group not found
    }

    int adm = is_admin_in_group(conv_id, admin_id);
    if (adm <= 0) {
        pthread_mutex_unlock(&db_mutex);
        return -3; // not admin
    }

    /* verificăm dacă există cerere */
    const char *sql_check =
        "SELECT 1 FROM group_join_requests "
        "WHERE conversation_id = ? AND user_id = ? "
        "LIMIT 1;";

    sqlite3_stmt *stmt;
    int rc;

    rc = sqlite3_prepare_v2(g_db, sql_check, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[group_approve] check prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, conv_id);
    sqlite3_bind_int(stmt, 2, user_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -4; // no request
    }
    sqlite3_finalize(stmt);

    /* 1) ștergem cererea */
    const char *sql_del =
        "DELETE FROM group_join_requests "
        "WHERE conversation_id = ? AND user_id = ?;";

    rc = sqlite3_prepare_v2(g_db, sql_del, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[group_approve] delete prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, conv_id);
    sqlite3_bind_int(stmt, 2, user_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[group_approve] delete failed: %s\n",
                sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }
    sqlite3_finalize(stmt);

    /* 2) adăugăm user-ul în members */
    const char *sql_ins =
        "INSERT OR IGNORE INTO conversation_members(conversation_id, user_id, is_admin, joined_at) "
        "VALUES (?, ?, 0, ?);";

    rc = sqlite3_prepare_v2(g_db, sql_ins, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[group_approve] insert member prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, conv_id);
    sqlite3_bind_int(stmt, 2, user_id);
    sqlite3_bind_int(stmt, 3, (int)time(NULL));

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[group_approve] insert member failed: %s\n",
                sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return 0;
}

/* trimitem mesaj în grup + notificăm toți membrii */
int group_send_message(const char *group_name, int sender_id,
                       const char *content)
{
    if (!group_name || !content || group_name[0] == '\0' || content[0] == '\0')
        return -1;
    if (sender_id <= 0)
        return -1;

    int conv_id = -1;
    int vis = GROUP_PUBLIC;

    pthread_mutex_lock(&db_mutex);

    int f = find_group_by_name(group_name, &conv_id, &vis);
    if (f <= 0) {
        pthread_mutex_unlock(&db_mutex);
        return -2; // group not found
    }

    /* verificăm dacă sender este membru */
    int member = is_member_of_group(conv_id, sender_id);
    if (member <= 0) {
        pthread_mutex_unlock(&db_mutex);
        return -3; // not member
    }

    /* 1) inserăm mesajul în DB */
    const char *sql_msg =
        "INSERT INTO messages(conversation_id, sender_id, content, created_at) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    int rc;

    rc = sqlite3_prepare_v2(g_db, sql_msg, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[group_send] insert msg prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, conv_id);
    sqlite3_bind_int(stmt, 2, sender_id);
    sqlite3_bind_text(stmt, 3, content, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, (int)time(NULL));

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[group_send] insert msg failed: %s\n",
                sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_finalize(stmt);

    /* 2) luăm lista de membri ca să-i notificăm */
    const char *sql_members =
        "SELECT user_id FROM conversation_members "
        "WHERE conversation_id = ?;";

    rc = sqlite3_prepare_v2(g_db, sql_members, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[group_send] select members prepare failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, conv_id);

    /* pregătim numele sender-ului pentru mesajul către clienți */
    char sender_name[64];
    auth_get_username_by_id(sender_id, sender_name, sizeof(sender_name));

    char notif[2048];

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int uid = sqlite3_column_int(stmt, 0);

        int fd = sessions_find_fd_by_user_id(uid);
        if (fd >= 0) {
            /* poți decide dacă să notifici și sender-ul sau nu */
            snprintf(notif, sizeof(notif),
                     "NEW_GROUP_MSG %s\nFrom: %s\nContent: %s\n",
                     group_name,
                     sender_name,
                     content);

            send(fd, notif, strlen(notif), 0);
        }
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return 0;
}
