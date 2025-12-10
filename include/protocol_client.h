#pragma once

void cmd_register(int sockfd, char *arg1, char *arg2);
void cmd_login(int sockfd, char *arg1, char *arg2);
void cmd_logout(int sockfd);
void cmd_post(int sockfd, char vis_str[], char content[]);
void cmd_view_public(int sockfd);
void cmd_view_feed(int sockfd);
void cmd_send_message(int sockfd, char *arg1, char msg[]);
void cmd_list_messages(int sockfd, char *arg1);
void cmd_add_friend(int sockfd, char *arg1);
void cmd_list_friends(int sockfd);
void cmd_change_vis(int sockfd, const char* arg1);
void cmd_make_admin(int sockfd, const char* arg1);
void cmd_delete_post(int sockfd, const char* arg1);
void cmd_delete_user(int sockfd, const char* arg1);
void cmd_view_user(int sockfd, const char* arg1);
void cmd_delete_friend(int sockfd, const char* arg1);
void cmd_change_friend(int sockfd, const char* arg1, const char*arg2);
void cmd_create_group(int sockfd, const char* arg1, const char *arg2);
void cmd_join_group(int sockfd, const char* arg1);
void cmd_request_join(int sockfd, const char* arg1);
void cmd_approve_member(int sockfd, const char* arg1, const char* arg2);
void cmd_view_members(int sockfd, const char* arg1);
void cmd_leave_group(int sockfd, const char* arg1);
void cmd_send_group(int sockfd, const char* arg1, const char* arg2);
void cmd_view_group(int sockfd);