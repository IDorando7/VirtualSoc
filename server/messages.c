#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sqlite3.h>

#include "messages.h"
#include "storage.h"
#include "models.h"

static void sort_pair(int *a, int *b)
{
    if (*a > *b)
    {
        int tmp = *a;
        *a = *b;
        *b = tmp;
    }
}

int messages_find_or_create_dm(int user1_id, int user2_id)
{
    if (user1_id <= 0 || user2_id <= 0)
        return -1;

    sort_pair(&user1_id, &user2_id);

    const char *sql_find =
        "SELECT c.id "
        "FROM conversations c "
        "JOIN conversation_members m1 ON m1.conversation_id = c.id AND m1.user_id = ? "
        "JOIN conversation_members m2 ON m2.conversation_id = c.id AND m2.user_id = ? "
        "WHERE c.is_group = 0 "
        "LIMIT 1;";

    sqlite3_stmt *stmt = NULL;
    int conv_id = -1;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql_find, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[messages] find DM prepare failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, user1_id);
    sqlite3_bind_int(stmt, 2, user2_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        conv_id = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return conv_id;
    }
    sqlite3_finalize(stmt);

    const char *sql_insert_conv =
        "INSERT INTO conversations(title, is_group, visibility, created_by, created_at) "
        "VALUES (?, 0, 2, ?, ?);";

    rc = sqlite3_prepare_v2(g_db, sql_insert_conv, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[messages] insert conv prepare failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, user1_id);
    sqlite3_bind_int(stmt, 3, (int)time(NULL));

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[messages] insert conv failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    conv_id = (int)sqlite3_last_insert_rowid(g_db);
    sqlite3_finalize(stmt);

    const char *sql_insert_member =
        "INSERT INTO conversation_members(conversation_id, user_id, joined_at) "
        "VALUES (?, ?, ?);";

    rc = sqlite3_prepare_v2(g_db, sql_insert_member, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[messages] insert member prepare failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, conv_id);
    sqlite3_bind_int(stmt, 2, user1_id);
    sqlite3_bind_int(stmt, 3, (int)time(NULL));
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[messages] insert member1 failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_reset(stmt);

    sqlite3_bind_int(stmt, 1, conv_id);
    sqlite3_bind_int(stmt, 2, user2_id);
    sqlite3_bind_int(stmt, 3, (int)time(NULL));
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[messages] insert member2 failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return conv_id;
}

int messages_add(int conversation_id, int sender_id, const char *content)
{
    if (conversation_id <= 0 || sender_id <= 0 || !content)
        return -1;
    const char *sql =
        "INSERT INTO messages(conversation_id, sender_id, content, created_at) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    int rc;
    int msg_id = -1;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[messages] insert msg prepare failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, conversation_id);
    sqlite3_bind_int(stmt, 2, sender_id);
    sqlite3_bind_text(stmt, 3, content, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, (int)time(NULL));

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[messages] insert msg failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    msg_id = (int)sqlite3_last_insert_rowid(g_db);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return msg_id;
}

int messages_get_history_dm(int user1_id, int user2_id, struct Message *out_array, int max_size)
{
    if (max_size <= 0)
        return 0;
    sort_pair(&user1_id, &user2_id);
    int conv_id = -1;
    {
        const char *sql_find =
            "SELECT c.id "
            "FROM conversations c "
            "JOIN conversation_members m1 ON m1.conversation_id = c.id AND m1.user_id = ? "
            "JOIN conversation_members m2 ON m2.conversation_id = c.id AND m2.user_id = ? "
            "WHERE c.is_group = 0 "
            "LIMIT 1;";

        sqlite3_stmt *stmt;
        int rc;

        pthread_mutex_lock(&db_mutex);

        rc = sqlite3_prepare_v2(g_db, sql_find, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
        {
            fprintf(stderr, "[messages] history find DM prepare failed: %s\n", sqlite3_errmsg(g_db));
            pthread_mutex_unlock(&db_mutex);
            return -1;
        }

        sqlite3_bind_int(stmt, 1, user1_id);
        sqlite3_bind_int(stmt, 2, user2_id);

        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW)
            conv_id = sqlite3_column_int(stmt, 0);
        else if (rc != SQLITE_DONE)
            fprintf(stderr, "[messages] history find DM error: %s\n", sqlite3_errmsg(g_db));

        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
    }

    if (conv_id <= 0)
        return 0;

    const char *sql_msgs =
        "SELECT m.id, m.conversation_id, m.sender_id, u.name, m.content, m.created_at "
        "FROM messages m "
        "JOIN users u ON u.id = m.sender_id "
        "WHERE m.conversation_id = ? "
        "ORDER BY m.created_at ASC;";

    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql_msgs, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[messages] history prepare failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, conv_id);

    int count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size)
    {
        out_array[count].id             = sqlite3_column_int(stmt, 0);
        out_array[count].conversation_id = sqlite3_column_int(stmt, 1);
        out_array[count].sender_id      = sqlite3_column_int(stmt, 2);

        const unsigned char *name = sqlite3_column_text(stmt, 3);
        if (name) {
            strncpy(out_array[count].sender_name,
                    (const char *)name,
                    sizeof(out_array[count].sender_name) - 1);
            out_array[count].sender_name[sizeof(out_array[count].sender_name) - 1] = '\0';
        } else {
            out_array[count].sender_name[0] = '\0';
        }

        const unsigned char *txt = sqlite3_column_text(stmt, 4);
        if (txt) {
            strncpy(out_array[count].content,
                    (const char *)txt,
                    sizeof(out_array[count].content) - 1);
            out_array[count].content[sizeof(out_array[count].content) - 1] = '\0';
        } else {
            out_array[count].content[0] = '\0';
        }

        out_array[count].created_at = (time_t)sqlite3_column_int(stmt, 5);

        count++;
    }

    if (rc != SQLITE_DONE)
        fprintf(stderr, "[messages] history select error: %s\n", sqlite3_errmsg(g_db));
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return count;
}

const char* msg_side_label(int sender_id, int current_user_id)
{
    return (sender_id == current_user_id) ? "You" : "Them";
}

const char* msg_sender_color(int sender_id, int current_user_id)
{
    return (sender_id == current_user_id) ? "\033[32m" : "\033[36m";
}

void format_messages_for_client(char *buf, size_t buf_size, struct Message *msgs, int count, int current_user_id)
{
    if (buf_size == 0)
        return;

    buf[0] = '\0';
    int offset = 0;

    offset += snprintf(buf + offset, buf_size - offset,
                       "\033[32mOK\033[0m\nMESSAGES %d\n\n", count);

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

        offset += snprintf(buf + offset, buf_size - offset,
                           "\033[90m========== Message #%d ==========\033[0m\n"
                           "\033[35mConversation:\033[0m %d\n"
                           "\033[35mFrom:\033[0m %s%s\033[0m (%s)\n"
                           "\033[35mTime:\033[0m %s\n"
                           "\033[35mContent:\033[0m\n%s\n\n",
                           m->id,
                           m->conversation_id,
                           sender_color,
                           m->sender_name,
                           side,
                           timebuf,
                           m->content);
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

void messages_send_for_client(int client_fd,
                              struct Message *msgs, int count,
                              int current_user_id)
{
    char out[4096];
    int off = 0;

    off = snprintf(out, sizeof(out),
                   "\033[32mOK\033[0m\nMESSAGES %d\n\n", count);
    write_all(client_fd, out, (size_t)off);

    for (int i = 0; i < count; i++) {
        struct Message *m = &msgs[i];

        char timebuf[64] = {0};
        time_t t = (time_t)m->created_at;
        struct tm tm_info;
        localtime_r(&t, &tm_info);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm_info);

        const char *side = msg_side_label(m->sender_id, current_user_id);
        const char *sender_color = msg_sender_color(m->sender_id, current_user_id);

        off = snprintf(out, sizeof(out),
                       "\033[90m========== Message #%d ==========\033[0m\n"
                       "\033[35mConversation:\033[0m %d\n"
                       "\033[35mFrom:\033[0m %s%s\033[0m (%s)\n"
                       "\033[35mTime:\033[0m \033[34m%s\033[0m\n"
                       "\033[35mContent:\033[0m\n%s\n\n",
                       m->id,
                       m->conversation_id,
                       sender_color,
                       m->sender_name,
                       side,
                       timebuf,
                       m->content);

        write_all(client_fd, out, (size_t)off);
    }

    write_all(client_fd, "END\n", 4);
}
