#pragma once
#ifndef GROUPS_H
#define GROUPS_H

#include "messages.h"
#include "common.h"

/* coduri de eroare pentru grupuri */
#define GROUP_OK                  0
#define GROUP_ERR_EXISTS          (-1)
#define GROUP_ERR_NOT_FOUND       (-2)
#define GROUP_ERR_NOT_PUBLIC      (-3)
#define GROUP_ERR_ALREADY_MEMBER  (-4)
#define GROUP_ERR_NOT_ADMIN       (-5)
#define GROUP_ERR_NO_REQUEST      (-6)
#define GROUP_ERR_NO_PERMISSION   (-7)

/* info simplă despre un membru din grup */
struct GroupMemberInfo {
    int  user_id;
    char username[64];
    int  is_admin;   /* 1 = admin/owner, 0 = membru normal */
};

struct GroupInfo {
    int  group_id;
    char name[64];
    int  is_public;  /* 1 = public, 0 = privat */
    int  is_admin;   /* 1 = user-ul e admin în acel grup */
};

struct GroupRequestInfo {
    int user_id;
    char username[64];
};

int groups_create(int owner_id, const char *name, int is_public);
int groups_join_public(int user_id, const char *group_name);
int groups_request_join(int user_id, const char *group_name);
int groups_approve_member(int admin_id, const char *group_name, const char *username);
int groups_send_group_msg(int sender_id, const char *group_name, const char *text);
int groups_leave(int user_id, const char *group_name);
int groups_view_members(int requester_id, const char *group_name, struct GroupMemberInfo *out_array, int max_size);
int groups_list_for_user(int user_id, struct GroupInfo *out_array, int max_size);
int groups_get_group_history(int requester_id, const char *group_name, struct Message *out_array, int max_size);
void format_group_messages_for_client(char *buf, int buf_size, const char *group_name, struct Message *msgs, int count, int current_user_id);
int groups_set_visibility(int admin_id, const char *group_name, int is_public);
int groups_kick_member(int admin_id, const char *group_name, const char *username);
int groups_list_requests(int admin_id, const char *group_name, struct GroupRequestInfo *out_array, int max_size);
int groups_reject_request(int admin_id, const char *group_name, const char *username);

#endif
