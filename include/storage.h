#pragma once
#ifndef STORAGE_H
#define STORAGE_H

#include <sqlite3.h>
#include "common.h"

extern sqlite3 *g_db;
extern pthread_mutex_t db_mutex;
int storage_init(const char *path);
void storage_close(void);

#endif
