#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sqlite3.h>
#include <stdio.h>
#include "notifications.h"
#include "storage.h"

int notifications_add(int user_id, const char *type, const char *payload)
{
    if (user_id <= 0) return -1;
    if (!type) type = "GENERIC";
    if (!payload) payload = "";

    const char *sql =
        "INSERT INTO notifications(user_id, type, payload, created_at, deleted) "
        "VALUES (?, ?, ?, ?, 0);";

    sqlite3_stmt *stmt = NULL;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[notifs_add] prepare failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, payload, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, (int)time(NULL));

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[notifs_add] insert failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    pthread_mutex_unlock(&db_mutex);
    return 0;
}

int notifications_list(int user_id, struct Notification *out, int max_size)
{
    if (user_id <= 0) return -1;
    if (!out || max_size <= 0) return 0;

    const char *sql =
        "SELECT id, user_id, type, payload, created_at "
        "FROM notifications "
        "WHERE user_id = ? AND deleted = 0 "
        "ORDER BY created_at DESC "
        "LIMIT ?;";

    sqlite3_stmt *stmt = NULL;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[notifs_list] prepare failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, max_size);

    int count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW && count < max_size)
    {
        out[count].id        = sqlite3_column_int(stmt, 0);
        out[count].user_id   = sqlite3_column_int(stmt, 1);
        out[count].created_at= sqlite3_column_int(stmt, 4);

        const char *t = (const char *)sqlite3_column_text(stmt, 2);
        const char *p = (const char *)sqlite3_column_text(stmt, 3);

        strncpy(out[count].type, t ? t : "", sizeof(out[count].type) - 1);
        out[count].type[sizeof(out[count].type) - 1] = '\0';

        strncpy(out[count].payload, p ? p : "", sizeof(out[count].payload) - 1);
        out[count].payload[sizeof(out[count].payload) - 1] = '\0';

        count++;
    }

    if (rc != SQLITE_DONE && rc != SQLITE_ROW)
    {
        fprintf(stderr, "[notifs_list] step error: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return count;
}

int notifications_delete_all(int user_id)
{
    if (user_id <= 0) return -1;

    const char *sql =
        "UPDATE notifications SET deleted = 1 WHERE user_id = ? AND deleted = 0;";

    sqlite3_stmt *stmt = NULL;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "[notifs_delete] prepare failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, user_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        fprintf(stderr, "[notifs_delete] update failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    pthread_mutex_unlock(&db_mutex);
    return 1;
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

void notifications_send_for_client(int client_fd, struct Notification *ns, int count)
{
    char out[2048];
    int off = snprintf(out, sizeof(out),
                       "\033[32mOK\033[0m\nNOTIFS %d\n\n", count);
    write_all(client_fd, out, (size_t)off);

    for (int i = 0; i < count; i++) {
        char timebuf[64] = {0};
        time_t t = (time_t)ns[i].created_at;
        struct tm tm_info;
        localtime_r(&t, &tm_info);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm_info);

        off = snprintf(out, sizeof(out),
            "\033[90m========== Notif #%d ==========\033[0m\n"
            "\033[35mTime:\033[0m \033[34m%s\033[0m\n"
            "\033[35mType:\033[0m \033[33m%s\033[0m\n"
            "\033[35mFrom:\033[0m\n%s\n\n",
            ns[i].id, timebuf, ns[i].type, ns[i].payload);

        write_all(client_fd, out, (size_t)off);
    }

    write_all(client_fd, "END\n", 4);
}