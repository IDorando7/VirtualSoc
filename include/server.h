#pragma once

struct thData
{
    int id_thread;
    int client;
};

int sd; // socket fd

int server_start(int port);
void server_run();