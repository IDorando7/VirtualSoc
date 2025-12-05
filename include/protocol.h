#ifndef PROTOCOL_H
#define PROTOCOL_H

// -------------------------------
// Protocol constants
// -------------------------------

#define MAX_CMD_LEN 1024
#define MAX_CONTENT_LEN 8192

// Status responses
#define RESP_OK     "OK"
#define RESP_ERROR  "ERROR"

// -------------------------------
// Commands supported by protocol
// -------------------------------

// Authentication
#define CMD_REGISTER            "REGISTER"      // REGISTER username password type
#define CMD_LOGIN               "LOGIN"         // LOGIN username password
#define CMD_LOGOUT              "LOGOUT"

// Profile settings
#define CMD_SET_PROFILE_VIS     "SET_PROFILE_VIS"  // SET_PROFILE_VIS PUBLIC|PRIVATE

// Friends
#define CMD_ADD_FRIEND          "ADD_FRIEND"    // ADD_FRIEND username type
#define CMD_LIST_FRIENDS        "LIST_FRIENDS"

// Posts
#define CMD_POST                "POST"          // POST VISIBILITY LENGTH\nCONTENT
#define CMD_VIEW_PUBLIC_POSTS   "VIEW_PUBLIC_POSTS"
#define CMD_VIEW_FEED           "VIEW_FEED"

// Messages
#define CMD_SEND_MESSAGE        "SEND_MESSAGE"  // SEND_MESSAGE username LENGTH\nCONTENT
#define CMD_LIST_MESSAGES       "LIST_MESSAGES" // LIST_MESSAGES username

// Group chat
#define CMD_CREATE_GROUP        "CREATE_GROUP"
#define CMD_JOIN_GROUP          "JOIN_GROUP"
#define CMD_SEND_GROUP_MSG      "SEND_GROUP_MSG"

// ------------
// Error Codes
// ------------

#define ERR_UNKNOWN_CMD         "UNKNOWN_COMMAND"
#define ERR_NOT_AUTH            "NOT_AUTHENTICATED"
#define ERR_BAD_ARGS            "BAD_ARGUMENTS"
#define ERR_USER_EXISTS         "USER_ALREADY_EXISTS"
#define ERR_USER_NOT_FOUND      "USER_NOT_FOUND"
#define ERR_WRONG_PASS          "WRONG_PASSWORD"
#define ERR_NO_PERMISSION       "NO_PERMISSION"
#define ERR_INTERNAL            "INTERNAL_ERROR"

// --------
// Helpers
// --------

/**
 * Builds a simple OK response with a message.
 * Example:   build_ok(buf, "Login successful");
 */
static inline void build_ok(char *buf, const char *msg)
{
    sprintf(buf, "%s %s\n", RESP_OK, msg);
}

/**
 * Builds an error response.
 * Example:   build_error(buf, ERR_BAD_ARGS, "Missing fields");
 */
static inline void build_error(char *buf, const char *err_code, const char *msg)
{
    sprintf(buf, "%s %s %s\n", RESP_ERROR, err_code, msg);
}

#endif // PROTOCOL_H
