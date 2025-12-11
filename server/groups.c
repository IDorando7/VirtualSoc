#include "common.h"
#include "groups.h"
#include "auth.h"
#include "storage.h"

#include <sqlite3.h>
#include <string.h>
#include <time.h>

extern sqlite3 *g_db;
extern pthread_mutex_t db_mutex;

static int groups_get_info(const char *name, int *out_group_id, int *out_is_public, int *out_owner_id)
{
    const char *sql =
        "SELECT id, is_public, owner_id FROM groups WHERE name = ?;";

    sqlite3_stmt *stmt;
    int rc;

    *out_group_id = -1;
    if (out_is_public)
        *out_is_public = 0;
    if (out_owner_id)
        *out_owner_id  = -1;

    rc = sqlite3_prepare_v3(g_db, sql, -1, 0, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_get_info] prepare failed: %s\n", sqlite3_errmsg(g_db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        *out_group_id = sqlite3_column_int(stmt, 0);
        if (out_is_public)
            *out_is_public = sqlite3_column_int(stmt, 1);
        if (out_owner_id)
            *out_owner_id = sqlite3_column_int(stmt, 2);
        sqlite3_finalize(stmt);
        return 0;
    } else {
        sqlite3_finalize(stmt);
        return GROUP_ERR_NOT_FOUND;
    }
}

int groups_create(int owner_id, const char *name, int is_public)
{
    if (owner_id <= 0 || !name || !*name)
        return -1;

    pthread_mutex_lock(&db_mutex);

    const char *sql_check = "SELECT id FROM groups WHERE name = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v3(g_db, sql_check, -1, 0, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_create] prepare check failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_EXISTS;
    }
    sqlite3_finalize(stmt);

    const char *sql_insert_group =
        "INSERT INTO groups(name, owner_id, is_public) VALUES(?, ?, ?);";

    rc = sqlite3_prepare_v3(g_db, sql_insert_group, -1, 0, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_create] prepare insert failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, owner_id);
    sqlite3_bind_int(stmt, 3, is_public ? 1 : 0);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[groups_create] insert group failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    int group_id = (int)sqlite3_last_insert_rowid(g_db);
    sqlite3_finalize(stmt);

    const char *sql_insert_member =
        "INSERT INTO group_members(group_id, user_id, role) VALUES(?, ?, 1);";

    rc = sqlite3_prepare_v3(g_db, sql_insert_member, -1, 0, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_create] prepare insert member failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, owner_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[groups_create] insert member failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return GROUP_OK;
}

int groups_join_public(int user_id, const char *group_name)
{
    if (user_id <= 0 || !group_name)
        return -1;

    pthread_mutex_lock(&db_mutex);

    int group_id, is_public;
    int rc_info = groups_get_info(group_name, &group_id, &is_public, NULL);
    if (rc_info == GROUP_ERR_NOT_FOUND)
    {
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_FOUND;
    } else if (rc_info < 0)
    {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    if (!is_public)
    {
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_PUBLIC;
    }

    const char *sql_check =
        "SELECT 1 FROM group_members WHERE group_id = ? AND user_id = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v3(g_db, sql_check, -1, 0, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_join_public] prepare check member failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, user_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_ALREADY_MEMBER;
    }
    sqlite3_finalize(stmt);

    const char *sql_insert =
        "INSERT INTO group_members(group_id, user_id, role) VALUES(?, ?, 0);";

    rc = sqlite3_prepare_v3(g_db, sql_insert, -1, 0, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_join_public] prepare insert failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, user_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[groups_join_public] insert failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return GROUP_OK;
}

int groups_request_join(int user_id, const char *group_name)
{
    if (user_id <= 0 || !group_name)
        return -1;

    pthread_mutex_lock(&db_mutex);
    int group_id, is_public;
    int rc_info = groups_get_info(group_name, &group_id, &is_public, NULL);
    if (rc_info == GROUP_ERR_NOT_FOUND)
    {
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_FOUND;
    }
    else if (rc_info < 0)
    {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    const char *sql_check_member =
        "SELECT 1 FROM group_members WHERE group_id = ? AND user_id = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v3(g_db, sql_check_member, -1, 0, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_request_join] prepare check member failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, user_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_ALREADY_MEMBER;
    }
    sqlite3_finalize(stmt);

    const char *sql_insert_req =
        "INSERT OR IGNORE INTO group_requests(group_id, user_id) VALUES(?, ?);";

    rc = sqlite3_prepare_v3(g_db, sql_insert_req, -1, 0, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_request_join] prepare insert failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, user_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[groups_request_join] insert failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return GROUP_OK;
}

int groups_approve_member(int admin_id, const char *group_name, const char *username)
{
    if (admin_id <= 0 || !group_name || !username)
        return -1;

    pthread_mutex_lock(&db_mutex);

    int group_id, is_public, owner_id;
    int rc_info = groups_get_info(group_name, &group_id, &is_public, &owner_id);
    if (rc_info == GROUP_ERR_NOT_FOUND)
    {
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_FOUND;
    }
    else if (rc_info < 0)
    {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    const char *sql_check_admin =
        "SELECT role FROM group_members WHERE group_id = ? AND user_id = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v3(g_db, sql_check_admin, -1, 0, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_approve_member] prepare check admin failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, admin_id);

    int is_admin = 0;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        int role = sqlite3_column_int(stmt, 0);
        if (role == 1) is_admin = 1;
    }
    sqlite3_finalize(stmt);

    if (!is_admin && admin_id != owner_id)
    {
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_ADMIN;
    }

    const char *sql_user =
        "SELECT id FROM users WHERE name = ?;";
    rc = sqlite3_prepare_v3(g_db, sql_user, -1, 0, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_approve_member] prepare user failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    int user_id = -1;
    if (rc == SQLITE_ROW)
        user_id = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (user_id <= 0)
    {
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_FOUND;
    }

    const char *sql_check_req =
        "SELECT 1 FROM group_requests WHERE group_id = ? AND user_id = ?;";
    rc = sqlite3_prepare_v3(g_db, sql_check_req, -1, 0, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_approve_member] prepare check req failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, user_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NO_REQUEST;
    }
    sqlite3_finalize(stmt);

    const char *sql_del_req =
        "DELETE FROM group_requests WHERE group_id = ? AND user_id = ?;";
    rc = sqlite3_prepare_v3(g_db, sql_del_req, -1, 0, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_approve_member] prepare del req failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, user_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    const char *sql_insert_member =
        "INSERT OR IGNORE INTO group_members(group_id, user_id, role) VALUES(?, ?, 0);";
    rc = sqlite3_prepare_v3(g_db, sql_insert_member, -1, 0, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_approve_member] prepare insert member failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }
    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, user_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[groups_approve_member] insert member failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }
    sqlite3_finalize(stmt);

    pthread_mutex_unlock(&db_mutex);
    return GROUP_OK;
}

int groups_send_group_msg(int sender_id, const char *group_name, const char *text)
{
    if (sender_id <= 0 || !group_name || !text || !*text)
        return -1;

    pthread_mutex_lock(&db_mutex);

    int group_id;
    int rc_info = groups_get_info(group_name, &group_id, NULL, NULL);
    if (rc_info == GROUP_ERR_NOT_FOUND)
    {
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_FOUND;
    }
    else if (rc_info < 0)
    {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    const char *sql_check_member =
        "SELECT 1 FROM group_members WHERE group_id = ? AND user_id = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v3(g_db, sql_check_member, -1, 0, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_send_group_msg] prepare check member failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, sender_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NO_PERMISSION;
    }
    sqlite3_finalize(stmt);

    const char *sql_insert_msg =
        "INSERT INTO group_messages(group_id, sender_id, content, created_at) "
        "VALUES(?, ?, ?, ?);";

    rc = sqlite3_prepare_v3(g_db, sql_insert_msg, -1, 0, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_send_group_msg] prepare insert failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, sender_id);
    sqlite3_bind_text(stmt, 3, text, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, (int)time(NULL));

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[groups_send_group_msg] insert failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return GROUP_OK;
}

int groups_leave(int user_id, const char *group_name)
{
    const char *sql_find_group =
        "SELECT id FROM groups WHERE name = ?;";

    const char *sql_remove_member =
        "DELETE FROM group_members WHERE user_id = ? AND group_id = ?;";

    sqlite3_stmt *stmt;
    int rc;
    int group_id = -1;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql_find_group, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, group_name, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW)
        group_id = sqlite3_column_int(stmt, 0);
    else
    {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_FOUND;
    }
    sqlite3_finalize(stmt);

    rc = sqlite3_prepare_v2(g_db, sql_remove_member, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, group_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    int changes = sqlite3_changes(g_db);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    if (changes == 0)
        return GROUP_ERR_NO_PERMISSION;
    return GROUP_OK;
}

int groups_view_members(int requester_id, const char *group_name, struct GroupMemberInfo *out_array, int max_size)
{
    if (requester_id <= 0 || !group_name || !out_array || max_size <= 0)
        return -1;

    const char *sql_find_group =
        "SELECT id FROM groups WHERE name = ?;";

    const char *sql_check_member =
        "SELECT COUNT(*) "
        "FROM group_members "
        "WHERE group_id = ? AND user_id = ?;";

    const char *sql_list_members =
        "SELECT gm.user_id, u.name, gm.role "
        "FROM group_members gm "
        "JOIN users u ON u.id = gm.user_id "
        "WHERE gm.group_id = ? "
        "ORDER BY u.name;";

    sqlite3_stmt *stmt = NULL;
    int rc;
    int group_id = -1;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql_find_group, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups] prepare find_group failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, group_name, -1, SQLITE_TRANSIENT);

    if ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
        group_id = sqlite3_column_int(stmt, 0);
    else {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_FOUND;
    }
    sqlite3_finalize(stmt);

    rc = sqlite3_prepare_v2(g_db, sql_check_member, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups] prepare check_member failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, requester_id);

    int is_member = 0;
    if ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
        is_member = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (is_member <= 0)
    {
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NO_PERMISSION;
    }

    rc = sqlite3_prepare_v2(g_db, sql_list_members, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups] prepare list_members failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, group_id);
    int count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size)
    {
        out_array[count].user_id = sqlite3_column_int(stmt, 0);

        const unsigned char *uname = sqlite3_column_text(stmt, 1);
        if (uname)
        {
            strncpy(out_array[count].username,
                    (const char *)uname,
                    sizeof(out_array[count].username) - 1);
            out_array[count].username[
                sizeof(out_array[count].username) - 1] = '\0';
        }
        else
        {
            out_array[count].username[0] = '\0';
        }

        int role = sqlite3_column_int(stmt, 2);
        out_array[count].is_admin = (role != 0);
        count++;
    }

    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[groups] list_members error: %s\n",
                sqlite3_errmsg(g_db));
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return count;
}

int groups_list_for_user(int user_id, struct GroupInfo *out_array, int max_size)
{
    if (user_id <= 0 || !out_array || max_size <= 0)
        return -1;

    const char *sql =
        "SELECT g.id, g.name, g.is_public, gm.role "
        "FROM group_members gm "
        "JOIN groups g ON g.id = gm.group_id "
        "WHERE gm.user_id = ? "
        "ORDER BY g.name;";

    sqlite3_stmt *stmt = NULL;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups] prepare list_for_user failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, user_id);

    int count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size)
    {
        out_array[count].group_id  = sqlite3_column_int(stmt, 0);

        const unsigned char *gname = sqlite3_column_text(stmt, 1);
        if (gname)
        {
            strncpy(out_array[count].name,
                    (const char *)gname,
                    sizeof(out_array[count].name) - 1);
            out_array[count].name[sizeof(out_array[count].name) - 1] = '\0';
        }
        else
        {
            out_array[count].name[0] = '\0';
        }

        out_array[count].is_public = sqlite3_column_int(stmt, 2);

        int role = sqlite3_column_int(stmt, 3);
        out_array[count].is_admin = (role != 0);

        count++;
    }

    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[groups] list_for_user error: %s\n",
                sqlite3_errmsg(g_db));
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return count;
}

void format_group_messages_for_client(char *buf, int buf_size, const char *group_name, struct Message *msgs, int count, int current_user_id)
{
    if (buf_size == 0)
        return;

    buf[0] = '\0';
    int offset = 0;

    offset += snprintf(buf + offset, buf_size - offset,
                       "\033[32mOK\033[0m\n"
                       "GROUP_MESSAGES %d\n"
                       "Group: \033[36m%s\033[0m\n\n",
                       count, group_name);

    for (int i = 0; i < count; i++)
    {
        if (offset >= (int)buf_size - 1)
            break;

        struct Message *m = &msgs[i];
        char timebuf[64] = {0};
        time_t t = m->created_at;
        struct tm tm_info;
        localtime_r(&t, &tm_info);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm_info);

        const char *side = msg_side_label(m->sender_id, current_user_id);
        const char *sender_color = msg_sender_color(m->sender_id, current_user_id);

        offset += snprintf(
            buf + offset, buf_size - offset,
            "\033[90m========== Group Message #%d ==========\033[0m\n"
            "\033[35mGroup:\033[0m %s\n"
            "\033[35mFrom:\033[0m %s%s\033[0m (%s)\n"
            "\033[35mTime:\033[0m %s\n"
            "\033[35mContent:\033[0m\n%s\n\n",
            m->id,
            group_name,
            sender_color,
            m->sender_name,
            side,
            timebuf,
            m->content
        );
    }
}

int groups_get_group_history(int requester_id, const char *group_name, struct Message *out_array, int max_size)
{
    if (requester_id <= 0 || !group_name || !*group_name ||
        !out_array || max_size <= 0)
        return -1;

    const char *sql_find_group =
        "SELECT id FROM groups WHERE name = ?;";

    const char *sql_check_member =
        "SELECT COUNT(*) "
        "FROM group_members "
        "WHERE group_id = ? AND user_id = ?;";

    const char *sql_list_msgs =
        "SELECT gm.id, gm.sender_id, u.name, gm.content, gm.created_at "
        "FROM group_messages gm "
        "JOIN users u ON u.id = gm.sender_id "
        "WHERE gm.group_id = ? "
        "ORDER BY gm.created_at ASC "
        "LIMIT ?;";

    sqlite3_stmt *stmt = NULL;
    int rc;
    int group_id = -1;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql_find_group, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_get_group_history] prepare find_group failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, group_name, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
        group_id = sqlite3_column_int(stmt, 0);
    else {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -2;
    }
    sqlite3_finalize(stmt);

    rc = sqlite3_prepare_v2(g_db, sql_check_member, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_get_group_history] prepare check_member failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, requester_id);

    int is_member = 0;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
        is_member = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (is_member <= 0)
    {
        pthread_mutex_unlock(&db_mutex);
        return -3;
    }

    rc = sqlite3_prepare_v2(g_db, sql_list_msgs, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_get_group_history] prepare list_msgs failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, max_size);

    int count = 0;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size)
    {
        struct Message *m = &out_array[count];

        m->id              = sqlite3_column_int(stmt, 0);
        m->conversation_id = group_id;
        m->sender_id       = sqlite3_column_int(stmt, 1);

        const unsigned char *uname = sqlite3_column_text(stmt, 2);
        if (uname) {
            strncpy(m->sender_name,
                    (const char*)uname,
                    sizeof(m->sender_name) - 1);
            m->sender_name[sizeof(m->sender_name) - 1] = '\0';
        } else {
            m->sender_name[0] = '\0';
        }

        const unsigned char *txt = sqlite3_column_text(stmt, 3);
        if (txt) {
            strncpy(m->content,
                    (const char*)txt,
                    sizeof(m->content) - 1);
            m->content[sizeof(m->content) - 1] = '\0';
        } else {
            m->content[0] = '\0';
        }

        m->created_at = sqlite3_column_int(stmt, 4);

        count++;
    }

    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[groups_get_group_history] list_msgs error: %s\n",
                sqlite3_errmsg(g_db));
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return count;
}

int groups_set_visibility(int admin_id, const char *group_name, int is_public)
{
    if (admin_id <= 0 || !group_name || !*group_name)
        return -1;

    const char *sql_find_group =
        "SELECT id, owner_id, is_public FROM groups WHERE name = ?;";

    const char *sql_check_admin =
        "SELECT role FROM group_members WHERE group_id = ? AND user_id = ?;";

    const char *sql_update_vis =
        "UPDATE groups SET is_public = ? WHERE id = ?;";

    sqlite3_stmt *stmt = NULL;
    int rc;
    int group_id = -1;
    int owner_id = -1;
    int current_is_public = 0;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql_find_group, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_set_visibility] prepare find_group failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, group_name, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        group_id         = sqlite3_column_int(stmt, 0);
        owner_id         = sqlite3_column_int(stmt, 1);
        current_is_public = sqlite3_column_int(stmt, 2);
    }
    else
    {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_FOUND;
    }
    sqlite3_finalize(stmt);

    if (current_is_public == (is_public ? 1 : 0)) {
        pthread_mutex_unlock(&db_mutex);
        return GROUP_OK;
    }

    int is_admin = 0;
    if (admin_id == owner_id)
    {
        is_admin = 1;
    }
    else
    {
        rc = sqlite3_prepare_v2(g_db, sql_check_admin, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
        {
            fprintf(stderr, "[groups_set_visibility] prepare check_admin failed: %s\n",
                    sqlite3_errmsg(g_db));
            pthread_mutex_unlock(&db_mutex);
            return -1;
        }

        sqlite3_bind_int(stmt, 1, group_id);
        sqlite3_bind_int(stmt, 2, admin_id);

        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW)
        {
            int role = sqlite3_column_int(stmt, 0);
            if (role == 1)
                is_admin = 1;
        }
        sqlite3_finalize(stmt);
    }

    if (!is_admin)
    {
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_ADMIN;
    }

    rc = sqlite3_prepare_v2(g_db, sql_update_vis, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[groups_set_visibility] prepare update failed: %s\n",
                sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, is_public ? 1 : 0);
    sqlite3_bind_int(stmt, 2, group_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[groups_set_visibility] update failed: %s\n",
                sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return GROUP_OK;
}

int groups_kick_member(int admin_id, const char *group_name, const char *username)
{
    if (admin_id <= 0 || !group_name || !username)
        return -1;

    const char *sql_find_group =
        "SELECT id, owner_id FROM groups WHERE name = ?;";

    const char *sql_check_admin =
        "SELECT role FROM group_members WHERE group_id = ? AND user_id = ?;";

    const char *sql_get_user =
        "SELECT id FROM users WHERE name = ?;";

    const char *sql_delete_member =
        "DELETE FROM group_members WHERE group_id = ? AND user_id = ?;";

    sqlite3_stmt *stmt = NULL;
    int rc;

    int group_id = -1;
    int owner_id = -1;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql_find_group, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, group_name, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        group_id = sqlite3_column_int(stmt, 0);
        owner_id = sqlite3_column_int(stmt, 1);
    }
    else
    {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_FOUND;
    }
    sqlite3_finalize(stmt);

    int is_admin = 0;
    if (admin_id == owner_id) {
        is_admin = 1;
    } else {
        rc = sqlite3_prepare_v2(g_db, sql_check_admin, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            pthread_mutex_unlock(&db_mutex);
            return -1;
        }

        sqlite3_bind_int(stmt, 1, group_id);
        sqlite3_bind_int(stmt, 2, admin_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int role = sqlite3_column_int(stmt, 0);
            if (role == 1) is_admin = 1;
        }
        sqlite3_finalize(stmt);
    }

    if (!is_admin)
    {
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_ADMIN;
    }

    int user_id = -1;

    rc = sqlite3_prepare_v2(g_db, sql_get_user, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW)
        user_id = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    if (user_id <= 0)
    {
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_FOUND;
    }

    if (user_id == owner_id)
    {
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NO_PERMISSION;
    }

    rc = sqlite3_prepare_v2(g_db, sql_delete_member, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, group_id);
    sqlite3_bind_int(stmt, 2, user_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    int changes = sqlite3_changes(g_db);
    pthread_mutex_unlock(&db_mutex);

    if (changes == 0)
        return GROUP_ERR_NO_PERMISSION;
    return GROUP_OK;
}

int groups_list_requests(int admin_id, const char *group_name, struct GroupRequestInfo *out_array, int max_size)
{
    if (admin_id <= 0 || !group_name || !out_array || max_size <= 0)
        return -1;

    const char *sql_find_group =
        "SELECT id, owner_id FROM groups WHERE name = ?;";

    const char *sql_check_admin =
        "SELECT role FROM group_members WHERE group_id = ? AND user_id = ?;";

    const char *sql_list_requests =
        "SELECT gr.user_id, u.name "
        "FROM group_requests gr "
        "JOIN users u ON u.id = gr.user_id "
        "WHERE gr.group_id = ? "
        "ORDER BY u.name;";

    sqlite3_stmt *stmt = NULL;
    int rc;

    int group_id = -1;
    int owner_id = -1;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql_find_group, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, group_name, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        group_id = sqlite3_column_int(stmt, 0);
        owner_id = sqlite3_column_int(stmt, 1);
    }
    else
    {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_FOUND;
    }
    sqlite3_finalize(stmt);

    int is_admin = 0;
    if (admin_id == owner_id)
    {
        is_admin = 1;
    }
    else
    {
        rc = sqlite3_prepare_v2(g_db, sql_check_admin, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
        {
            pthread_mutex_unlock(&db_mutex);
            return -1;
        }

        sqlite3_bind_int(stmt, 1, group_id);
        sqlite3_bind_int(stmt, 2, admin_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            int role = sqlite3_column_int(stmt, 0);
            if (role == 1) is_admin = 1;
        }
        sqlite3_finalize(stmt);
    }

    if (!is_admin)
    {
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_ADMIN;
    }

    rc = sqlite3_prepare_v2(g_db, sql_list_requests, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, group_id);

    int count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size)
    {
        out_array[count].user_id = sqlite3_column_int(stmt, 0);

        const unsigned char *uname = sqlite3_column_text(stmt, 1);
        if (uname)
            strncpy(out_array[count].username, (const char *)uname, sizeof(out_array[count].username) - 1);
        else
            out_array[count].username[0] = '\0';

        count++;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return count;
}

int groups_reject_request(int admin_id, const char *group_name, const char *username)
{
    if (admin_id <= 0 || !group_name || !username)
        return -1;

    const char *sql_find_group =
        "SELECT id, owner_id FROM groups WHERE name = ?;";

    const char *sql_check_admin =
        "SELECT role FROM group_members WHERE group_id = ? AND user_id = ?;";

    const char *sql_find_user =
        "SELECT id FROM users WHERE name = ?;";

    const char *sql_check_request =
        "SELECT 1 FROM group_requests WHERE group_id = ? AND user_id = ?;";

    const char *sql_delete_request =
        "DELETE FROM group_requests WHERE group_id = ? AND user_id = ?;";

    sqlite3_stmt *stmt = NULL;
    int rc;
    int group_id = -1;
    int owner_id = -1;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql_find_group, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, group_name, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        group_id = sqlite3_column_int(stmt, 0);
        owner_id = sqlite3_column_int(stmt, 1);
    }
    else
    {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_FOUND;
    }
    sqlite3_finalize(stmt);

    int is_admin = (admin_id == owner_id);

    if (!is_admin)
    {
        rc = sqlite3_prepare_v2(g_db, sql_check_admin, -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, group_id);
            sqlite3_bind_int(stmt, 2, admin_id);
            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                int role = sqlite3_column_int(stmt, 0);
                if (role == 1) is_admin = 1;
            }
        }
        sqlite3_finalize(stmt);
    }

    if (!is_admin)
    {
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_ADMIN;
    }

    int user_id = -1;
    rc = sqlite3_prepare_v2(g_db, sql_find_user, -1, &stmt, NULL);
    if (rc == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            user_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (user_id <= 0)
    {
        pthread_mutex_unlock(&db_mutex);
        return GROUP_ERR_NOT_FOUND;
    }

    rc = sqlite3_prepare_v2(g_db, sql_check_request, -1, &stmt, NULL);
    if (rc == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, group_id);
        sqlite3_bind_int(stmt, 2, user_id);

        if (sqlite3_step(stmt) != SQLITE_ROW)
        {
            sqlite3_finalize(stmt);
            pthread_mutex_unlock(&db_mutex);
            return GROUP_ERR_NO_REQUEST;
        }
    }
    sqlite3_finalize(stmt);

    rc = sqlite3_prepare_v2(g_db, sql_delete_request, -1, &stmt, NULL);
    if (rc == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, group_id);
        sqlite3_bind_int(stmt, 2, user_id);
        sqlite3_step(stmt);
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return GROUP_OK;
}

