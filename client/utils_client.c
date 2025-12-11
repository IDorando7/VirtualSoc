#include "common.h"

void trim_newline(char *s)
{
    if (!s)
        return;
    int len = strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r' || s[len-1] == ' '))
        s[--len] = '\0';
}

void Parser(char *input, char **cmd, char **arg1, char **arg2)
{
    *cmd = input;
    char *space = strchr(input, ' ');
    if (!space)
    {
        *arg1 = NULL;
        *arg2 = NULL;
        if (*cmd)
            trim_newline(*cmd);
        return;
    }
    *space = '\0';
    input = space + 1;

    while (*input == ' ')
        input++;

    *arg1 = input;
    space = strchr(input, ' ');
    if (!space)
    {
        *arg2 = NULL;
        if (*cmd)
            trim_newline(*cmd);
        if (*arg1)
            trim_newline(*arg1);
        return;
    }
    *space = '\0';
    input = space + 1;

    while (*input == ' ')
        input++;

    *arg2 = (*input) ? input : NULL;

    if (*cmd) trim_newline(*cmd);
    if (*arg1) trim_newline(*arg1);
    if (*arg2) trim_newline(*arg2);
}

int read_and_normalize(char buffer[], int size)
{
    int n = read(0, buffer, size - 1);

    if (n <= 0)
    {
        buffer[0] = '\0';
        return -1;
    }

    buffer[n] = '\0';

    if (buffer[0] == '\n')
    {
        buffer[0] = '\0';
        return 0;
    }

    if (n > 0 && buffer[n - 1] == '\n')
        buffer[n - 1] = '\0';

    return n;
}

