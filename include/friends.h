#pragma once
#ifndef FRIENDS_H
#define FRIENDS_H

#include "models.h"

int friends_init(void);
int friends_add(int user_id, int other_id, enum friend_type friend_type);
int friends_list_for_user(int user_id, struct Friendship *out_array, int max_size);
void format_friends_for_client(char *buf, size_t buf_size, struct Friendship *friends, int count, int current_user_id);
int friends_delete(int user_id_1, const char *friend_username);
int friends_change_status(int user_id, int friend_id, enum friend_type new_type);


#endif