#include "command_dispatch.h"

#include "auth.h"
#include "common.h"
#include "friends.h"
#include "messages.h"
#include "posts.h"
#include "utils_client.h"
#include "protocol.h"
#include "groups.h"
#include "server.h"
#include "notify_server.h"
#include "sessions.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "response.h"
#include "helpers.h"

void command_dispatch(int client)
{
    char buffer[MAX_CMD_LEN];
    char response[MAX_CONTENT_LEN];

    while (1)
    {
        int n = read(client, buffer, sizeof(buffer) - 1);
        if (n < 0)
        {
            perror("[server] read");
            break;
        }

        if (n == 0)
        {
            printf("[server] client disconnected\n");
            break;
        }

        buffer[n] = '\0';

        printf("[Thread]Message received...%s\n", buffer);

        char *cmd  = NULL;
        char *arg1 = NULL;
        char *arg2 = NULL;
        Parser(buffer, &cmd, &arg1, &arg2);

        printf("%s, %s, %s\n", cmd, arg1, arg2);

        if (strcmp(cmd, CMD_REGISTER) == 0)
        {
            int ok = auth_register(arg1, arg2);
            if (ok == 0)
                build_ok(response, sizeof(response), "Register successful");
            else if (ok == 1)
                build_error(response, sizeof(response), ERR_USER_EXISTS, "User already exists");
            else
                build_error(response, sizeof(response), ERR_INTERNAL, "Register failed");

            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_LOGIN) == 0)
        {
            int ok = auth_login(client, arg1, arg2);
            if (ok == 0)
                build_ok(response, sizeof(response), "Login successful");
            else if (ok == 1)
                build_error(response, sizeof(response), ERR_USER_EXISTS, "User already exists");
            else
                build_error(response, sizeof(response), ERR_INTERNAL, "Login failed");

            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_LOGOUT) == 0)
        {
            int ok = auth_logout(client);
            if (ok == 0)
                build_ok(response, sizeof(response), "Logout successful");
            else
                build_error(response, sizeof(response), ERR_NOT_AUTH, "Not auth");

            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_POST) == 0)
        {
            int author_id = auth_get_user_id(client);
            if (author_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "Not auth");
                write(client, response, strlen(response));
                continue;
            }

            int vis = 0;
            if (arg1 && strcmp(arg1, "public") == 0) vis = 0;
            else if (arg1 && strcmp(arg1, "friends") == 0) vis = 1;
            else if (arg1 && strcmp(arg1, "close") == 0) vis = 2;

            int ok = posts_add(author_id, vis, arg2);

            if (ok >= 0)
                build_ok(response, sizeof(response), "Posts successful");
            else
                build_error(response, sizeof(response), ERR_INTERNAL, "Posts failed");

            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_VIEW_PUBLIC_POSTS) == 0)
        {
            struct Post out_array[MAX_POSTS];
            int count = posts_get_public(out_array, MAX_POSTS);
            if (count < 0)
            {
                build_error(response, sizeof(response), ERR_INTERNAL, "Public feed failed");
                write(client, response, strlen(response));
                continue;
            }

            posts_send_for_client(client, out_array, count);
            continue;
        }

        if (strcmp(cmd, CMD_VIEW_FEED) == 0)
        {
            struct Post out_array[MAX_POSTS];
            int user_id = auth_get_user_id(client);
            if (user_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int count = posts_get_feed_for_user(user_id, out_array, MAX_POSTS);
            if (count < 0)
            {
                build_error(response, sizeof(response), ERR_INTERNAL, "Public feed failed");
                write(client, response, strlen(response));
                continue;
            }

            posts_send_for_client(client, out_array, count);
            continue;
        }

        if (strcmp(cmd, CMD_SEND_MESSAGE) == 0)
        {
            int sender_id = auth_get_user_id(client);
            if (sender_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            char sender_name[64];
            int ok = auth_get_username_by_id(sender_id, sender_name, sizeof(sender_name));
            if (ok < 0)
            {
                build_error(response, sizeof(response), ERR_USER_NOT_FOUND, "Sender doesn't exist.");
                write(client, response, strlen(response));
                continue;
            }

            int target_id = auth_get_user_id_by_name(arg1);
            if (target_id < 0)
            {
                build_error(response, sizeof(response), ERR_USER_NOT_FOUND, "User doesn't exist.");
                write(client, response, strlen(response));
                continue;
            }

            int conv_id = messages_find_or_create_dm(sender_id, target_id);
            if (conv_id < 0)
            {
                build_error(response, sizeof(response), ERR_INTERNAL, "Internal error (msg).");
                write(client, response, strlen(response));
                continue;
            }

            int msg_id = messages_add(conv_id, sender_id, arg2);
            if (msg_id < 0)
            {
                build_error(response, sizeof(response), ERR_INTERNAL, "Internal error at (msg).");
                write(client, response, strlen(response));
                continue;
            }

            char payload[1800];
            snprintf(payload, sizeof(payload), "%s", sender_name);
            char notif[2048];
            build_notif(notif, sizeof(notif), "DM", payload);
            notify_user(target_id, notif);

            build_ok(response, sizeof(response), "Message sent");
            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_LIST_MESSAGES) == 0)
        {
            int me_id = auth_get_user_id(client);
            if (me_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int target_id = auth_get_user_id_by_name(arg1);
            if (target_id < 0)
            {
                build_error(response, sizeof(response), ERR_USER_NOT_FOUND, "User doesn't exist.");
                write(client, response, strlen(response));
                continue;
            }

            struct Message msgs[MAX_MESSAGE_LIST];
            int count = messages_get_history_dm(me_id, target_id, msgs, MAX_MESSAGE_LIST);
            if (count < 0)
            {
                build_error(response, sizeof(response), ERR_INTERNAL, "Internal error (msg).");
                write(client, response, strlen(response));
                continue;
            }

            messages_send_for_client(client, msgs, count, me_id);
            continue;
        }

        if (strcmp(cmd, CMD_ADD_FRIEND) == 0)
        {
            int target_id = auth_get_user_id_by_name(arg1);
            if (target_id < 0)
            {
                build_error(response, sizeof(response), ERR_USER_NOT_FOUND, "User doesn't exist.");
                write(client, response, strlen(response));
                continue;
            }

            int me_id = auth_get_user_id(client);
            if (me_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int ok = friends_add(me_id, target_id, FRIEND_NORMAL);
            if (ok < 0)
            {
                build_error(response, sizeof(response), ERR_INTERNAL, "Internal error. Can't add friend.");
                write(client, response, strlen(response));
                continue;
            }

            char name[64];
            name[0] = '\0';
            auth_get_username_by_id(me_id, name, sizeof(name));

            char payload[1800];
            snprintf(payload, sizeof(payload), ": %s", name);
            char notif[2048];
            build_notif(notif, sizeof(notif), "FRIEND_REQUEST", payload);
            notify_user(target_id, notif);

            build_ok(response, sizeof(response), "Add friend");
            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_LIST_FRIENDS) == 0)
        {
            struct Friendship out_friends[MAX_FRIENDS_LIST];
            int user_id = auth_get_user_id(client);
            if (user_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int count = friends_list_for_user(user_id, out_friends, MAX_FRIENDS_LIST);
            if (count < 0)
            {
                build_error(response, sizeof(response), ERR_INTERNAL, "Friends list failed");
                write(client, response, strlen(response));
                continue;
            }

            format_friends_for_client(response, sizeof(response), out_friends, count, user_id);
            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_SET_PROFILE_VIS) == 0)
        {
            enum user_vis vis;
            if (arg1 && strcmp(arg1, "PUBLIC") == 0) vis = USER_PUBLIC;
            else if (arg1 && strcmp(arg1, "PRIVATE") == 0) vis = USER_PRIVATE;
            else
            {
                build_error(response, sizeof(response), ERR_BAD_ARGS, "Usage: SET_PROFILE_VIS <PUBLIC|PRIVATE>");
                write(client, response, strlen(response));
                continue;
            }

            int user_id = auth_get_user_id(client);
            if (user_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int ok = auth_set_profile_visibility(user_id, vis);
            if (ok < 0)
            {
                build_error(response, sizeof(response), ERR_INTERNAL, "Could not update profile visibility.");
                write(client, response, strlen(response));
                continue;
            }

            build_ok(response, sizeof(response), "Profile visibility updated");
            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_MAKE_ADMIN) == 0)
        {
            int requester_id = auth_get_user_id(client);
            if (requester_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int ok = auth_make_admin(requester_id, arg1);
            if (ok == AUTH_ERR_NOT_ADMIN)
                build_error(response, sizeof(response), ERR_NO_PERMISSION, "You are not admin.");
            else if (ok == AUTH_ERR_USER_NOT_FOUND)
                build_error(response, sizeof(response), ERR_USER_NOT_FOUND, "User not found.");
            else if (ok != AUTH_OK)
                build_error(response, sizeof(response), ERR_INTERNAL, "Could not promote user.");
            else
                build_ok(response, sizeof(response), "User promoted to admin.");

            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_DELETE_USER) == 0)
        {
            int requester_id = auth_get_user_id(client);
            if (requester_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int ok = auth_delete_user(requester_id, arg1);
            if (ok == AUTH_ERR_NOT_ADMIN)
                build_error(response, sizeof(response), ERR_NO_PERMISSION, "You are not admin.");
            else if (ok == AUTH_ERR_USER_NOT_FOUND)
                build_error(response, sizeof(response), ERR_USER_NOT_FOUND, "User not found.");
            else if (ok != AUTH_OK)
                build_error(response, sizeof(response), ERR_INTERNAL, "Could not remove user.");
            else
                build_ok(response, sizeof(response), "User deleted.");

            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_DELETE_POST) == 0)
        {
            int requester_id = auth_get_user_id(client);
            if (requester_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int post_id = arg1 ? atoi(arg1) : 0;
            int ok = posts_delete(requester_id, post_id);

            if (ok == 1)
                build_ok(response, sizeof(response), "Post deleted.");
            else if (ok == 0)
                build_error(response, sizeof(response), ERR_POST_NOT_FOUND, "Post not found.");
            else if (ok == -2)
                build_error(response, sizeof(response), ERR_NO_PERMISSION, "You can only delete your own posts (unless you are admin).");
            else
                build_error(response, sizeof(response), ERR_INTERNAL, "Could not delete post.");

            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_VIEW_USER_POSTS) == 0)
        {
            const char *target_username = arg1;
            int viewer_id = auth_get_user_id(client);

            int target_id = auth_get_user_id_by_name(target_username);
            if (target_id < 0)
            {
                build_error(response, sizeof(response), ERR_USER_NOT_FOUND, "User not found.");
                write(client, response, strlen(response));
                continue;
            }

            struct Post posts[MAX_POSTS];
            int count = posts_get_for_user(viewer_id, target_id, posts, MAX_POSTS);

            if (count < 0)
            {
                build_error(response, sizeof(response), ERR_INTERNAL, "Could not load user posts.");
                write(client, response, strlen(response));
                continue;
            }

            posts_send_for_client(client, posts, count);
            continue;
        }

        if (strcmp(cmd, CMD_DELETE_FRIEND) == 0)
        {
            int user_id = auth_get_user_id(client);
            if (user_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int ok = friends_delete(user_id, arg1);
            if (ok == 1)
                build_ok(response, sizeof(response), "Friendship deleted.");
            else if (ok == 0)
                build_error(response, sizeof(response), ERR_FRIENDSHIP_NOT_FOUND, "You are not friends.");
            else if (ok == -2)
                build_error(response, sizeof(response), ERR_USER_NOT_FOUND, "User not found.");
            else
                build_error(response, sizeof(response), ERR_INTERNAL, "Failed to delete friendship.");

            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_SET_FRIEND_STATUS) == 0)
        {
            int me_id = auth_get_user_id(client);
            if (me_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int friend_id = auth_get_user_id_by_name(arg1);
            if (friend_id <= 0)
            {
                build_error(response, sizeof(response), ERR_USER_NOT_FOUND, "User not found.");
                write(client, response, strlen(response));
                continue;
            }

            enum friend_type new_type;
            if (arg2 && strcmp(arg2, "NORMAL") == 0) new_type = FRIEND_NORMAL;
            else if (arg2 && strcmp(arg2, "CLOSE") == 0) new_type = FRIEND_CLOSE;
            else
            {
                build_error(response, sizeof(response), ERR_BAD_ARGS, "Usage: SET_FRIEND_STATUS <user> <NORMAL|CLOSE>");
                write(client, response, strlen(response));
                continue;
            }

            int rc = friends_change_status(me_id, friend_id, new_type);
            if (rc < 0)
                build_error(response, sizeof(response), ERR_INTERNAL, "Could not change friend status.");
            else if (rc == 0)
                build_error(response, sizeof(response), ERR_FRIENDSHIP_NOT_FOUND,
                            "You are not friends in this direction yet. Use add <user> first.");
            else
                build_ok(response, sizeof(response), "Friend status updated.");

            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_CREATE_GROUP) == 0)
        {
            if (!arg1 || !arg2)
            {
                build_error(response, sizeof(response), ERR_BAD_ARGS, "Usage: CREATE_GROUP <name> <PUBLIC|PRIVATE>");
                write(client, response, strlen(response));
                continue;
            }

            int owner_id = auth_get_user_id(client);
            if (owner_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int is_public;
            if (strcmp(arg2, "PUBLIC") == 0) is_public = 1;
            else if (strcmp(arg2, "PRIVATE") == 0) is_public = 0;
            else
            {
                build_error(response, sizeof(response), ERR_BAD_ARGS, "Visibility must be PUBLIC or PRIVATE.");
                write(client, response, strlen(response));
                continue;
            }

            int rc = groups_create(owner_id, arg1, is_public);
            if (rc == GROUP_OK)
                build_ok(response, sizeof(response), "Group created.");
            else if (rc == GROUP_ERR_EXISTS)
                build_error(response, sizeof(response), ERR_INTERNAL, "Group already exists.");
            else
                build_error(response, sizeof(response), ERR_INTERNAL, "Could not create group.");

            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_JOIN_GROUP) == 0)
        {
            if (!arg1)
            {
                build_error(response, sizeof(response), ERR_BAD_ARGS, "Usage: JOIN_GROUP <group_name>");
                write(client, response, strlen(response));
                continue;
            }

            int user_id = auth_get_user_id(client);
            if (user_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int rc = groups_join_public(user_id, arg1);
            if (rc == GROUP_OK)
                build_ok(response, sizeof(response), "Joined group.");
            else if (rc == GROUP_ERR_NOT_FOUND)
                build_error(response, sizeof(response), ERR_GROUP_NOT_FOUND, "Group not found.");
            else if (rc == GROUP_ERR_NOT_PUBLIC)
                build_error(response, sizeof(response), ERR_NO_PERMISSION, "Group is private. Use REQUEST_GROUP.");
            else if (rc == GROUP_ERR_ALREADY_MEMBER)
                build_error(response, sizeof(response), ERR_INTERNAL, "You are already a member.");
            else
                build_error(response, sizeof(response), ERR_INTERNAL, "Could not join group.");

            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_REQUEST_GROUP) == 0)
        {
            if (!arg1)
            {
                build_error(response, sizeof(response), ERR_BAD_ARGS, "Usage: REQUEST_GROUP <group_name>");
                write(client, response, strlen(response));
                continue;
            }

            int user_id = auth_get_user_id(client);
            if (user_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int rc = groups_request_join(user_id, arg1);
            if (rc == GROUP_OK)
                build_ok(response, sizeof(response), "Join request sent.");
            else if (rc == GROUP_ERR_NOT_FOUND)
                build_error(response, sizeof(response), ERR_GROUP_NOT_FOUND, "Group not found.");
            else if (rc == GROUP_ERR_ALREADY_MEMBER)
                build_error(response, sizeof(response), ERR_INTERNAL, "You are already a member.");
            else
                build_error(response, sizeof(response), ERR_INTERNAL, "Could not send join request.");

            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_APPROVE_GROUP_MEMBER) == 0)
        {
            if (!arg1 || !arg2)
            {
                build_error(response, sizeof(response), ERR_BAD_ARGS,
                            "Usage: APPROVE_GROUP_MEMBER <group_name> <username>");
                write(client, response, strlen(response));
                continue;
            }

            int admin_id = auth_get_user_id(client);
            if (admin_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            int rc = groups_approve_member(admin_id, arg1, arg2);
            if (rc == GROUP_OK)
                build_ok(response, sizeof(response), "Member approved.");
            else if (rc == GROUP_ERR_NOT_FOUND)
                build_error(response, sizeof(response), ERR_GROUP_NOT_FOUND, "Group or user/request not found.");
            else if (rc == GROUP_ERR_NOT_ADMIN)
                build_error(response, sizeof(response), ERR_NO_PERMISSION, "You are not group admin/owner.");
            else if (rc == GROUP_ERR_NO_REQUEST)
                build_error(response, sizeof(response), ERR_REQ_NOT_FOUND, "No pending join request for this user.");
            else
                build_error(response, sizeof(response), ERR_INTERNAL, "Could not approve member.");

            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_SEND_GROUP_MSG) == 0)
        {
            if (!arg1 || !arg2)
            {
                build_error(response, sizeof(response), ERR_BAD_ARGS,
                            "Usage: SEND_GROUP_MSG <group_name> <message...>");
                write(client, response, strlen(response));
                continue;
            }

            const char *group_name = arg1;
            const char *text = arg2;

            int sender_id = auth_get_user_id(client);
            if (sender_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            char sender_name[64];
            if (auth_get_username_by_id(sender_id, sender_name, sizeof(sender_name)) < 0)
            {
                build_error(response, sizeof(response), ERR_INTERNAL, "Could not resolve sender name.");
                write(client, response, strlen(response));
                continue;
            }

            int rc = groups_send_group_msg(sender_id, group_name, text);

            if (rc == GROUP_OK)
                build_ok(response, sizeof(response), "Group message sent.");
            else if (rc == GROUP_ERR_NOT_FOUND)
                build_error(response, sizeof(response), ERR_GROUP_NOT_FOUND, "Group not found.");
            else if (rc == GROUP_ERR_NO_PERMISSION)
                build_error(response, sizeof(response), ERR_NO_PERMISSION, "You are not a member of this group.");
            else
                build_error(response, sizeof(response), ERR_INTERNAL, "Could not send group message.");

            write(client, response, strlen(response));

            if (rc != GROUP_OK)
                continue;

            int member_ids[2048];
            int member_count = groups_list_member_ids(group_name, member_ids,
                                                     (int)(sizeof(member_ids)/sizeof(member_ids[0])));
            if (member_count < 0)
                continue;

            for (int i = 0; i < member_count; i++)
            {
                int uid = member_ids[i];
                if (uid == sender_id) continue;

                char payload[1800];
                snprintf(payload, sizeof(payload), "%s %s", group_name, sender_name);

                char notif[2048];
                build_notif(notif, sizeof(notif), "GROUP_MSG", payload);
                notify_user(uid, notif);
            }

            continue;
        }

        if (strcmp(cmd, CMD_LEAVE_GROUP) == 0)
        {
            int user_id = auth_get_user_id(client);
            if (user_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            if (!arg1)
            {
                build_error(response, sizeof(response), ERR_BAD_ARGS, "Usage: LEAVE_GROUP <group_name>");
                write(client, response, strlen(response));
                continue;
            }

            int rc = groups_leave(user_id, arg1);
            if (rc == GROUP_OK)
                build_ok(response, sizeof(response), "You have left the group.");
            else if (rc == GROUP_ERR_NOT_FOUND)
                build_error(response, sizeof(response), ERR_GROUP_NOT_FOUND, "Group does not exist.");
            else if (rc == GROUP_ERR_NO_PERMISSION)
                build_error(response, sizeof(response), ERR_NO_PERMISSION, "You are not a member of this group.");
            else
                build_error(response, sizeof(response), ERR_INTERNAL, "Could not leave the group.");

            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_MEMBERS_GROUP) == 0)
        {
            int user_id = auth_get_user_id(client);
            if (user_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            if (!arg1)
            {
                build_error(response, sizeof(response), ERR_BAD_ARGS, "Usage: GROUP_MEMBERS <group_name>");
                write(client, response, strlen(response));
                continue;
            }

            struct GroupMemberInfo members[128];
            int rc = groups_view_members(user_id, arg1, members, 128);

            if (rc == GROUP_ERR_NOT_FOUND)
            {
                build_error(response, sizeof(response), ERR_GROUP_NOT_FOUND, "Group not found.");
                write(client, response, strlen(response));
                continue;
            }
            else if (rc == GROUP_ERR_NO_PERMISSION)
            {
                build_error(response, sizeof(response), ERR_NO_PERMISSION, "You are not a member of this group.");
                write(client, response, strlen(response));
                continue;
            }
            else if (rc < 0)
            {
                build_error(response, sizeof(response), ERR_INTERNAL, "Internal error.");
                write(client, response, strlen(response));
                continue;
            }

            char resp[MAX_CONTENT_LEN];
            int off = 0;
            off += snprintf(resp + off, sizeof(resp) - off, "INFO Members of group %s:\n", arg1);

            for (int i = 0; i < rc && off < (int)sizeof(resp); i++)
            {
                off += snprintf(resp + off, sizeof(resp) - off,
                                " - %s%s\n",
                                members[i].username,
                                members[i].is_admin ? " [admin]" : "");
            }

            write(client, resp, strlen(resp));
            continue;
        }

        if (strcmp(cmd, CMD_LIST_GROUPS) == 0)
        {
            int user_id = auth_get_user_id(client);
            if (user_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            if (arg1 != NULL || arg2 != NULL)
            {
                build_error(response, sizeof(response), ERR_BAD_ARGS, "Usage: LIST_GROUPS (no arguments)");
                write(client, response, strlen(response));
                continue;
            }

            struct GroupInfo groups[128];
            int count = groups_list_for_user(user_id, groups, 128);
            if (count < 0)
            {
                build_error(response, sizeof(response), ERR_INTERNAL, "Could not list groups.");
                write(client, response, strlen(response));
                continue;
            }

            char resp[MAX_CONTENT_LEN];
            int off = 0;

            if (count == 0)
            {
                off += snprintf(resp + off, sizeof(resp) - off,
                                "INFO You are not a member of any group.\n");
            }
            else
            {
                off += snprintf(resp + off, sizeof(resp) - off,
                                "INFO Your groups:\n");
                for (int i = 0; i < count && off < (int)sizeof(resp); i++)
                {
                    off += snprintf(resp + off, sizeof(resp) - off,
                                    " - %s%s%s\n",
                                    groups[i].name,
                                    groups[i].is_public ? " [public" : " [private",
                                    groups[i].is_admin ? ", admin]" : "]");
                }
            }

            write(client, resp, strlen(resp));
            continue;
        }

        if (strcmp(cmd, CMD_GROUP_MESSAGES) == 0)
        {
            int user_id = auth_get_user_id(client);
            if (user_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            if (arg1 == NULL)
            {
                build_error(response, sizeof(response), ERR_BAD_ARGS,
                            "Usage: GROUP_MESSAGES <group_name>");
                write(client, response, strlen(response));
                continue;
            }

            struct Message msgs[MAX_MESSAGE_LIST];
            int count = groups_get_group_history(user_id, arg1, msgs, MAX_MESSAGE_LIST);

            if (count < 0)
            {
                if (count == -2)
                    build_error(response, sizeof(response), ERR_GROUP_NOT_FOUND, "Group not found.");
                else if (count == -3)
                    build_error(response, sizeof(response), ERR_NO_PERMISSION, "You are not a member of this group.");
                else
                    build_error(response, sizeof(response), ERR_INTERNAL, "Could not load group messages.");

                write(client, response, strlen(response));
                continue;
            }

            group_messages_send_for_client(client, arg1, msgs, count, user_id);
            continue;
        }

        if (strcmp(cmd, CMD_SET_GROUP_VIS) == 0)
        {
            int user_id = auth_get_user_id(client);
            if (user_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            if (arg1 == NULL || arg2 == NULL)
            {
                build_error(response, sizeof(response), ERR_BAD_ARGS,
                            "Usage: SET_GROUP_VIS <group_name> <PUBLIC|PRIVATE>");
                write(client, response, strlen(response));
                continue;
            }

            int is_public;
            if (strcmp(arg2, "PUBLIC") == 0) is_public = 1;
            else if (strcmp(arg2, "PRIVATE") == 0) is_public = 0;
            else
            {
                build_error(response, sizeof(response), ERR_BAD_ARGS, "Visibility must be PUBLIC or PRIVATE.");
                write(client, response, strlen(response));
                continue;
            }

            int rc = groups_set_visibility(user_id, arg1, is_public);
            if (rc == GROUP_OK)
                build_ok(response, sizeof(response),
                         is_public ? "Group visibility set to PUBLIC." : "Group visibility set to PRIVATE.");
            else if (rc == GROUP_ERR_NOT_FOUND)
                build_error(response, sizeof(response), ERR_GROUP_NOT_FOUND, "Group not found.");
            else if (rc == GROUP_ERR_NOT_ADMIN)
                build_error(response, sizeof(response), ERR_NO_PERMISSION, "Only group owner/admin can change visibility.");
            else
                build_error(response, sizeof(response), ERR_INTERNAL, "Could not change group visibility.");

            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_KICK_GROUP_MEMBER) == 0)
        {
            int user_id = auth_get_user_id(client);
            if (user_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            if (!arg1 || !arg2)
            {
                build_error(response, sizeof(response), ERR_BAD_ARGS,
                            "Usage: KICK_GROUP_MEMBER <group_name> <username>");
                write(client, response, strlen(response));
                continue;
            }

            int rc = groups_kick_member(user_id, arg1, arg2);
            if (rc == GROUP_OK)
                build_ok(response, sizeof(response), "User removed from group.");
            else if (rc == GROUP_ERR_NOT_FOUND)
                build_error(response, sizeof(response), ERR_USER_NOT_FOUND, "Group or user not found.");
            else if (rc == GROUP_ERR_NOT_ADMIN)
                build_error(response, sizeof(response), ERR_NO_PERMISSION, "You must be group admin or owner.");
            else if (rc == GROUP_ERR_NO_PERMISSION)
                build_error(response, sizeof(response), ERR_NO_PERMISSION, "Cannot remove this user (owner or not in group).");
            else
                build_error(response, sizeof(response), ERR_INTERNAL, "Could not remove user from group.");

            write(client, response, strlen(response));
            continue;
        }

        if (strcmp(cmd, CMD_LIST_GROUP_REQUESTS) == 0)
        {
            int admin_id = auth_get_user_id(client);
            if (admin_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            if (!arg1)
            {
                build_error(response, sizeof(response), ERR_BAD_ARGS,
                            "Usage: LIST_GROUP_REQUESTS <group_name>");
                write(client, response, strlen(response));
                continue;
            }

            struct GroupRequestInfo reqs[128];
            int count = groups_list_requests(admin_id, arg1, reqs, 128);

            if (count == GROUP_ERR_NOT_FOUND)
            {
                build_error(response, sizeof(response), ERR_GROUP_NOT_FOUND, "Group not found.");
                write(client, response, strlen(response));
                continue;
            }
            if (count == GROUP_ERR_NOT_ADMIN)
            {
                build_error(response, sizeof(response), ERR_NO_PERMISSION, "You must be group admin.");
                write(client, response, strlen(response));
                continue;
            }
            if (count < 0)
            {
                build_error(response, sizeof(response), ERR_INTERNAL, "Could not fetch requests.");
                write(client, response, strlen(response));
                continue;
            }

            char resp[4096];
            int offset = 0;

            offset += snprintf(resp + offset, sizeof(resp) - offset,
                               "OK JOIN_REQUESTS %d\n", count);

            for (int i = 0; i < count && offset < (int)sizeof(resp) - 1; i++)
            {
                offset += snprintf(resp + offset, sizeof(resp) - offset,
                                   " - %s (uid: %d)\n",
                                   reqs[i].username,
                                   reqs[i].user_id);
            }

            write(client, resp, strlen(resp));
            continue;
        }

        if (strcmp(cmd, CMD_REJECT_GROUP_REQUEST) == 0)
        {
            int admin_id = auth_get_user_id(client);
            if (admin_id < 0)
            {
                build_error(response, sizeof(response), ERR_NOT_AUTH, "You must login first.");
                write(client, response, strlen(response));
                continue;
            }

            if (!arg1 || !arg2)
            {
                build_error(response, sizeof(response), ERR_BAD_ARGS,
                            "Usage: REJECT_GROUP_REQUEST <group> <username>");
                write(client, response, strlen(response));
                continue;
            }

            int rc = groups_reject_request(admin_id, arg1, arg2);
            if (rc == GROUP_ERR_NOT_FOUND)
                build_error(response, sizeof(response), ERR_GROUP_NOT_FOUND, "Group or user not found.");
            else if (rc == GROUP_ERR_NO_PERMISSION)
                build_error(response, sizeof(response), ERR_NO_PERMISSION, "Not allowed.");
            else if (rc == GROUP_ERR_NO_REQUEST)
                build_error(response, sizeof(response), ERR_REQ_NOT_FOUND, "User has no pending request.");
            else if (rc == GROUP_ERR_NOT_ADMIN)
                build_error(response, sizeof(response), ERR_NO_PERMISSION, "Must be admin of the group.");
            else if (rc < 0)
                build_error(response, sizeof(response), ERR_INTERNAL, "Internal error.");
            else
                build_ok(response, sizeof(response), "Join request rejected.");

            write(client, response, strlen(response));
            continue;
        }

        build_error(response, sizeof(response), ERR_BAD_ARGS, "Unknown command");
        write(client, response, strlen(response));
    }
}
