#pragma once
#ifndef AUTH_H
#define AUTH_H

#include "models.h"
#include <sodium.h>

#define AUTH_OK                 0
#define AUTH_ERR_EXISTS         1
#define AUTH_ERR_USER_NOT_FOUND 2
#define AUTH_ERR_WRONG_PASS     3
#define AUTH_ERR_NO_SLOT        4
#define AUTH_ERR_UNKNOWN        5

int auth_register(const char *username, const char* password);
int auth_login(int client_fd, const char *username, const char* password);
int auth_logout(int client_fd);

int auth_get_user_id(int client_fd);
int auth_get_user_id_by_name(const char *username);
int auth_get_username_by_id(int user_id, char *out, size_t out_size);

int auth_set_profile_visibility(int user_id, enum user_vis);

#endif
