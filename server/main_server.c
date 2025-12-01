#include "server.h"
#include "common.h"

int main(void)
{
    int sockfd = server_start(PORT);
    server_run(sockfd);

    return 0;
}
