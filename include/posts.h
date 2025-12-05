#pragma once
#ifndef POSTS_H
#define POSTS_H

#include "models.h"

int posts_add(int author_id, int visibility, const char *content);
int posts_get_public(struct Post *out_array, int max_size);
int posts_get_feed_for_user(int user_id, struct Post *out_array, int max_size);
static const char* visibility_to_string(enum post_visibility v);
void format_posts_for_client(char *buf, size_t buf_size, struct Post *posts, int count);

#endif

