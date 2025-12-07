#include "common.h"
#include "protocol_client.h"
#include "protocol.h"

void cmd_register(int sockfd, char *arg1, char *arg2)
{
    char buffer[MAX_CMD_LEN];
    char response[MAX_CMD_LEN];

    sprintf(buffer, "%s %s %s\n", CMD_REGISTER, arg1, arg2);

    write(sockfd, buffer, sizeof(buffer));

    int n = read(sockfd, response, sizeof(response));
    if (n < 0) return;
    response[n] = '\0';

    printf("Server: %s", response);
}

void cmd_login(int sockfd, char *arg1, char *arg2)
{
    char buffer[MAX_CMD_LEN];
    char response[MAX_CMD_LEN];

    sprintf(buffer, "%s %s %s\n", CMD_LOGIN, arg1, arg2);

    write(sockfd, buffer, sizeof(buffer));

    int n = read(sockfd, response, sizeof(response));
    if (n < 0) return;
    response[n] = '\0';

    printf("Server: %s", response);
}

void cmd_logout(int sockfd)
{
    char response[MAX_CMD_LEN];

    write(sockfd, CMD_LOGOUT, sizeof(CMD_LOGOUT));

    int n = read(sockfd, response, sizeof(response));
    if (n < 0) return;
    response[n] = '\0';

    printf("Server: %s", response);
}

void cmd_post(int sockfd, char vis_str[], char content[])
{
    char buffer[MAX_CONTENT_LEN];
    char response[MAX_CMD_LEN];

    sprintf(buffer, "%s %s %s\n", CMD_POST, vis_str, content);

    write(sockfd, buffer, sizeof(buffer));

    int n = read(sockfd, response, sizeof(response));
    if (n < 0) return;
    response[n] = '\0';

    printf("Server: %s", response);
}

void cmd_view_public(int sockfd)
{
    char response[MAX_CMD_LEN];

    write(sockfd, CMD_VIEW_PUBLIC_POSTS, sizeof(CMD_VIEW_PUBLIC_POSTS));

    int n = read(sockfd, response, sizeof(response));
    if (n < 0) return;
    response[n] = '\0';

    printf("%s", response);
}

void cmd_view_feed(int sockfd)
{
    char response[MAX_CMD_LEN];

    write(sockfd, CMD_VIEW_FEED, sizeof(CMD_VIEW_FEED));

    int n = read(sockfd, response, sizeof(response));
    if (n < 0) return;
    response[n] = '\0';

    printf("%s", response);
}

void cmd_send_message(int sockfd, char* arg1, char msg[])
{
    char buffer[MAX_CONTENT_LEN];
    char response[MAX_CONTENT_LEN];

    sprintf(buffer, "%s %s %s\n", CMD_SEND_MESSAGE, arg1, msg);

    write(sockfd, buffer, sizeof(buffer));

    int n = read(sockfd, response, sizeof(response));
    if (n < 0) return;
    response[n] = '\0';

    printf("%s", response);
}

void cmd_list_messages(int sockfd, char * arg1)
{
    char buffer[MAX_CMD_LEN];
    char response[MAX_CMD_LEN];

    sprintf(buffer, "%s %s\n", CMD_LIST_MESSAGES, arg1);

    write(sockfd, buffer, sizeof(buffer));

    int n = read(sockfd, response, sizeof(response));
    if (n < 0) return;
    response[n] = '\0';

    printf("%s", response);
}

void cmd_add_friend(int sockfd, char* arg1)
{
    char buffer[MAX_CMD_LEN];
    char response[MAX_CMD_LEN];

    sprintf(buffer, "%s %s\n", CMD_ADD_FRIEND, arg1);

    write(sockfd, buffer, sizeof(buffer));

    int n = read(sockfd, response, sizeof(response));
    if (n < 0) return;
    response[n] = '\0';

    printf("Server: %s", response);
}

void cmd_list_friends(int sockfd)
{
    char response[MAX_CMD_LEN];

    write(sockfd, CMD_LIST_FRIENDS, sizeof(CMD_LIST_FRIENDS));

    int n = read(sockfd, response, sizeof(response));
    if (n < 0) return;
    response[n] = '\0';

    printf("%s", response);
}

void cmd_change_vis(int sockfd, const char* arg1)
{
    char response[MAX_CMD_LEN];
    char buffer[MAX_CMD_LEN];

    sprintf(buffer, "%s %s\n", CMD_SET_PROFILE_VIS, arg1);
    int n = read(sockfd, response, sizeof(response));
    if (n < 0) return;
    response[n] = '\0';

    printf("Server: %s", response);
}
