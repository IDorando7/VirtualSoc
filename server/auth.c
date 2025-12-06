// auth.c
#include <sodium.h>
#include "common.h"
#include "auth.h"
#include "models.h"
#include "storage.h"   // g_db, db_mutex
#include "sessions.h"  // sessions_set, sessions_clear, sessions_get_user_id

/*
 * Tabela users:
 *
 *  CREATE TABLE IF NOT EXISTS users (
 *      id            INTEGER PRIMARY KEY AUTOINCREMENT,
 *      name          TEXT UNIQUE NOT NULL,
 *      password_hash TEXT NOT NULL,
 *      type          INTEGER NOT NULL,  -- USER_NORMAL / USER_ADMIN
 *      vis           INTEGER NOT NULL   -- USER_PUBLIC / USER_PRIVATE
 *  );
 *
 * Sesiunile sunt gestionate exclusiv în sessions.c (tabela sessions).
 *
 * auth.c folosește:
 *  - users (SQLite)
 *  - sessions_* pentru client_fd <-> user_id
 */

/* ========= Inițializare lazy pentru tabela users ========= */

static void init_auth_once(void)
{
    static int initialized = 0;
    if (initialized)
        return;
    initialized = 1;

    pthread_mutex_lock(&db_mutex);

    char *errmsg = NULL;
    int rc;

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
        fprintf(stderr, "[auth] Cannot create users table: %s\n", errmsg);
        sqlite3_free(errmsg);
        // continuăm, dar orice operație ulterioară va pica cu AUTH_ERR_UNKNOWN
    }

    pthread_mutex_unlock(&db_mutex);
}

/* ========= Helper intern: găsește user_id după username ========= */

static int db_find_user_id_by_name(const char *username)
{
    const char *sql = "SELECT id FROM users WHERE name = ? LIMIT 1;";
    sqlite3_stmt *stmt;
    int id = -1;

    pthread_mutex_lock(&db_mutex);

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[auth] prepare find user failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        id = sqlite3_column_int(stmt, 0);
    } else if (rc != SQLITE_DONE) {
        fprintf(stderr, "[auth] find user error: %s\n", sqlite3_errmsg(g_db));
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);
    return id;
}

/* ========= REGISTER (cu hashing parole) ========= */

int auth_register(const char *username, const char *password)
{
    init_auth_once();

    if (!username || !password || username[0] == '\0' || password[0] == '\0')
        return AUTH_ERR_UNKNOWN;

    /* verificăm dacă userul există deja */
    if (db_find_user_id_by_name(username) >= 0) {
        return AUTH_ERR_EXISTS;
    }

    /* generăm hash Argon2id pentru parolă */
    char hash[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(
            hash,
            password,
            strlen(password),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE
        ) != 0)
    {
        fprintf(stderr, "[auth] crypto_pwhash_str failed\n");
        return AUTH_ERR_UNKNOWN;
    }

    const char *sql =
        "INSERT INTO users(name, password_hash, type, vis) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[auth] prepare insert user failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return AUTH_ERR_UNKNOWN;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, USER_NORMAL);   // default type
    sqlite3_bind_int(stmt, 4, USER_PUBLIC);   // default visibility

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[auth] insert user failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return AUTH_ERR_UNKNOWN;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return AUTH_OK;
}

/* ========= LOGIN (verificare hash + set sesiune prin sessions.c) ========= */

int auth_login(int client_fd, const char *username, const char *password)
{
    init_auth_once();

    if (!username || !password)
        return AUTH_ERR_UNKNOWN;

    const char *sql =
        "SELECT id, password_hash FROM users WHERE name = ? LIMIT 1;";

    sqlite3_stmt *stmt;
    int user_id = -1;
    char stored_hash[crypto_pwhash_STRBYTES];

    pthread_mutex_lock(&db_mutex);

    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[auth] prepare login failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return AUTH_ERR_UNKNOWN;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        user_id = sqlite3_column_int(stmt, 0);
        const unsigned char *h = sqlite3_column_text(stmt, 1);
        if (!h) {
            sqlite3_finalize(stmt);
            pthread_mutex_unlock(&db_mutex);
            return AUTH_ERR_UNKNOWN;
        }
        strncpy(stored_hash, (const char *)h, sizeof(stored_hash) - 1);
        stored_hash[sizeof(stored_hash) - 1] = '\0';
    } else if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return AUTH_ERR_USER_NOT_FOUND;
    } else {
        fprintf(stderr, "[auth] login select error: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return AUTH_ERR_UNKNOWN;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    /* verificare parolă cu Argon2id (constant-time) */
    if (crypto_pwhash_str_verify(stored_hash, password, strlen(password)) != 0) {
        return AUTH_ERR_WRONG_PASS;
    }

    /* setăm sesiunea prin sessions.c */
    if (sessions_set(client_fd, user_id) != 0) {
        return AUTH_ERR_UNKNOWN;
    }

    return AUTH_OK;
}

/* ========= LOGOUT (șterge sesiunea prin sessions.c) ========= */

int auth_logout(int client_fd)
{
    init_auth_once();

    if (sessions_clear(client_fd) != 0) {
        // poți alege să întorci OK chiar dacă nu exista sesiunea
        return AUTH_ERR_UNKNOWN;
    }

    return AUTH_OK;
}

/* ========= Obține user_id pentru client_fd (verifică dacă e logat) ========= */

int auth_get_user_id(int client_fd)
{
    init_auth_once();
    /* sessions_get_user_id întoarce user_id sau -1 dacă nu există sesiune */
    return sessions_get_user_id(client_fd);
}

/* ========= Obține user_id direct după username ========= */

int auth_get_user_id_by_name(const char *username)
{
    init_auth_once();
    return db_find_user_id_by_name(username);
}

int auth_get_username_by_id(int user_id, char *out, size_t out_size)
{
    if (!out || out_size == 0)
        return -1;

    const char *sql =
        "SELECT name FROM users WHERE id = ? LIMIT 1;";

    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[auth] prepare get username by id failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, user_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char *name = sqlite3_column_text(stmt, 0);
        if (name) {
            strncpy(out, (const char *)name, out_size - 1);
            out[out_size - 1] = '\0';
        } else {
            out[0] = '\0';
        }
    } else {
        out[0] = '\0';
        // row not found → user not found
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return 0;
}

