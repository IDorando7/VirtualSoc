#include "client.h"
#include "common.h"
#include "utils_client.h"
#include "protocol_client.h"
#include "protocol.h"

int client_connect(const char *host, int port)
{
    struct sockaddr_in server;
    int sd;
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Error at socket creation.\n");
        return -1;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(host);
    server.sin_port = htons(port);

    if (connect(sd, (struct sockaddr *) &server, sizeof(struct sockaddr)) == -1)
    {
        perror("[client]Eroare la connect().\n");
        return -1;
    }
    return sd;
}

void client_loop(int sockfd)
{
    printf("Welcome to VirtualSoc!\n");
    printf("Type 'help' for commands.\n");

    char buffer[MAX_CMD_LEN];
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
            printf("  view_user <user>\n");
            printf("  send <user>\n");
            printf("  messages <user>\n");
            printf("  add <user>\n");
            printf("  friends\n");
            printf("  change_vis <PUBLIC|PRIVATE>\n");
            printf("  change_friend <user> <NORMAL|CLOSE>\n");
            printf("  make_admin <user>\n");
            printf("  delete_user <user>\n");
            printf("  delete_post <post>\n");
            printf("  delete_friend <user>\n");
            printf("  create_group <group> <PUBLIC|PRIVATE>\n");
            printf("  join_group <group>\n");
            printf("  request_join <group>\n");
            printf("  approve_member <group> <user> \n");
            printf("  send_group <group> <text>\n");
            printf("  view_members <group>\n");
            printf("  leave_group <group>\n");
            printf("  list_groups\n");
            printf("  exit\n");
            continue;
        }

        char *cmd = NULL;
        char *arg1 = NULL;
        char *arg2 = NULL;
        Parser(buffer, &cmd, &arg1, &arg2);

        // register
        if (strcmp(cmd, "register") == 0)
        {
            if (arg1 == NULL || arg2 == NULL)
            {
                printf("Usage: register <user> <pass>\n");
                continue;
            }
            cmd_register(sockfd, arg1, arg2);
            continue;
        }

        // login
        if (strcmp(cmd, "login") == 0)
        {
            if (arg1 == NULL || arg2 == NULL)
            {
                printf("Usage: login <user> <pass>\n");
                continue;
            }
            cmd_login(sockfd, arg1, arg2);
            continue;
        }

        // logout
        if (strncmp(cmd, "logout", 6) == 0)
        {
            printf("%s, %s\n", arg1, arg2);
            if (arg1 != NULL || arg2 != NULL)
            {
                printf("Usage: logout\n");
                continue;
            }

            printf("adadad\n");
            fflush(stdout);
            cmd_logout(sockfd);
            continue;
        }

        // post
        if (strcmp(cmd, "post") == 0)
        {
            if (arg1 != NULL || arg2 != NULL)
            {
                printf("Usage: post\n");
                continue;
            }

            char vis_str[32];
            char content[MAX_CONTENT_LEN];

            printf("Visibility (public/friends/close): ");
            fflush(stdout);
            int n = read_and_normalize(vis_str, sizeof(vis_str));
            if (n < 0)
            {
                printf("Error at read");
                break;
            }

            if (strcmp(vis_str, "public") != 0 && strcmp(vis_str, "friends") != 0
                && strcmp(vis_str, "close") != 0)
            {
                printf("Invalid visibility argument");
                break;
            }

            printf("Content: ");
            fflush(stdout);
            int m = read_and_normalize(content, sizeof(content));
            printf("%s", content);
            fflush(stdout);
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
            if (arg1 != NULL || arg2 != NULL)
            {
                printf("Usage: view_public\n");
                continue;
            }
            cmd_view_public(sockfd);
            continue;
        }

        // view_feed
        if (strcmp(cmd, "view_feed") == 0)
        {
            if (arg1 != NULL || arg2 != NULL)
            {
                printf("Usage: view_feed\n");
                continue;
            }
            cmd_view_feed(sockfd);
            continue;
        }

        // send message
        if (strcmp(cmd, "send") == 0)
        {
            if (arg1 == NULL)
            {
                printf("Usage: send <username>\n");
                continue;
            }

            char msg[MAX_CONTENT_LEN];
            printf("Message for %s: ", arg1);
            fflush(stdout);
            int n = read_and_normalize(msg, sizeof(msg));
            if (n < 0)
            {
                printf("Error at read");
                break;
            }

            cmd_send_message(sockfd, arg1, msg);
            continue;
        }

        // message view
        if (strcmp(cmd, "messages") == 0)
        {
            if (arg1 == NULL)
            {
                printf("Usage: messages <username>\n");
                continue;
            }
            cmd_list_messages(sockfd, arg1);
            continue;
        }

        if (strcmp(cmd, "add") == 0)
        {
            if (arg1 == NULL)
            {
                printf("Usage: add <username>\n");
                continue;
            }
            cmd_add_friend(sockfd, arg1);
            continue;
        }

        if (strcmp(cmd, "friends") == 0)
        {
            if (arg1 != NULL)
            {
                printf("Usage: friends\n");
                continue;
            }
            cmd_list_friends(sockfd);
            continue;
        }

        if (strcmp(cmd, "change_vis") == 0)
        {
            if (arg1 == NULL || (strcmp(arg1, "PUBLIC") != 0 && strcmp(arg1, "PRIVATE") != 0))
            {
                printf("Usage: change_vis <PUBLIC|PRIVATE>\n");
                continue;
            }
            cmd_change_vis(sockfd, arg1);
            continue;
        }

        if (strcmp(cmd, "make_admin") == 0)
        {
            if (arg1 == NULL)
            {
                printf("Usage: make_admin <user>\n");
                continue;
            }
            cmd_make_admin(sockfd, arg1);
            continue;
        }

        if (strcmp(cmd, "delete_user") == 0)
        {
            if (arg1 == NULL)
            {
                printf("Usage: delete_user <user>\n");
                continue;
            }
            cmd_delete_user(sockfd, arg1);
            continue;
        }

        if (strcmp(cmd, "delete_post") == 0)
        {
            if (arg1 == NULL)
            {
                printf("Usage: delete_user <post_id>\n");
                continue;
            }
            cmd_delete_post(sockfd, arg1);
            continue;
        }

        if (strcmp(cmd, "view_user") == 0)
        {
            if (arg1 == NULL)
            {
                printf("Usage: view_user <user>\n");
                continue;
            }
            cmd_view_user(sockfd, arg1);
            continue;
        }

        if (strcmp(cmd, "delete_friend") == 0)
        {
            if (arg1 == NULL)
            {
                printf("Usage: delete_friend <user>\n");
                continue;
            }
            cmd_delete_friend(sockfd, arg1);
            continue;
        }

        if (strcmp(cmd, "change_friend") == 0)
        {
            if (arg1 == NULL || arg2 == NULL || (strcmp(arg2, "NORMAL") != 0 && strcmp(arg2, "CLOSE") != 0))
            {
                printf("Usage: change_friend <user> <NORMAL|CLOSE>\n");
                continue;
            }
            cmd_change_friend(sockfd, arg1, arg2);
            continue;
        }

        if (strcmp(cmd, "create_group") == 0)
        {
            if (arg1 == NULL || arg2 == NULL || (strcmp(arg2, "PUBLIC") != 0 && strcmp(arg2, "PRIVATE") != 0))
            {
                printf("Usage: create_group <group> <NORMAL|CLOSE>\n");
                continue;
            }
            cmd_create_group(sockfd, arg1, arg2);
            continue;
        }

        if (strcmp(cmd, "join_group") == 0)
        {
            if (arg1 == NULL)
            {
                printf("Usage: join_group <group>\n");
                continue;
            }
            cmd_join_group(sockfd, arg1);
            continue;
        }

        if (strcmp(cmd, "request_join") == 0)
        {
            if (arg1 == NULL)
            {
                printf("Usage: request_join <group>\n");
                continue;
            }
            cmd_request_join(sockfd, arg1);
            continue;
        }

        if (strcmp(cmd, "approve_member") == 0)
        {
            if (arg1 == NULL || arg2 == NULL)
            {
                printf("Usage: approve_member <group> <user>\n");
                continue;
            }
            cmd_approve_member(sockfd, arg1, arg2);
            continue;
        }

        if (strcmp(cmd, "send_group") == 0)
        {
            if (arg1 == NULL || arg2 == NULL)
            {
                printf("Usage: send_group <group> <text>\n");
                continue;
            }
            cmd_send_group(sockfd, arg1, arg2);
            continue;
        }

        if (strcmp(cmd, "view_members") == 0)
        {
            if (arg1 == NULL)
            {
                printf("Usage: view_members <group>\n");
                continue;
            }
            cmd_view_members(sockfd, arg1);
            continue;
        }

        if (strcmp(cmd, "leave_group") == 0)
        {
            if (arg1 == NULL)
            {
                printf("Usage: leave_group <group>\n");
                continue;
            }
            cmd_leave_group(sockfd, arg1);
            continue;
        }

        if (strcmp(cmd, "list_groups") == 0)
        {
            if (arg1 != NULL)
            {
                printf("Usage: list_group\n");
                continue;
            }
            cmd_view_group(sockfd);
            continue;
        }

        printf("Unknown command: %s\n", cmd);
    }

    close(sockfd);
}
