#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MAX_CMD_LEN 1024
#define MAX_CONTENT_LEN 1024
#define MAX_MESSAGE_LIST 512
#define MAX_FRIENDS_LIST 8192
#define MAX_FEED        640

#define RESP_OK     "OK"
#define RESP_ERROR  "ERROR"

#define CMD_REGISTER            "REGISTER"
#define CMD_LOGIN               "LOGIN"
#define CMD_LOGOUT              "LOGOUT"

#define CMD_SET_PROFILE_VIS     "SET_PROFILE_VIS"

#define CMD_MAKE_ADMIN      "MAKE_ADMIN"
#define CMD_DELETE_USER     "DELETE_USER"
#define CMD_DELETE_POST     "DELETE_POST"

#define CMD_ADD_FRIEND          "ADD_FRIEND"
#define CMD_LIST_FRIENDS        "LIST_FRIENDS"
#define CMD_DELETE_FRIEND       "DELETE_FRIEND"
#define CMD_SET_FRIEND_STATUS   "SET_FRIEND_STATUS"

#define CMD_POST                "POST"
#define CMD_VIEW_PUBLIC_POSTS   "VIEW_PUBLIC_POSTS"
#define CMD_VIEW_FEED           "VIEW_FEED"
#define CMD_VIEW_USER_POSTS     "VIEW_USER_POSTS"

#define CMD_SEND_MESSAGE        "SEND_MESSAGE"
#define CMD_LIST_MESSAGES       "LIST_MESSAGES"

#define CMD_CREATE_GROUP        "CREATE_GROUP"
#define CMD_JOIN_GROUP          "JOIN_GROUP"
#define CMD_SEND_GROUP_MSG      "SEND_GROUP_MSG"
#define CMD_MEMBERS_GROUP       "MEMBERS_GROUP"
#define CMD_REQUEST_GROUP       "REQUEST_GROUP"
#define CMD_APPROVE_GROUP_MEMBER "APPROVE_GROUP_MEMBER"
#define CMD_LEAVE_GROUP         "LEAVE_GROUP"
#define CMD_LIST_GROUPS         "LIST_GROUPS"
#define CMD_GROUP_MESSAGES      "GROUP_MESSAGES"
#define CMD_SET_GROUP_VIS       "SET_GROUP_VIS"
#define CMD_KICK_GROUP_MEMBER   "KICK_GROUP_MEMBER"
#define CMD_LIST_GROUP_REQUESTS "LIST_GROUP_REQUESTS"
#define CMD_REJECT_GROUP_REQUEST "REJECT_GROUP_REQUEST"


#define ERR_UNKNOWN_CMD             "UNKNOWN_COMMAND"
#define ERR_NOT_AUTH                "NOT_AUTHENTICATED"
#define ERR_BAD_ARGS                "BAD_ARGUMENTS"
#define ERR_USER_EXISTS             "USER_ALREADY_EXISTS"
#define ERR_USER_NOT_FOUND          "USER_NOT_FOUND"
#define ERR_POST_NOT_FOUND          "POST_NOT_FOUND"
#define ERR_WRONG_PASS              "WRONG_PASSWORD"
#define ERR_NO_PERMISSION           "NO_PERMISSION"
#define ERR_INTERNAL                "INTERNAL_ERROR"
#define ERR_FRIENDSHIP_NOT_FOUND    "FRIENDSHIP_NOT_FOUND"
#define ERR_GROUP_NOT_FOUND         "GROUP_NOT_FOUND"
#define ERR_REQ_NOT_FOUND           "REQ_NOT_FOUND"

static void build_ok(char *buf, const char *msg)
{
    sprintf(buf, "%s %s\n", RESP_OK, msg);
}

static void build_error(char *buf, const char *err_code, const char *msg)
{
    sprintf(buf, "%s %s %s\n", RESP_ERROR, err_code, msg);
}

#endif
