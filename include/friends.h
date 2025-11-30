#pragma once
#ifndef FRIENDS_H
#define FRIENDS_H

#include "models.h"

int friends_add(int user_id, int other_id, enum friend_type friend_type);
int friends_are_friends(int user_id, int other_id); // Checks if 2 users are friends
enum friend_type friends_get_type(int user_id, int other_id);
int friends_list_for_user(int user_id, int *out_array, int max_size);

#endif