#include "client.h"
#include "common.h"

int main(void)
{
    int sockfd = client_connect(IP_LOCAL, PORT);
    printf("CLIENT: CONNECTED\n");
    client_loop(sockfd);
    return 0;
}