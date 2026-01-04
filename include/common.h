#pragma once
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stdint.h>

#define PORT 2908
#define IP_LOCAL "127.0.0.1"
#define IP_LAN "To be continued"

#define C_RESET  "\033[0m"
#define C_GREEN  "\033[32m"
#define C_RED    "\033[31m"
#define C_YELLOW "\033[33m"
#define C_GRAY   "\033[90m"
#define C_BLUE   "\033[34m"