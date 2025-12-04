#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sqlite3.h>
#include <sodium.h>

#include "common.h"
#include "auth.h"
#include "models.h"
#include "storage.h"   // g_db, db_mutex

/*
 * Coduri eroare:
 * AUTH_OK                 0
 * AUTH_ERR_EXISTS         1
 * AUTH_ERR_USER_NOT_FOUND 2
 * AUTH_ERR_WRONG_PASS     3
 * AUTH_ERR_NO_SLOT        4
 * AUTH_ERR_UNKNOWN        5
 */

/*
 * ============================
 * inițializare lazy
 * ============================
 */
static void init_auth_once()
{
    static int initialized = 0;
    if (initialized)
        return;
    initialized = 1;

    pthread_mutex_lock(&db_mutex);

    char *errmsg = NULL;
    int rc;

    // users
    const char *sql_users =
        "CREATE TABLE IF NOT EXISTS users ("
        "  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name          TEXT UNIQUE NOT NULL,"
        "  password_hash TEXT NOT NULL,"
        "  type          INTEGER NOT NULL,"
        "  vis           INTEGER NOT NULL"
        ");";

    rc = sqlite3_exec(g_db, sql_users, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[auth] create users error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }

    // sessions
    const char *sql_sessions =
        "CREATE TABLE IF NOT EXISTS sessions ("
        "  id        INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  client_fd INTEGER UNIQUE NOT NULL,"
        "  user_id   INTEGER NOT NULL"
        ");";

    rc = sqlite3_exec(g_db, sql_sessions, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[auth] create sessions error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }

    // reset sesiuni vechi (client_fd nu e persistent)
    rc = sqlite3_exec(g_db, "DELETE FROM sessions;", NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[auth] clearing sessions error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }

    pthread_mutex_unlock(&db_mutex);
}

/* helper intern */
static int db_find_user_id_by_name(const char *username)
{
    const char *sql = "SELECT id FROM users WHERE name = ? LIMIT 1;";
    sqlite3_stmt *stmt;
    int id = -1;

    pthread_mutex_lock(&db_mutex);

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
        id = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return id;
}

/*
 * ============================
 * REGISTER (cu hashing)
 * ============================
 */
int auth_register(const char *username, const char *password)
{
    init_auth_once();

    if (db_find_user_id_by_name(username) >= 0)
        return AUTH_ERR_EXISTS;

    // generăm hash Argon2id
    char hash[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(
            hash,
            password,
            strlen(password),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE
        ) != 0)
    {
        return AUTH_ERR_UNKNOWN;
    }

    const char *sql =
        "INSERT INTO users(name, password_hash, type, vis) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt *stmt;

    pthread_mutex_lock(&db_mutex);

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return AUTH_ERR_UNKNOWN;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, USER_NORMAL);
    sqlite3_bind_int(stmt, 4, USER_PUBLIC);

    int rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return (rc == SQLITE_DONE) ? AUTH_OK : AUTH_ERR_UNKNOWN;
}

/*
 * ============================
 * LOGIN (verificare HASH)
 * ============================
 */
int auth_login(int client_fd, const char *username, const char *password)
{
    init_auth_once();

    // încărcăm hash-ul userului
    const char *sql =
        "SELECT id, password_hash FROM users WHERE name = ? LIMIT 1;";

    sqlite3_stmt *stmt;
    int user_id = -1;
    char stored_hash[128];

    pthread_mutex_lock(&db_mutex);

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return AUTH_ERR_UNKNOWN;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {
        user_id = sqlite3_column_int(stmt, 0);
        const char *h = (const char*)sqlite3_column_text(stmt, 1);
        strncpy(stored_hash, h, sizeof(stored_hash));
        stored_hash[sizeof(stored_hash)-1] = '\0';
    } else {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return AUTH_ERR_USER_NOT_FOUND;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    // verificare constant-time: crypto_pwhash_str_verify()
    if (crypto_pwhash_str_verify(stored_hash, password, strlen(password)) != 0) {
        return AUTH_ERR_WRONG_PASS;
    }

    // creăm/actualizăm sesiunea
    const char *sql2 =
        "INSERT INTO sessions(client_fd, user_id) "
        "VALUES (?, ?) "
        "ON CONFLICT(client_fd) DO UPDATE SET user_id = excluded.user_id;";

    pthread_mutex_lock(&db_mutex);

    if (sqlite3_prepare_v2(g_db, sql2, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return AUTH_ERR_UNKNOWN;
    }

    sqlite3_bind_int(stmt, 1, client_fd);
    sqlite3_bind_int(stmt, 2, user_id);

    rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return (rc == SQLITE_DONE) ? AUTH_OK : AUTH_ERR_UNKNOWN;
}

/*
 * ============================
 * LOGOUT
 * ============================
 */
int auth_logout(int client_fd)
{
    init_auth_once();

    const char *sql = "DELETE FROM sessions WHERE client_fd = ?;";
    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&db_mutex);

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return AUTH_ERR_UNKNOWN;
    }

    sqlite3_bind_int(stmt, 1, client_fd);
    rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return (rc == SQLITE_DONE) ? AUTH_OK : AUTH_ERR_UNKNOWN;
}

/*
 * ============================
 * auth_get_user_id(client_fd)
 * ============================
 */
int auth_get_user_id(int client_fd)
{
    init_auth_once();

    const char *sql =
        "SELECT user_id FROM sessions WHERE client_fd = ? LIMIT 1;";

    sqlite3_stmt *stmt;
    int user_id = -1;

    pthread_mutex_lock(&db_mutex);

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, client_fd);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
        user_id = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return user_id;
}

/*
 * ============================
 * auth_get_user_id_by_name()
 * ============================
 */
int auth_get_user_id_by_name(const char *username)
{
    init_auth_once();
    return db_find_user_id_by_name(username);
}
