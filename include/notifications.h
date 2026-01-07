#ifndef VIRTUALSOC_NOTIFICATIONS_H
#define VIRTUALSOC_NOTIFICATIONS_H
#pragma once
#include "models.h"

int notifications_add(int user_id, const char *type, const char *payload);

int notifications_list(int user_id, struct Notification *out, int max_size);

int notifications_delete_all(int user_id);

void notifications_send_for_client(int client_fd, struct Notification *ns, int count);

#endif