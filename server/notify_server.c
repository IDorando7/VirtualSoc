#include "notify_server.h"
#include "sessions.h"
#include <unistd.h>
#include <string.h>
#include "common.h"
#include "notifications.h"

static int write_all(int fd, const char *buf, size_t len)
{
    size_t off = 0;
    while (off < len)
    {
        ssize_t n = write(fd, buf + off, len - off);
        if (n > 0)
        {
            off += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        return -1;
    }
    return 0;
}

static void parse_notif_line(const char *line,
                             char *type, size_t type_cap,
                             char *payload, size_t payload_cap)
{
    if (!type || type_cap == 0 || !payload || payload_cap == 0) return;
    type[0] = '\0';
    payload[0] = '\0';

    if (!line)
    {
        snprintf(type, type_cap, "GENERIC");
        snprintf(payload, payload_cap, "");
        return;
    }

    const char *p = line;

    if (strncmp(p, "NOTIF ", 6) == 0) p += 6;

    const char *sp = strchr(p, ' ');
    if (!sp)
    {
        snprintf(type, type_cap, "GENERIC");
        snprintf(payload, payload_cap, "%s", p);
        return;
    }

    size_t tlen = (size_t)(sp - p);
    if (tlen >= type_cap) tlen = type_cap - 1;
    memcpy(type, p, tlen);
    type[tlen] = '\0';

    const char *pl = sp + 1;
    size_t plen = strcspn(pl, "\n");
    if (plen >= payload_cap) plen = payload_cap - 1;
    memcpy(payload, pl, plen);
    payload[plen] = '\0';
}

void notify_user(int user_id, const char *line)
{
    if (user_id <= 0 || !line) return;

    char type[64];
    char payload[1024];
    parse_notif_line(line, type, sizeof(type), payload, sizeof(payload));

    notifications_add(user_id, type, payload);

    int fd = sessions_find_fd_by_user_id(user_id);
    if (fd < 0) return;

    write_all(fd, line, strlen(line));
}