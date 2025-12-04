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
    char buffer[MAX_CMD_LEN];
    char response[MAX_CMD_LEN];

    sprintf(buffer, "%s %s %s\n", CMD_LOGIN, vis_str, content);

    write(sockfd, buffer, sizeof(buffer));

    int n = read(sockfd, response, sizeof(response));
    if (n < 0) return;
    response[n] = '\0';

    printf("Server: %s", response);
}

void cmd_view_public(int sockfd)
{

}

void cmd_view_feed(int sockfd)
{

}

void cmd_send_message(int sockfd, char* arg1, char msg[])
{

}

void cmd_list_messages(int sockfd, char * arg1)
{

}
