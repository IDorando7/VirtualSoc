#include "server.h"
#include "common.h"

int main(void)
{
    server_start(PORT);
    server_run();

    return 0;
}
