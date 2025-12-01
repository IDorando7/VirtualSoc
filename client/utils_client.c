#include "common.h"
void Parser(char buffer[], char **cmd, char **arg1, char **arg2)
{
    int count = 0;
    char *p = buffer;

    while (*p)
    {
        if (*p == ' ')
        {
            *p = '\0';
            count++;
        }
        p++;
    }

    char *token = buffer;

    if (count >= 0)
        *cmd = token;

    while (*token)
        token++;
    token++;

    if (count >= 1)
        *arg1 = token;

    while (*token)
        token++;
    token++;

    if (count >= 2)
        *arg2 = token;
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

    if (buffer[0] == '\n') {
        buffer[0] = '\0';
        return 0;
    }

    if (n > 0 && buffer[n - 1] == '\n')
        buffer[n - 1] = '\0';

    return n;
}

