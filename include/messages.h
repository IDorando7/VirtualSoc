#pragma once
#ifndef MESSAGES_H
#define MESSAGES_H

#include <time.h>

struct Message {
    int   id;
    int   conversation_id;
    int   sender_id;
    char  sender_name[64];
    char  content[1024];
    time_t created_at;
};

int messages_find_or_create_dm(int user1_id, int user2_id);
int messages_add(int conversation_id, int sender_id, const char *content);
int messages_get_history_dm(int user1_id, int user2_id,
                            struct Message *out_array, int max_size);

/* Formatter colorat pentru client (current_user_id folosit pt "(you)") */
void format_messages_for_client(char *buf, size_t buf_size,
                                struct Message *msgs, int count,
                                int current_user_id);
const char* msg_side_label(int sender_id, int current_user_id);
const char* msg_sender_color(int sender_id, int current_user_id);

#endif
