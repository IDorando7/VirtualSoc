#include "common.h"
#include "auth.h"
#include "models.h"
#include "protocol.h"

struct Session g_sessions[MAX_SESSIONS];

pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sessions_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_sessions_once()
{
    static int initialized = 0;
    if (initialized)
        return;
    initialized = 1;

    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        g_sessions[i].client_fd = -1;
        g_sessions[i].user_id = -1;
    }
}

int find_user_index_by_name(const char *username)
{
    for (int i = 0; i < g_user_count; i++)
    {
        if (strcmp(g_users[i].name, username) == 0)
            return i;
    }
    return -1;
}

int sessions_find_index_by_fd(int client_fd)
{
    for (int i = 0; i < MAX_SESSIONS; i++)
        if (g_sessions[i].client_fd == client_fd)
            return i;
    return -1;
}

int sessions_find_free_slot()
{
    for (int i = 0; i < MAX_SESSIONS; i++)
        if (g_sessions[i].client_fd == -1)
            return i;
    return -1;
}

int auth_register(const char *username)
{
    init_sessions_once();

    pthread_mutex_lock(&users_mutex);

    if (find_user_index_by_name(username) != -1)
    {
        pthread_mutex_unlock(&users_mutex);
        return AUTH_ERR_EXISTS;
    }

    if (g_user_count >= MAX_USERS)
    {
        pthread_mutex_unlock(&users_mutex);
        return AUTH_ERR_NO_SLOT;
    }

    int id = g_user_count;
    g_users[id].id = id;
    strncpy(g_users[id].name, username, sizeof(g_users[id].name) - 1);
    g_users[id].name[sizeof(g_users[id].name) - 1] = '\0';
    g_user_count++;

    pthread_mutex_unlock(&users_mutex);

    //storage_save_users() dacă vrei să salvezi imediat în fișier

    return AUTH_OK;

}

int auth_login(int client_fd, const char *username)
{
    init_sessions_once();
    int user_index = find_user_index_by_name(username);

    pthread_mutex_lock(&users_mutex);

    if (user_index < 0)
    {
        pthread_mutex_unlock(&users_mutex);
        return AUTH_ERR_USER_NOT_FOUND;
    }

    int user_id = g_users[user_index].id;
    pthread_mutex_unlock(&users_mutex);

    pthread_mutex_lock(&sessions_mutex);

    int idx = sessions_find_index_by_fd(client_fd);
    if (idx < 0)
    {
        idx = sessions_find_free_slot();
        if (idx < 0)
        {
            pthread_mutex_unlock(&sessions_mutex);
            return AUTH_ERR_NO_SLOT;
        }
    }

    g_sessions[idx].client_fd = client_fd;
    g_sessions[idx].user_id = user_id;

    pthread_mutex_unlock(&sessions_mutex);

    return AUTH_OK;
}

int auth_logout(int client_fd)
{
    init_sessions_once();

    pthread_mutex_lock(&sessions_mutex);

    int idx = sessions_find_index_by_fd(client_fd);
    if (idx >= 0)
    {
        g_sessions[idx].client_fd = -1;
        g_sessions[idx].user_id   = -1;
    }
    else
        return AUTH_ERR_EXISTS;

    pthread_mutex_unlock(&sessions_mutex);
    return AUTH_OK;
}

int auth_get_user_id(int client_fd)
{
    init_sessions_once();

    int user_id = -1;

    pthread_mutex_lock(&sessions_mutex);

    int idx = sessions_find_index_by_fd(client_fd);
    if (idx >= 0)
        user_id = g_sessions[idx].user_id;

    pthread_mutex_unlock(&sessions_mutex);

    return user_id;
}

int auth_get_user_id_by_name(const char *username)
{
    int idx = -1;

    pthread_mutex_lock(&users_mutex);
    idx = find_user_index_by_name(username);
    int user_id = -1;
    if (idx >= 0)
        user_id = g_users[idx].id;
    pthread_mutex_unlock(&users_mutex);

    return user_id;
}
