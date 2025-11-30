#pragma once

int client_connect(const char* host, int port);
void client_loop(int sockfd);