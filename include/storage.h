#pragma once
#ifndef STORAGE_H
#define STORAGE_H

#include <sqlite3.h>
#include "common.h"

/*
 * Global SQLite database handle.
 * Definit în storage.c:
 *
 *   sqlite3 *g_db = NULL;
 *
 * Este folosit de posts.c, auth.c, friends.c, messages.c etc.
 */
extern sqlite3 *g_db;

/*
 * Mutex global pentru acces thread-safe la baza de date.
 * Definiția trebuie să fie în storage.c:
 *
 *   pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;
 */
extern pthread_mutex_t db_mutex;

/*
 * Initializează conexiunea cu SQLite + creează tabelele necesare.
 * Parametru:
 *   path → calea către fișierul bazei de date (ex: "data/virtualsoc.db")
 *
 * Returnează:
 *   0  → succes
 *  -1 → eroare la deschidere / creare tabele
 */
int storage_init(const char *path);

/*
 * Închide conexiunea cu SQLite.
 * După apelul acesta, g_db devine NULL.
 */
void storage_close(void);

/*
 * (Opțional)
 * Dacă vrei persistenta manuală pentru users, posts etc.,
 * poți adăuga funcții dedicate aici.
 *
 * Ex:
 *   int storage_load_users();
 *   int storage_save_user(const User *u);
 *   int storage_load_posts();
 *   int storage_save_post(const Post *p);
 *
 * Pentru moment nu sunt necesare – SQLite e deja persistent.
 */

#endif // STORAGE_H
