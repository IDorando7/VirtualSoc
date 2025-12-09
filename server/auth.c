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
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    /* >>> Check if there is any admin, if not create initial one <<< */
    const char *sql_count_admin =
        "SELECT COUNT(*) FROM users WHERE type = ?;";

    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(g_db, sql_count_admin, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[auth] prepare count admin failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return;
    }

    sqlite3_bind_int(stmt, 1, USER_ADMIN);

    int admin_count = 0;
    if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        admin_count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (admin_count == 0) {
        /* create initial admin: username=admin, password=admin123 */
        const char *initial_user = "admin";
        const char *initial_pass = "admin";

        char hash[crypto_pwhash_STRBYTES];
        if (crypto_pwhash_str(
                hash,
                initial_pass,
                strlen(initial_pass),
                crypto_pwhash_OPSLIMIT_INTERACTIVE,
                crypto_pwhash_MEMLIMIT_INTERACTIVE
            ) != 0)
        {
            fprintf(stderr, "[auth] crypto_pwhash_str for initial admin failed\n");
        } else {
            const char *sql_ins =
                "INSERT INTO users(name, password_hash, type, vis) "
                "VALUES (?, ?, ?, ?);";

            rc = sqlite3_prepare_v2(g_db, sql_ins, -1, &stmt, NULL);
            if (rc == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, initial_user, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(stmt, 3, USER_ADMIN);
                sqlite3_bind_int(stmt, 4, USER_PRIVATE); // admin poate fi privat

                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                    fprintf(stderr, "[auth] insert initial admin failed: %s\n", sqlite3_errmsg(g_db));
                } else {
                    fprintf(stderr,
                            "[auth] Initial admin created: username='%s' password='%s' (CHANGE IT!)\n",
                            initial_user, initial_pass);
                }
                sqlite3_finalize(stmt);
            } else {
                fprintf(stderr, "[auth] prepare insert initial admin failed: %s\n", sqlite3_errmsg(g_db));
            }
        }
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
    printf("Auidadfa\n");
    fflush(stdout);
    if (sessions_clear(client_fd) != 0) {
        // poți alege să întorci OK chiar dacă nu exista sesiunea
        return AUTH_ERR_UNKNOWN;
    }

    printf("Auidadfa\n");
    fflush(stdout);

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

int auth_set_profile_visibility(int user_id, enum user_vis vis)
{
    if (user_id < 0)
        return -1;

    const char *sql =
        "UPDATE users SET vis = ? WHERE id = ?;";

    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[auth] prepare set vis failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, vis);
    sqlite3_bind_int(stmt, 2, user_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[auth] set vis failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return AUTH_OK;
}

int auth_is_admin(int user_id)
{
    if (user_id <= 0) return -1;

    const char *sql =
        "SELECT type FROM users WHERE id = ? LIMIT 1;";

    sqlite3_stmt *stmt;
    int rc;
    int is_admin = 0;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[auth] prepare is_admin failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    sqlite3_bind_int(stmt, 1, user_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        int type = sqlite3_column_int(stmt, 0);
        is_admin = (type == USER_ADMIN) ? 1 : 0;
    } else if (rc != SQLITE_DONE) {
        fprintf(stderr, "[auth] is_admin select error: %s\n", sqlite3_errmsg(g_db));
        is_admin = -1;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    return is_admin;
}

int auth_make_admin(int requester_id, const char *target_username)
{
    if (!target_username || target_username[0] == '\0')
        return AUTH_ERR_UNKNOWN;

    int is_admin = auth_is_admin(requester_id);
    if (is_admin <= 0) {
        // nu e admin
        return AUTH_ERR_NOT_ADMIN;
    }

    const char *sql =
        "UPDATE users SET type = ? WHERE name = ?;";

    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[auth] prepare make_admin failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return AUTH_ERR_UNKNOWN;
    }

    sqlite3_bind_int(stmt, 1, USER_ADMIN);
    sqlite3_bind_text(stmt, 2, target_username, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[auth] make_admin update failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return AUTH_ERR_UNKNOWN;
    }

    int changes = sqlite3_changes(g_db);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    if (changes == 0)
        return AUTH_ERR_USER_NOT_FOUND;

    return AUTH_OK;
}

int auth_delete_user(int requester_id, const char *target_username)
{
    if (!target_username || target_username[0] == '\0')
        return AUTH_ERR_UNKNOWN;

    int is_admin = auth_is_admin(requester_id);
    if (is_admin <= 0) {
        return AUTH_ERR_NOT_ADMIN;
    }

    const char *sql =
        "DELETE FROM users WHERE name = ?;";

    sqlite3_stmt *stmt;
    int rc;

    pthread_mutex_lock(&db_mutex);

    rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[auth] prepare delete_user failed: %s\n", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&db_mutex);
        return AUTH_ERR_UNKNOWN;
    }

    sqlite3_bind_text(stmt, 1, target_username, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[auth] delete_user failed: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return AUTH_ERR_UNKNOWN;
    }

    int changes = sqlite3_changes(g_db);
    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&db_mutex);

    if (changes == 0)
        return AUTH_ERR_USER_NOT_FOUND;

    return AUTH_OK;
}



