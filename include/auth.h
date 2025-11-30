#pragma once

int auth_register(const char *username);
int auth_login(int client_fd);
void auth_logout(int client_fd);
