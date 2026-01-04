
#ifndef VIRTUALSOC_HELPERS_H
#define VIRTUALSOC_HELPERS_H

void ui_print_line(const char *line);
int write_all(int fd, const void *buf, size_t len);
int send_text(int fd, const char *s);
int send_end(int fd);

#endif