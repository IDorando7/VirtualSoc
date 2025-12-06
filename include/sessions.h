#pragma once
#ifndef SESSIONS_H
#define SESSIONS_H

/*
 * Modul pentru gestionarea sesiunilor:
 *  - legătura client_fd <-> user_id este stocată în tabela `sessions`
 *
 * Tabela sessions (SQLite):
 *   CREATE TABLE IF NOT EXISTS sessions (
 *       id        INTEGER PRIMARY KEY AUTOINCREMENT,
 *       client_fd INTEGER UNIQUE NOT NULL,
 *       user_id   INTEGER NOT NULL
 *   );
 *
 * Toate funcțiile folosesc g_db și db_mutex din storage.h.
 */

/* Creează tabela sessions (dacă nu există) și șterge sesiunile vechi. */
int sessions_init(void);

/* Setează/actualizează sesiunea: client_fd -> user_id (upsert). */
int sessions_set(int client_fd, int user_id);

/* Șterge sesiunea pentru un client_fd (logout). */
int sessions_clear(int client_fd);

/* Obține user_id pentru un client_fd, sau -1 dacă nu există sesiune. */
int sessions_get_user_id(int client_fd);

/* Obține client_fd pentru un user_id, sau -1 dacă userul nu este online. */
int sessions_find_fd_by_user_id(int user_id);

#endif /* SESSIONS_H */
