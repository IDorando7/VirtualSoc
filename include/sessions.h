#pragma once
#ifndef SESSIONS_H
#define SESSIONS_H

int sessions_init(void);
int sessions_set(int client_fd, int user_id);
int sessions_clear(int client_fd);
int sessions_get_user_id(int client_fd);
int sessions_find_fd_by_user_id(int user_id);

#endif
