#include "command_dispatch.h"

#include "auth.h"
#include "common.h"
#include "friends.h"
#include "messages.h"
#include "posts.h"
#include "utils_client.h"
#include "protocol.h"
#include "server.h"
#include "sessions.h"

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
        {
            build_error(response, ERR_INTERNAL, "Public feed failed");
            return;
        }

        format_posts_for_client(response, sizeof(response), out_array, count);
        write(tdL.client, response, sizeof(response));
        return;
    }

    if (strcmp(cmd, CMD_SEND_MESSAGE) == 0)
    {
        int sender_id = auth_get_user_id(tdL.client);
        if (sender_id < 0)
        {
            build_error(response, ERR_NOT_AUTH, "You must login first.");
            write(tdL.client, response, sizeof(response));
            return;
        }
        char sender_name[64];
        int ok = auth_get_username_by_id(sender_id, sender_name, sizeof(sender_name));
        if (ok < 0)
        {
            build_error(response, ERR_USER_NOT_FOUND, "Sender doesn't exist.");
            write(tdL.client, response, sizeof(response));
            return;
        }

        int target_id =  auth_get_user_id_by_name(arg1);
        if (target_id < 0)
        {
            build_error(response, ERR_USER_NOT_FOUND, "User doesn't exist.");
            write(tdL.client, response, sizeof(response));
            return;
        }

        int conv_id = messages_find_or_create_dm(sender_id, target_id);
        if (conv_id < 0)
        {
            build_error(response, ERR_INTERNAL, "Internal error (msg).");
            write(tdL.client, response, sizeof(response));
            return;
        }

        int target_fd = sessions_find_fd_by_user_id(target_id);
        if (target_fd >= 0)
        {
            char notify[MAX_CONTENT_LEN];
            snprintf(notify, sizeof(notify), "NEW_MSG_FROM %s\nContent: %s\n",
                sender_name, arg2);
            write(target_fd, notify, sizeof(notify));
        }

        build_ok(response, "Message sent");
        write(tdL.client, response, sizeof(response));
        return;
    }

    if (strcmp(cmd, CMD_LIST_MESSAGES) == 0)
    {
        int me_id = auth_get_user_id(tdL.client);
        if (me_id < 0)
        {
            build_error(response, ERR_NOT_AUTH, "You must login first.");
            write(tdL.client, response, sizeof(response));
            return;
        }

        int target_id = auth_get_user_id_by_name(arg1);
        if (target_id < 0)
        {
            build_error(response, ERR_USER_NOT_FOUND, "User doesn't exist.");
            write(tdL.client, response, sizeof(response));
            return;
        }

        struct Message msgs[MAX_MESSAGE_LIST];
        int count = messages_get_history_dm(me_id, target_id, msgs, MAX_MESSAGE_LIST);

        if (count < 0)
        {
            build_error(response, ERR_INTERNAL, "Internal error (msg).");
            write(tdL.client, response, sizeof(response));
            return;
        }

        char resp[MAX_CONTENT_LEN * MAX_MESSAGE_LIST];
        format_messages_for_client(resp, sizeof(resp), msgs, count, me_id);

        write(tdL.client, resp, sizeof(resp));
        return;
    }

    if (strcmp(cmd, CMD_ADD_FRIEND) == 0)
    {
        int target_id =  auth_get_user_id_by_name(arg1);
        if (target_id < 0)
        {
            build_error(response, ERR_USER_NOT_FOUND, "User doesn't exist.");
            write(tdL.client, response, sizeof(response));
            return;
        }

        int me_id = auth_get_user_id(tdL.client);
        if (me_id < 0)
        {
            build_error(response, ERR_NOT_AUTH, "You must login first.");
            write(tdL.client, response, sizeof(response));
            return;
        }

        int ok = friends_add(me_id, target_id, FRIEND_NORMAL);
        if (ok < 0)
        {
            build_error(response, ERR_INTERNAL, "Internal error. Can't add friend.");
            write(tdL.client, response, sizeof(response));
            return;
        }

        build_ok(response, "Add friend");
        write(tdL.client, response, sizeof(response));
        return;
    }

    if (strcmp(cmd, CMD_LIST_FRIENDS) == 0)
    {
        struct Friendship out_friends[MAX_FRIENDS_LIST];
        int user_id = auth_get_user_id(tdL.client);
        if (user_id < 0)
        {
            build_error(response, ERR_NOT_AUTH, "You must login first.");
            write(tdL.client, response, sizeof(response));
            return;
        }

        int count = friends_list_for_user(user_id, out_friends, MAX_FRIENDS_LIST);
        if (count < 0)
        {
            build_error(response, ERR_INTERNAL, "Friends list failed");
            return;
        }

        format_friends_for_client(response, sizeof(response), out_friends, count, user_id);
        write(tdL.client, response, sizeof(response));
        return;
    }

    if (strcmp(cmd, CMD_SET_PROFILE_VIS) == 0)
    {
        enum user_vis vis;
        if (arg1 == "PUBLIC")
            vis = USER_PUBLIC;
        else if (arg1 == "PRIVATE")
            vis = USER_PRIVATE;

        int user_id = auth_get_user_id(tdL.client);
        if (user_id < 0) {
            build_error(response, ERR_NOT_AUTH, "You must login first.");
            write(tdL.client, response, strlen(response));
            return;
        }

        int ok = auth_set_profile_visibility(user_id, vis);
        if (ok < 0)
        {
            build_error(response, ERR_INTERNAL, "Could not update profile visibility.");
            write(tdL.client, response, strlen(response));
            return;
        }

        build_ok(response, "Profile visibility updated");
        write(tdL.client, response, strlen(response));
        return;
    }

}
