#include "client.h"
#include "common.h"
#include "utils_client.h"
#include "protocol_client.h"
#include "protocol.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "helpers.h"

static void print_prompt(void)
{
    printf("VirtualSoc> ");
    fflush(stdout);
}

static void print_help(void)
{
    printf("Available commands:\n");
    printf("  register <user> <pass>\n");
    printf("  login <user> <pass>\n");
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
    printf("  approve_member <group> <user>\n");
    printf("  send_group <group> <text>\n");
    printf("  view_members <group>\n");
    printf("  leave_group <group>\n");
    printf("  list_groups\n");
    printf("  view_group_messages <group>\n");
    printf("  set_group_vis <group> <PUBLIC|PRIVATE>\n");
    printf("  kick_group <group> <user>\n");
    printf("  requests <group>\n");
    printf("  reject <group> <user>\n");
    printf("  exit\n");
}

int client_connect(const char *host, int port)
{
    struct sockaddr_in server;
    int sd;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error at socket creation");
        return -1;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(host);
    server.sin_port = htons(port);

    if (connect(sd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("[client] Error at connect()");
        close(sd);
        return -1;
    }

    return sd;
}

void client_loop(int sockfd)
{
    printf("Welcome to VirtualSoc!\n");
    printf("Type 'help' for commands.\n");

    char line[MAX_CMD_LEN];

    print_prompt();

    while (1)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sockfd, &readfds);

        int maxfd = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;

        int rc = select(maxfd + 1, &readfds, NULL, NULL, NULL);
        if (rc < 0)
        {
            if (errno == EINTR) continue;
            perror("[client] select");
            break;
        }

        if (FD_ISSET(sockfd, &readfds))
        {
            static char acc[65536];
            static size_t acc_len = 0;

            char chunk[4096];
            int n = (int)read(sockfd, chunk, sizeof(chunk));
            if (n < 0)
            {
                ui_print_line("ERROR ERR_INTERNAL read() failed\n");
                print_prompt();
                continue;
            }
            if (n == 0)
            {
                ui_print_line("INFO Disconnected from server.\n");
                break;
            }

            if (acc_len + (size_t)n >= sizeof(acc))
                acc_len = 0;

            memcpy(acc + acc_len, chunk, (size_t)n);
            acc_len += (size_t)n;

            int got_end = 0;
            size_t start = 0;

            for (size_t i = 0; i < acc_len; i++)
            {
                if (acc[i] == '\n') {
                    size_t line_len = i - start;

                    char linebuf[8192];
                    size_t copy_len = line_len;
                    if (copy_len >= sizeof(linebuf)) copy_len = sizeof(linebuf) - 1;

                    memcpy(linebuf, acc + start, copy_len);
                    linebuf[copy_len] = '\0';

                    if (strcmp(linebuf, "END") == 0)
                    {
                        size_t rest = acc_len - (i + 1);
                        memmove(acc, acc + (i + 1), rest);
                        acc_len = rest;

                        got_end = 1;
                        start = 0;
                        break;
                    }

                    char out[8200];
                    snprintf(out, sizeof(out), "%s\n", linebuf);
                    ui_print_line(out);

                    start = i + 1;
                }
            }

            if (!got_end && start > 0)
            {
                size_t rest = acc_len - start;
                memmove(acc, acc + start, rest);
                acc_len = rest;
            }

            if (got_end)
                print_prompt();

            continue;
        }

        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            int n = read_and_normalize(line, sizeof(line));
            if (n < 0) {
                printf("\n[INFO] stdin closed.\n");
                break;
            }
            if (line[0] == '\0') {
                print_prompt();
                continue;
            }

            if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
                printf("Goodbye!\n\n\nI made sure to steal your data ;)\n");
                break;
            }

            if (strcmp(line, "help") == 0) {
                print_help();
                print_prompt();
                continue;
            }

            char *cmd = NULL, *arg1 = NULL, *arg2 = NULL;
            Parser(line, &cmd, &arg1, &arg2);
            if (!cmd || cmd[0] == '\0') {
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "register") == 0) {
                if (!arg1 || !arg2) { printf("Usage: register <user> <pass>\n"); print_prompt(); continue; }
                cmd_register(sockfd, arg1, arg2);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "login") == 0) {
                if (!arg1 || !arg2) { printf("Usage: login <user> <pass>\n"); print_prompt(); continue; }
                cmd_login(sockfd, arg1, arg2);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "logout") == 0) {
                if (arg1 || arg2) { printf("Usage: logout\n"); print_prompt(); continue; }
                cmd_logout(sockfd);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "post") == 0) {
                if (arg1 || arg2) { printf("Usage: post\n"); print_prompt(); continue; }

                char vis_str[32];
                char content[MAX_CONTENT_LEN];

                printf("Visibility (public/friends/close): ");
                fflush(stdout);
                int vn = read_and_normalize(vis_str, sizeof(vis_str));
                if (vn < 0) { printf("Error at read\n"); print_prompt(); continue; }

                if (strcmp(vis_str, "public") != 0 &&
                    strcmp(vis_str, "friends") != 0 &&
                    strcmp(vis_str, "close") != 0)
                {
                    printf("Invalid visibility argument\n");
                    print_prompt();
                    continue;
                }

                printf("Content: ");
                fflush(stdout);
                int cn = read_and_normalize(content, sizeof(content));
                if (cn < 0) { printf("Error at read\n"); print_prompt(); continue; }

                cmd_post(sockfd, vis_str, content);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "view_public") == 0) {
                if (arg1 || arg2) { printf("Usage: view_public\n"); print_prompt(); continue; }
                cmd_view_public(sockfd);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "view_feed") == 0) {
                if (arg1 || arg2) { printf("Usage: view_feed\n"); print_prompt(); continue; }
                cmd_view_feed(sockfd);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "view_user") == 0) {
                if (!arg1) { printf("Usage: view_user <user>\n"); print_prompt(); continue; }
                cmd_view_user(sockfd, arg1);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "send") == 0) {
                if (!arg1) { printf("Usage: send <username>\n"); print_prompt(); continue; }

                char msg[MAX_CONTENT_LEN];
                printf("Message for %s: ", arg1);
                fflush(stdout);
                int mn = read_and_normalize(msg, sizeof(msg));
                if (mn < 0) { printf("Error at read\n"); break; }

                cmd_send_message(sockfd, arg1, msg);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "messages") == 0) {
                if (!arg1) { printf("Usage: messages <username>\n"); print_prompt(); continue; }
                cmd_list_messages(sockfd, arg1);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "add") == 0) {
                if (!arg1) { printf("Usage: add <username>\n"); print_prompt(); continue; }
                cmd_add_friend(sockfd, arg1);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "friends") == 0) {
                if (arg1 || arg2) { printf("Usage: friends\n"); print_prompt(); continue; }
                cmd_list_friends(sockfd);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "change_vis") == 0) {
                if (!arg1 || (strcmp(arg1, "PUBLIC") != 0 && strcmp(arg1, "PRIVATE") != 0)) {
                    printf("Usage: change_vis <PUBLIC|PRIVATE>\n");
                    print_prompt();
                    continue;
                }
                cmd_change_vis(sockfd, arg1);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "change_friend") == 0) {
                if (!arg1 || !arg2 || (strcmp(arg2, "NORMAL") != 0 && strcmp(arg2, "CLOSE") != 0)) {
                    printf("Usage: change_friend <user> <NORMAL|CLOSE>\n");
                    print_prompt();
                    continue;
                }
                cmd_change_friend(sockfd, arg1, arg2);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "make_admin") == 0) {
                if (!arg1) { printf("Usage: make_admin <user>\n"); print_prompt(); continue; }
                cmd_make_admin(sockfd, arg1);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "delete_user") == 0) {
                if (!arg1) { printf("Usage: delete_user <user>\n"); print_prompt(); continue; }
                cmd_delete_user(sockfd, arg1);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "delete_post") == 0) {
                if (!arg1) { printf("Usage: delete_post <post_id>\n"); print_prompt(); continue; }
                cmd_delete_post(sockfd, arg1);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "delete_friend") == 0) {
                if (!arg1) { printf("Usage: delete_friend <user>\n"); print_prompt(); continue; }
                cmd_delete_friend(sockfd, arg1);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "create_group") == 0) {
                if (!arg1 || !arg2 || (strcmp(arg2, "PUBLIC") != 0 && strcmp(arg2, "PRIVATE") != 0)) {
                    printf("Usage: create_group <group> <PUBLIC|PRIVATE>\n");
                    print_prompt();
                    continue;
                }
                cmd_create_group(sockfd, arg1, arg2);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "join_group") == 0) {
                if (!arg1) { printf("Usage: join_group <group>\n"); print_prompt(); continue; }
                cmd_join_group(sockfd, arg1);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "request_join") == 0) {
                if (!arg1) { printf("Usage: request_join <group>\n"); print_prompt(); continue; }
                cmd_request_join(sockfd, arg1);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "approve_member") == 0) {
                if (!arg1 || !arg2) { printf("Usage: approve_member <group> <user>\n"); print_prompt(); continue; }
                cmd_approve_member(sockfd, arg1, arg2);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "send_group") == 0) {
                if (!arg1 || !arg2) { printf("Usage: send_group <group> <text>\n"); print_prompt(); continue; }
                cmd_send_group(sockfd, arg1, arg2);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "view_members") == 0) {
                if (!arg1) { printf("Usage: view_members <group>\n"); print_prompt(); continue; }
                cmd_view_members(sockfd, arg1);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "leave_group") == 0) {
                if (!arg1) { printf("Usage: leave_group <group>\n"); print_prompt(); continue; }
                cmd_leave_group(sockfd, arg1);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "list_groups") == 0) {
                if (arg1 || arg2) { printf("Usage: list_groups\n"); print_prompt(); continue; }
                cmd_view_group(sockfd);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "view_group_messages") == 0) {
                if (!arg1) { printf("Usage: view_group_messages <group>\n"); print_prompt(); continue; }
                cmd_view_group_messages(sockfd, arg1);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "set_group_vis") == 0) {
                if (!arg1 || !arg2 || (strcmp(arg2, "PUBLIC") != 0 && strcmp(arg2, "PRIVATE") != 0)) {
                    printf("Usage: set_group_vis <group> <PUBLIC|PRIVATE>\n");
                    print_prompt();
                    continue;
                }
                cmd_set_group_vis(sockfd, arg1, arg2);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "kick_group") == 0) {
                if (!arg1 || !arg2) { printf("Usage: kick_group <group> <user>\n"); print_prompt(); continue; }
                cmd_kick_member(sockfd, arg1, arg2);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "requests") == 0) {
                if (!arg1) { printf("Usage: requests <group>\n"); print_prompt(); continue; }
                cmd_get_requests(sockfd, arg1);
                print_prompt();
                continue;
            }

            if (strcmp(cmd, "reject") == 0) {
                if (!arg1 || !arg2) { printf("Usage: reject <group> <user>\n"); print_prompt(); continue; }
                cmd_reject_request(sockfd, arg1, arg2);
                print_prompt();
                continue;
            }

            printf("Unknown command: %s\n", cmd);
            print_prompt();
        }
    }

    close(sockfd);
}
