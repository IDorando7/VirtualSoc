#include "command_dispatch.h"

#include "auth.h"
#include "common.h"
#include "posts.h"
#include "utils_client.h"
#include "protocol.h"
#include "server.h"

void command_dispatch(void * arg)
{
    struct thData tdL;
    char buffer[MAX_CMD_LEN];
    char response[MAX_CONTENT_LEN];

    tdL= *(struct thData*)arg;
    if (read(tdL.client, buffer, sizeof(buffer)) <= 0)
    {
        printf("[Thread %d]\n",tdL.id_thread);
        perror ("Error at read\n");
    }

    printf ("[Thread %d]Message received...%s\n",tdL.id_thread, buffer);

    char *cmd  = NULL;
    char *arg1 = NULL;
    char *arg2 = NULL;
    Parser(buffer, &cmd, &arg1, &arg2);

    // register
    if (strcmp(cmd, CMD_REGISTER) == 0)
    {
        int ok = auth_register(arg1, arg2);
        if (ok == 0)
            build_ok(response, "Register successful");
        else if (ok == 1)
            build_error(response, ERR_USER_EXISTS, "User already exists");
        else
            build_error(response,ERR_INTERNAL, "Register failed");

        write(tdL.client, response, sizeof(response));
        return;
    }

    // login
    if (strcmp(cmd, CMD_LOGIN) == 0)
    {
        int ok = auth_login(tdL.client, arg1, arg2);
        if (ok == 0)
            build_ok(response, "Login successful");
        else if (ok == 1)
            build_error(response, ERR_USER_EXISTS, "User already exists");
        else
            build_error(response,ERR_INTERNAL, "Login failed");

        write(tdL.client, response, sizeof(response));
        return;
    }

    // logout
    if (strcmp(cmd, CMD_LOGOUT) == 0)
    {
        int ok = auth_logout(tdL.client);
        if (ok == 0)
            build_ok(response, "Logout successful");
        else build_error(response, ERR_NOT_AUTH, "Not auth");

        write(tdL.client, response, sizeof(response));
        return;
    }

    if (strcmp(cmd, CMD_POST) == 0)
    {
        int author_id = auth_get_user_id(tdL.client);
        int vis = 0;
        if (strcmp(arg1, "public") == 0)
            vis = 0;
        else if (strcmp(arg1, "friend") == 0)
            vis = 1;
        else if (strcmp(arg1, "close") == 0)
            vis = 2;
        int ok = posts_add(author_id, vis, arg2);

        if (ok >= 0)
            build_ok(response, "Posts successful");
        else build_error(response, ERR_INTERNAL, "Posts failed");

        write(tdL.client, response, sizeof(response));
        return;
    }

    if (strcmp(cmd, CMD_VIEW_PUBLIC_POSTS) == 0)
    {
        struct Post out_array[MAX_POSTS];
        int count = posts_get_public(out_array, MAX_POSTS);
        if (count < 0)
            build_error(response, ERR_INTERNAL, "Public feed failed");
        else build_ok(response, "Public feeds successful");

        format_posts_for_client(response, sizeof(response), out_array, count);
        write(tdL.client, response, sizeof(response));
        return;
    }

    if (strcmp(cmd, CMD_VIEW_FEED) == 0)
    {
        struct Post out_array[MAX_POSTS];
        int user_id = auth_get_user_id(tdL.client);
        if (user_id < 0)
        {
            build_error(response, ERR_NOT_AUTH, "You must login first.");
            write(tdL.client, response, sizeof(response));
            return;
        }
        int count = posts_get_feed_for_user(user_id, out_array, MAX_POSTS);
        if (count < 0)
            build_error(response, ERR_INTERNAL, "Public feed failed");
        else build_ok(response, "Public feeds successful");

        format_posts_for_client(response, sizeof(response), out_array, count);
        write(tdL.client, response, sizeof(response));
        return;
    }

    if (strcmp(cmd, CMD_SEND_MESSAGE) == 0)
    {

    }
}