#pragma once
#ifndef FRIENDS_H
#define FRIENDS_H

#include "models.h"

int friends_add(int user_id, int other_id, enum friend_type friend_type);
int friends_list_for_user(int user_id, struct Friendship *out_array, int max_size);
void format_friends_for_client(char *buf, size_t buf_size, struct Friendship *friends, int count, int current_user_id);
int friends_delete(int user_id_1, const char *friend_username);
int friends_change_status(int user_id, int friend_id, enum friend_type new_type);
int friends_are_mutual(int a, int b);
int friends_request_send(int from_id, int to_id);
int friends_request_list(int to_id, struct FriendRequestInfo *out, int max);
int friends_request_accept(int me_id, const char *from_username);
int friends_request_reject(int me_id, const char *from_username);
int friends_request_accept_by_ids(int me_id, int from_id, enum friend_type my_type, enum friend_type other_type);

#endif