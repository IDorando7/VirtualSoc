#ifndef RESPONSE_H
#define RESPONSE_H

#include <stddef.h>

int build_ok(char *buf, size_t cap, const char *msg);
int build_info(char *buf, size_t cap, const char *msg);
int build_error(char *buf, size_t cap, const char *code, const char *msg);
int build_notif(char *buf, size_t cap, const char *type, const char *payload);

#endif
