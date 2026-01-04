#include "protocol_client.h"
#include "protocol.h"
#include "common.h"
#include "helpers.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifndef CLIENT_RESP_MAX
#ifdef MAX_FEED
#define CLIENT_RESP_MAX MAX_FEED
#else
#define CLIENT_RESP_MAX 16384
#endif
#endif

static int client_send_and_read(int sockfd, const char *req, char *resp, size_t resp_cap)
{
    if (!req || !resp || resp_cap == 0) return -1;

    ssize_t w = write(sockfd, req, strlen(req));
    if (w <= 0) return -1;

    int n = (int)read(sockfd, resp, resp_cap - 1);
    if (n <= 0) return -1;

    resp[n] = '\0';
    return n;
}

static void send_and_print(int sockfd, const char *req)
{
    char resp[CLIENT_RESP_MAX];
    int n = client_send_and_read(sockfd, req, resp, sizeof(resp));
    if (n > 0) ui_print_line(resp);
}

void cmd_register(int sockfd, char *arg1, char *arg2)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s %s\n", CMD_REGISTER, arg1, arg2);
    send_and_print(sockfd, req);
}

void cmd_login(int sockfd, char *arg1, char *arg2)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s %s\n", CMD_LOGIN, arg1, arg2);
    send_and_print(sockfd, req);
}

void cmd_logout(int sockfd)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s\n", CMD_LOGOUT);
    send_and_print(sockfd, req);
}

void cmd_post(int sockfd, char vis_str[], char content[])
{
    char req[MAX_CONTENT_LEN];
    snprintf(req, sizeof(req), "%s %s %s\n", CMD_POST, vis_str, content);
    send_and_print(sockfd, req);
}

void cmd_view_public(int sockfd)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s\n", CMD_VIEW_PUBLIC_POSTS);
    send_and_print(sockfd, req);
}

void cmd_view_feed(int sockfd)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s\n", CMD_VIEW_FEED);
    send_and_print(sockfd, req);
}

void cmd_send_message(int sockfd, char *arg1, char msg[])
{
    char req[MAX_CONTENT_LEN];
    snprintf(req, sizeof(req), "%s %s %s\n", CMD_SEND_MESSAGE, arg1, msg);
    send_and_print(sockfd, req);
}

void cmd_list_messages(int sockfd, char *arg1)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s\n", CMD_LIST_MESSAGES, arg1);
    send_and_print(sockfd, req);
}

void cmd_add_friend(int sockfd, char *arg1)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s\n", CMD_ADD_FRIEND, arg1);
    send_and_print(sockfd, req);
}

void cmd_list_friends(int sockfd)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s\n", CMD_LIST_FRIENDS);
    send_and_print(sockfd, req);
}

void cmd_change_vis(int sockfd, const char *arg1)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s\n", CMD_SET_PROFILE_VIS, arg1);
    send_and_print(sockfd, req);
}

void cmd_make_admin(int sockfd, const char *arg1)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s\n", CMD_MAKE_ADMIN, arg1);
    send_and_print(sockfd, req);
}

void cmd_delete_user(int sockfd, const char *arg1)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s\n", CMD_DELETE_USER, arg1);
    send_and_print(sockfd, req);
}

void cmd_delete_post(int sockfd, const char *arg1)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s\n", CMD_DELETE_POST, arg1);
    send_and_print(sockfd, req);
}

void cmd_view_user(int sockfd, const char *arg1)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s\n", CMD_VIEW_USER_POSTS, arg1);
    send_and_print(sockfd, req);
}

void cmd_delete_friend(int sockfd, const char *arg1)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s\n", CMD_DELETE_FRIEND, arg1);
    send_and_print(sockfd, req);
}

void cmd_change_friend(int sockfd, const char *arg1, const char *arg2)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s %s\n", CMD_SET_FRIEND_STATUS, arg1, arg2);
    send_and_print(sockfd, req);
}

void cmd_create_group(int sockfd, const char *arg1, const char *arg2)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s %s\n", CMD_CREATE_GROUP, arg1, arg2);
    send_and_print(sockfd, req);
}

void cmd_join_group(int sockfd, const char *arg1)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s\n", CMD_JOIN_GROUP, arg1);
    send_and_print(sockfd, req);
}

void cmd_request_join(int sockfd, const char *arg1)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s\n", CMD_REQUEST_GROUP, arg1);
    send_and_print(sockfd, req);
}

void cmd_approve_member(int sockfd, const char *arg1, const char *arg2)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s %s\n", CMD_APPROVE_GROUP_MEMBER, arg1, arg2);
    send_and_print(sockfd, req);
}

void cmd_view_members(int sockfd, const char *arg1)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s\n", CMD_MEMBERS_GROUP, arg1);
    send_and_print(sockfd, req);
}

void cmd_leave_group(int sockfd, const char *arg1)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s\n", CMD_LEAVE_GROUP, arg1);
    send_and_print(sockfd, req);
}

void cmd_send_group(int sockfd, const char *arg1, const char *arg2)
{
    char req[MAX_CONTENT_LEN];
    snprintf(req, sizeof(req), "%s %s %s\n", CMD_SEND_GROUP_MSG, arg1, arg2);
    send_and_print(sockfd, req);
}

void cmd_view_group(int sockfd)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s\n", CMD_LIST_GROUPS);
    send_and_print(sockfd, req);
}

void cmd_view_group_messages(int sockfd, const char *arg1)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s\n", CMD_GROUP_MESSAGES, arg1);
    send_and_print(sockfd, req);
}

void cmd_set_group_vis(int sockfd, const char *arg1, const char *arg2)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s %s\n", CMD_SET_GROUP_VIS, arg1, arg2);
    send_and_print(sockfd, req);
}

void cmd_kick_member(int sockfd, const char *arg1, const char *arg2)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s %s\n", CMD_KICK_GROUP_MEMBER, arg1, arg2);
    send_and_print(sockfd, req);
}

void cmd_get_requests(int sockfd, const char *arg1)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s\n", CMD_LIST_GROUP_REQUESTS, arg1);
    send_and_print(sockfd, req);
}

void cmd_reject_request(int sockfd, const char *arg1, const char *arg2)
{
    char req[MAX_CMD_LEN];
    snprintf(req, sizeof(req), "%s %s %s\n", CMD_REJECT_GROUP_REQUEST, arg1, arg2);
    send_and_print(sockfd, req);
}
