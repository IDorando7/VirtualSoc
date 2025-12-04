#include "server.h"
#include "common.h"
#include "storage.h"
#include <sodium.h>

int main(void)
{
    if (sodium_init() < 0)
    {
        fprintf(stderr, "libsodium init failed!\n");
        return 1;
    }

    if (storage_init("data/virtualsoc.db") < 0)
        return 1;

    int sockfd = server_start(PORT);
    if (sockfd < 0)
    {
        printf("[server] Failed to start server");
        storage_close();
        return 1;
    }
    server_run(sockfd);
    storage_close();
    return 0;
}
