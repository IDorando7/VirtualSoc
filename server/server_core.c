#include "server.h"
#include "common.h"
#include "models.h"
#include "command_dispatch.h"

void* client_handler(void *);
void answer(void *);

void* pula_mea()
{
    printf("Pula mea\n");
    return NULL;
}

int server_start(int port)
{
    struct sockaddr_in server;
    int sd;
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server] Error at socket creation.\n");
        return errno;
    }

    const int on = 1;
    setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));

    bzero (&server, sizeof (server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl (INADDR_ANY);
    server.sin_port = htons (PORT);

    if (bind(sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
        perror ("[server]Error at bind.\n");
        return errno;
    }

    if (listen(sd, 10) == -1)
    {
        perror ("[server]Error at listen.\n");
        return errno;
    }

    printf("[server] Listening on port %d...\n", port);
    return sd;
}

void server_run(int sd)
{
    struct sockaddr_in from;
    pthread_t th[MAX_ID_THREADS];
    int i = 0;
    while (1)
    {
        int client;
        struct thData* td = malloc(sizeof(struct thData));
        unsigned int length = sizeof(from);

        printf ("[server]Waiting at port %d...\n",PORT);
        fflush (stdout);

        if ((client = accept(sd, (struct sockaddr *) &from, &length)) < 0)
        {
            perror ("[server]Eroare la accept().\n");
            continue;
        }

        td -> id_thread = i++;
        td -> client = client;

        pthread_create(&th[i], NULL, &client_handler, td);
    }
}



void* client_handler(void * arg)
{
    struct thData* args = (struct thData*)arg;
    printf ("[thread]- %d - Waiting the message...\n", args->id_thread);
    fflush (stdout);
    command_dispatch(args->client);
    pthread_detach(pthread_self());
    close (args->client);
    return NULL;

}
