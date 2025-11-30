#include "server.h"
#include "common.h"
#include "models.h"

static void *treat(void *);
void answer(void *);

int server_start(int port)
{
    struct sockaddr_in server;

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

void server_run()
{
    struct sockaddr_in from;
    pthread_t th[MAX_ID_THREADS];
    int i = 0;
    while (1)
    {
        int client;
        struct thData * td;
        int length = sizeof(from);

        printf ("[server]Waiting at port %d...\n",PORT);
        fflush (stdout);

        if ((client = accept(sd, (struct sockaddr *) &from, &length)) < 0)
        {
            perror ("[server]Eroare la accept().\n");
            continue;
        }

        td = (struct thData*)malloc(sizeof(struct thData));
        td -> id_thread = i++;
        td -> client = client;

        pthread_create(&th[i], NULL, &treat, td);
    }
}

static void *treat(void * arg)
{
    struct thData tdL;
    tdL = *(struct thData*)arg;
    printf ("[thread]- %d - Waiting the message...\n", tdL.id_thread);
    fflush (stdout);
    pthread_detach(pthread_self());
    answer(arg);
    close ((intptr_t)arg);
    return NULL;

}

void answer(void *arg)
{

}