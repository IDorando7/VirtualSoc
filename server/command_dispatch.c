#include "command_dispatch.h"

#include "auth.h"
#include "common.h"
#include "utils_client.h"
#include "protocol.h"
#include "server.h"

void command_dispatch(void * arg)
{
    struct thData tdL;
    char buffer[MAX_CMD_LEN];
    char response[MAX_CMD_LEN];

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
        if (!(arg1 != NULL && arg2 != NULL))
        {
            build_error(response, ERR_BAD_ARGS, "Usage: register <user> <pass>");
            write(tdL.client, response, strlen(response));
            return;
        }
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
        if (!(arg1 != NULL && arg2 == NULL))
        {
            build_error(response, ERR_BAD_ARGS, "Usage: login <user>");
            write(tdL.client, response, strlen(response));
            return;
        }
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
        if (!(arg1 == NULL && arg2 == NULL))
        {
            build_error(response, ERR_BAD_ARGS, "Usage: logout");
            write(tdL.client, response, strlen(response));
            return;
        }
        int ok = auth_logout(tdL.client);
        if (ok == 0)
            build_ok(response, "Logout successful");
        else build_error(response, ERR_NOT_AUTH, "Not auth");

        write(tdL.client, response, sizeof(response));
        return;
    }
}