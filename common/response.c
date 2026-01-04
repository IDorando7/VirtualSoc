#include <stdio.h>
#include <stddef.h>

int build_ok(char *buf, size_t cap, const char *msg)
{
    if (!buf || cap == 0) return -1;
    if (!msg) msg = "";
    return snprintf(buf, cap, "OK %s\n", msg);
}

int build_info(char *buf, size_t cap, const char *msg)
{
    if (!buf || cap == 0) return -1;
    if (!msg) msg = "";
    return snprintf(buf, cap, "INFO %s\n", msg);
}

int build_error(char *buf, size_t cap, const char *code, const char *msg)
{
    if (!buf || cap == 0) return -1;
    if (!code) code = "ERR_INTERNAL";
    if (!msg) msg = "";
    return snprintf(buf, cap, "ERROR %s %s\n", code, msg);
}

int build_notif(char *buf, size_t cap, const char *type, const char *payload)
{
    if (!buf || cap == 0) return -1;
    if (!type) type = "GENERIC";
    if (!payload) payload = "";
    return snprintf(buf, cap, "NOTIF %s %s\n", type, payload);
}
