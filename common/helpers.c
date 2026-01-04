#include "common.h"
void ui_print_line(const char *line)
{
    if (!line || !*line) return;

    if (strncmp(line, "OK ", 3) == 0 || strcmp(line, "OK\n") == 0) {
        printf(C_GREEN "%s" C_RESET, line);
        return;
    }
    if (strncmp(line, "ERROR ", 6) == 0) {
        printf(C_RED "%s" C_RESET, line);
        return;
    }
    if (strncmp(line, "INFO ", 5) == 0) {
        printf(C_YELLOW "%s" C_RESET, line);
        return;
    }

    if (strncmp(line, "NOTIF ", 6) == 0) {
        time_t now = time(NULL);
        struct tm tm_info;
        localtime_r(&now, &tm_info);

        char tbuf[64];
        strftime(tbuf, sizeof(tbuf), "[%Y-%m-%d %H:%M:%S] ", &tm_info);

        printf(C_BLUE "%s" C_RESET, tbuf);
        printf(C_YELLOW "%s" C_RESET, line);
        return;
    }

    printf("%s", line);
}

int write_all(int fd, const void *buf, size_t len)
{
    const char *p = (const char *)buf;
    size_t left = len;

    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        p += (size_t)n;
        left -= (size_t)n;
    }
    return 0;
}

int send_text(int fd, const char *s)
{
    if (!s) s = "";
    return write_all(fd, s, strlen(s));
}

int send_end(int fd)
{
    return send_text(fd, "END\n");
}