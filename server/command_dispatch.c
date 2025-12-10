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

void command_dispatch(int client)
{
    char buffer[MAX_CMD_LEN];
    char response[MAX_CONTENT_LEN];

    while (1)
    {
        int n = read(client, buffer, sizeof(buffer) - 1);
        if (n <= 0)
        {
            perror("[server] read");
            break;
        }

        buffer[n] = '\0';

        printf("[Thread]Message received...%s\n", buffer);

        char *cmd  = NULL;
        char *arg1 = NULL;
        char *arg2 = NULL;
        Parser(buffer, &cmd, &arg1, &arg2);

        printf("%s, %s, %s\n", cmd, arg1, arg2);

        /* ================= REGISTER ================= */
        if (strcmp(cmd, CMD_REGISTER) == 0)
        {
            printf("register\n");
            int ok = auth_register(arg1, arg2);
            if (ok == 0)
                build_ok(response, "Register successful");
            else if (ok == 1)
                build_error(response, ERR_USER_EXISTS, "User already exists");
            else
                build_error(response, ERR_INTERNAL, "Register failed");

            write(client, response, strlen(response));
            printf("Aici am ajuns in acest state\n");
            continue;
        }

        /* ================= LOGIN ================= */
        if (strcmp(cmd, CMD_LOGIN) == 0)
        {
            int ok = auth_login(client, arg1, arg2);
            if (ok == 0)
                build_ok(response, "Login successful");
            else if (ok == 1)
                build_error(response, ERR_USER_EXISTS, "User already exists");
            else
                build_error(response, ERR_INTERNAL, "Login failed");

            write(client, response, strlen(response));
            continue;
        }

        /* ================= LOGOUT ================= */
        if (strcmp(cmd, CMD_LOGOUT) == 0)
        {
            int ok = auth_logout(client);
            if (ok == 0)
                build_ok(response, "Logout successful");
            else
                build_error(response, ERR_NOT_AUTH, "Not auth");

            write(client, response, strlen(response));
            continue;
        }

        /* ================= POST ================= */
        if (strcmp(cmd, CMD_POST) == 0)
        {
            int author_id = auth_get_user_id(client);
            int vis = 0;
            if (strcmp(arg1, "public") == 0)
                vis = 0;
            else if (strcmp(arg1, "friends") == 0)
                vis = 1;
            else if (strcmp(arg1, "close") == 0)
                vis = 2;

            printf("vis: %d\n", vis);
            int ok = posts_add(author_id, vis, arg2);

            printf("ok: %d\n", ok);

            if (ok >= 0)
                build_ok(response, "Posts successful");
            else
                build_error(response, ERR_INTERNAL, "Posts failed");

            write(client, response, strlen(response));
            continue;
        }

        /* ================= VIEW_PUBLIC_POSTS ================= */
        if (strcmp(cmd, CMD_VIEW_PUBLIC_POSTS) == 0)
        {
            struct Post out_array[MAX_POSTS];
            int count = posts_get_public(out_array, MAX_POSTS);
            if (count < 0)
            {
                build_error(response, ERR_INTERNAL, "Public feed failed");
                write(client, response, strlen(response));
                continue;
            }

            char resp[MAX_FEED];
            format_posts_for_client(resp, sizeof(resp), out_array, count);
            write(client, resp, strlen(resp));
            continue;
        }

        /* ================= VIEW_FEED ================= */
        if (strcmp(cmd, CMD_VIEW_FEED) == 0)
        {
            struct Post out_array[MAX_POSTS];
            int user_id = auth_get_user_id(client);
            if (user_id < 0)
            {
                build_error(response, ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int count = posts_get_feed_for_user(user_id, out_array, MAX_POSTS);
            if (count < 0)
            {
                build_error(response, ERR_INTERNAL, "Public feed failed");
                write(client, response, strlen(response));
                continue;
            }

            char resp[MAX_FEED];
            format_posts_for_client(resp, sizeof(resp), out_array, count);
            write(client, resp, strlen(resp));
            continue;
        }

        /* ================= SEND_MESSAGE ================= */
        if (strcmp(cmd, CMD_SEND_MESSAGE) == 0)
        {
            int sender_id = auth_get_user_id(client);
            if (sender_id < 0)
            {
                build_error(response, ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            char sender_name[64];
            int ok = auth_get_username_by_id(sender_id, sender_name, sizeof(sender_name));
            if (ok < 0)
            {
                build_error(response, ERR_USER_NOT_FOUND, "Sender doesn't exist.");
                write(client, response, strlen(response));
                continue;
            }

            int target_id = auth_get_user_id_by_name(arg1);
            if (target_id < 0)
            {
                build_error(response, ERR_USER_NOT_FOUND, "User doesn't exist.");
                write(client, response, strlen(response));
                continue;
            }

            int conv_id = messages_find_or_create_dm(sender_id, target_id);
            if (conv_id < 0)
            {
                build_error(response, ERR_INTERNAL, "Internal error (msg).");
                write(client, response, strlen(response));
                continue;
            }

            int msg_id = messages_add(conv_id, sender_id, arg2);
            if (msg_id < 0)
            {
                build_error(response, ERR_INTERNAL, "Internal error at (msg).");
                write(client, response, strlen(response));
                continue;
            }

            build_ok(response, "Message sent");
            write(client, response, strlen(response));
            continue;
        }

        /* ================= LIST_MESSAGES ================= */
        if (strcmp(cmd, CMD_LIST_MESSAGES) == 0)
        {
            int me_id = auth_get_user_id(client);
            if (me_id < 0)
            {
                build_error(response, ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int target_id = auth_get_user_id_by_name(arg1);
            if (target_id < 0)
            {
                build_error(response, ERR_USER_NOT_FOUND, "User doesn't exist.");
                write(client, response, strlen(response));
                continue;
            }

            struct Message msgs[MAX_MESSAGE_LIST];
            int count = messages_get_history_dm(me_id, target_id, msgs, MAX_MESSAGE_LIST);

            if (count < 0)
            {
                build_error(response, ERR_INTERNAL, "Internal error (msg).");
                write(client, response, strlen(response));
                continue;
            }

            char resp[8196];
            format_messages_for_client(resp, sizeof(resp), msgs, count, me_id);

            write(client, resp, strlen(resp));
            continue;
        }

        /* ================= ADD_FRIEND ================= */
        if (strcmp(cmd, CMD_ADD_FRIEND) == 0)
        {
            printf("%s", arg1);
            fflush(stdout);

            int target_id = auth_get_user_id_by_name(arg1);
            if (target_id < 0)
            {
                build_error(response, ERR_USER_NOT_FOUND, "User doesn't exist.");
                write(client, response, strlen(response));
                continue;
            }

            int me_id = auth_get_user_id(client);
            if (me_id < 0)
            {
                build_error(response, ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int ok = friends_add(me_id, target_id, FRIEND_NORMAL);
            if (ok < 0)
            {
                build_error(response, ERR_INTERNAL, "Internal error. Can't add friend.");
                write(client, response, strlen(response));
                continue;
            }

            build_ok(response, "Add friend");
            write(client, response, strlen(response));
            continue;
        }

        /* ================= LIST_FRIENDS ================= */
        if (strcmp(cmd, CMD_LIST_FRIENDS) == 0)
        {
            struct Friendship out_friends[MAX_FRIENDS_LIST];
            int user_id = auth_get_user_id(client);
            if (user_id < 0)
            {
                build_error(response, ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int count = friends_list_for_user(user_id, out_friends, MAX_FRIENDS_LIST);
            if (count < 0)
            {
                build_error(response, ERR_INTERNAL, "Friends list failed");
                write(client, response, strlen(response));
                continue;
            }

            format_friends_for_client(response, sizeof(response), out_friends, count, user_id);
            write(client, response, strlen(response));
            continue;
        }

        /* ================= SET_PROFILE_VIS ================= */
        if (strcmp(cmd, CMD_SET_PROFILE_VIS) == 0)
        {
            enum user_vis vis;
            if (strcmp(arg1, "PUBLIC") == 0)
                vis = USER_PUBLIC;
            else if (strcmp(arg1, "PRIVATE") == 0)
                vis = USER_PRIVATE;
            /* altfel vis e nedefinit, dar clientul ar trebui să trimită doar valori valide */

            int user_id = auth_get_user_id(client);
            if (user_id < 0) {
                build_error(response, ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int ok = auth_set_profile_visibility(user_id, vis);
            if (ok < 0)
            {
                build_error(response, ERR_INTERNAL, "Could not update profile visibility.");
                write(client, response, strlen(response));
                continue;
            }

            build_ok(response, "Profile visibility updated");
            write(client, response, strlen(response));
            continue;
        }

        /* ================= MAKE_ADMIN ================= */
        if (strcmp(cmd, CMD_MAKE_ADMIN) == 0)
        {
            int requester_id = auth_get_user_id(client);
            if (requester_id < 0) {
                build_error(response, ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int ok = auth_make_admin(requester_id, arg1);
            if (ok == AUTH_ERR_NOT_ADMIN) {
                build_error(response, ERR_NO_PERMISSION, "You are not admin.");
            } else if (ok == AUTH_ERR_USER_NOT_FOUND) {
                build_error(response, ERR_USER_NOT_FOUND, "User not found.");
            } else if (ok != AUTH_OK) {
                build_error(response, ERR_INTERNAL, "Could not promote user.");
            } else {
                build_ok(response, "User promoted to admin.");
            }

            write(client, response, strlen(response));
            continue;
        }

        /* ================= DELETE_USER ================= */
        if (strcmp(cmd, CMD_DELETE_USER) == 0)
        {
            int requester_id = auth_get_user_id(client);
            if (requester_id < 0) {
                build_error(response, ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int ok = auth_delete_user(requester_id, arg1);
            if (ok == AUTH_ERR_NOT_ADMIN) {
                build_error(response, ERR_NO_PERMISSION, "You are not admin.");
            } else if (ok == AUTH_ERR_USER_NOT_FOUND) {
                build_error(response, ERR_USER_NOT_FOUND, "User not found.");
            } else if (ok != AUTH_OK) {
                build_error(response, ERR_INTERNAL, "Could not remove user.");
            } else {
                build_ok(response, "User deleted.");
            }

            write(client, response, strlen(response));
            continue;
        }

        /* ================= DELETE_POST ================= */
        if (strcmp(cmd, CMD_DELETE_POST) == 0)
        {
            int requester_id = auth_get_user_id(client);
            if (requester_id < 0) {
                build_error(response, ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int post_id = atoi(arg1);
            int ok = posts_delete(requester_id, post_id);

            if (ok == 1) {
                build_ok(response, "Post deleted.");
            } else if (ok == 0) {
                build_error(response, ERR_POST_NOT_FOUND, "Post not found.");
            } else if (ok == -2) {
                build_error(response, ERR_NO_PERMISSION,
                            "You can only delete your own posts (unless you are admin).");
            } else {
                build_error(response, ERR_INTERNAL, "Could not delete post.");
            }

            write(client, response, strlen(response));
            continue;
        }

        /* ================= VIEW_USER_POSTS ================= */
        if (strcmp(cmd, CMD_VIEW_USER_POSTS) == 0)
        {
            const char *target_username = arg1;

            // viewer_id poate fi -1 dacă nu e logat
            int viewer_id = auth_get_user_id(client);

            int target_id = auth_get_user_id_by_name(target_username);
            if (target_id < 0) {
                build_error(response, ERR_USER_NOT_FOUND, "User not found.");
                write(client, response, strlen(response));
                continue;
            }

            struct Post posts[MAX_POSTS];
            int count = posts_get_for_user(viewer_id, target_id, posts, MAX_POSTS);

            if (count < 0) {
                build_error(response, ERR_INTERNAL, "Could not load user posts.");
                write(client, response, strlen(response));
                continue;
            }

            char resp[MAX_FEED];
            format_posts_for_client(resp, sizeof(resp), posts, count);
            write(client, resp, strlen(resp));
            continue;
        }

        /* ================= DELETE_FRIEND ================= */
        if (strcmp(cmd, CMD_DELETE_FRIEND) == 0)
        {
            int user_id = auth_get_user_id(client);
            if (user_id < 0) {
                build_error(response, ERR_NOT_AUTH,
                            "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int ok = friends_delete(user_id, arg1);
            if (ok == 1) {
                build_ok(response, "Friendship deleted.");
            } else if (ok == 0) {
                build_error(response, ERR_FRIENDSHIP_NOT_FOUND, "You are not friends.");
            } else if (ok == -2) {
                build_error(response, ERR_USER_NOT_FOUND, "User not found.");
            } else {
                build_error(response, ERR_INTERNAL,
                            "Failed to delete friendship.");
            }

            write(client, response, strlen(response));
            continue;
        }

        /* ================= SET_FRIEND_STATUS ================= */
        if (strcmp(cmd, CMD_SET_FRIEND_STATUS) == 0)
        {
            int me_id = auth_get_user_id(client);
            if (me_id < 0) {
                build_error(response, ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int friend_id = auth_get_user_id_by_name(arg1);
            if (friend_id <= 0) {
                build_error(response, ERR_USER_NOT_FOUND, "User not found.");
                write(client, response, strlen(response));
                continue;
            }

            enum friend_type new_type;
            if (strcmp(arg2, "NORMAL") == 0)
                new_type = FRIEND_NORMAL;
            else if (strcmp(arg2, "CLOSE") == 0)
                new_type = FRIEND_CLOSE;

            int rc = friends_change_status(me_id, friend_id, new_type);
            if (rc < 0) {
                build_error(response, ERR_INTERNAL, "Could not change friend status.");
            } else if (rc == 0) {
                build_error(response, ERR_FRIENDSHIP_NOT_FOUND,
                            "You are not friends in this direction yet. Use add <user> first.");
            } else {
                build_ok(response, "Friend status updated.");
            }

            write(client, response, strlen(response));
            continue;
        }
    }
}
