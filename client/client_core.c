#include "client.h"
#include "common.h"
#include "models.h"
#include "protocol.h"
#include "utils_client.h"
#include "protocol_client.h"

int client_connect(const char* host, int port)
{
    struct sockaddr_in server;
    int sd;
    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror ("Error at socket creation.\n");
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(host);
    server.sin_port = htons (port);

    if (connect(sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
        perror ("[client]Eroare la connect().\n");
        return errno;
    }

    return sd;
}

void client_loop(int sockfd)
{
    printf("Welcome to VirtualSoc!\n");
    printf("Type 'help' for commands.\n");

    char buffer[256];
    while (1)
    {
        printf("VirtualSoc> ");
        fflush(stdout);

        int n = read_and_normalize(buffer, sizeof(buffer));
        if (n < 0)
        {
            printf("Error at read");
            break;
        }
        if (buffer[0] == '\0')
            continue;

        // exit command
        if (strcmp(buffer, "exit") == 0 || strcmp(buffer, "quit") == 0)
        {
            printf("Goodbye!\n\n\nI made sure to steal your data ;)\n");
            break;
        }

        // help
        if (strcmp(buffer, "help") == 0)
        {
            printf("Available commands:\n");
            printf("  register <user>\n");
            printf("  login <user>\n");
            printf("  logout\n");
            printf("  post\n");
            printf("  view_public\n");
            printf("  view_feed\n");
            printf("  send <user>\n");
            printf("  messages <user>\n");
            printf("  exit\n");
            continue;
        }

        char *cmd  = NULL;
        char *arg1 = NULL;
        char *arg2 = NULL;
        Parser(buffer, &cmd, &arg1, &arg2);

        // register
        if (strcmp(cmd, "register") == 0)
        {
            if (arg1[0] == '\0')
            {
                printf("Usage: register <user>\n");
                continue;
            }
            cmd_register(sockfd, arg1);
            continue;
        }

        // login
        if (strcmp(cmd, "login") == 0)
        {
            if (arg1[0] == '\0') {
                printf("Usage: login <user>\n");
                continue;
            }
            cmd_login(sockfd, arg1);
            continue;
        }

        // logout
        if (strcmp(cmd, "logout") == 0)
        {
            cmd_logout(sockfd);
            continue;
        }

        // post
        if (strcmp(cmd, "post") == 0)
        {
            char vis_str[32];
            char content[4096];

            printf("Visibility (public/friends/close): ");
            int n = read_and_normalize(vis_str, sizeof(vis_str));
            if (n < 0)
            {
                printf("Error at read");
                break;
            }

            printf("Content: ");
            int m = read_and_normalize(content, sizeof(content));
            if (m < 0)
            {
                printf("Error at read");
                break;
            }

            cmd_post(sockfd, vis_str, content);
            continue;
        }

        // view_public
        if (strcmp(cmd, "view_public") == 0)
        {
            cmd_view_public(sockfd);
            continue;
        }

        // view_feed
        if (strcmp(cmd, "view_feed") == 0)
        {
            cmd_view_feed(sockfd);
            continue;
        }

        // send message
        if (strcmp(cmd, "send") == 0)
        {
            if (arg1[0] == '\0')
            {
                printf("Usage: send <username>\n");
                continue;
            }

            char msg[4096];
            printf("Message for %s: ", arg1);
            int n = read_and_normalize(msg, sizeof(msg));
            if (n < 0)
            {
                printf("Error at read");
                break;
            }

            cmd_send_message(sockfd, arg1, msg);
            continue;
        }

        if (strcmp(cmd, "messages") == 0)
        {
            if (arg1[0] == '\0')
            {
                printf("Usage: messages <username>\n");
                continue;
            }
            cmd_list_messages(sockfd, arg1);
            continue;
        }

        printf("Unknown command: %s\n", cmd);
    }

    close(sockfd);
}