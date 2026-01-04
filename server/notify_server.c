#include "notify_server.h"
#include "sessions.h"
#include <unistd.h>
#include <string.h>

void notify_user(int user_id, const char *line)
{
    if (!line)
        return;
    int fd = sessions_find_fd_by_user_id(user_id);
    if (fd < 0)
        return;
    write(fd, line, strlen(line));
}
