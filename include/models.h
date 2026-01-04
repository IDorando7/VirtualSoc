#ifndef MODELS_H
#define MODELS_H

#define MAX_POSTS 100
#define MAX_ID_THREADS 100
#define MAX_USERS 100
#define MAX_SESSIONS 100
#define MAX_SESSIONS 100


enum user_type {USER_NORMAL, USER_ADMIN};
enum user_vis {USER_PUBLIC, USER_PRIVATE};
enum post_visibility {VIS_PUBLIC, VIS_FRIENDS, VIS_CLOSE_FRIENDS};
enum friend_type {FRIEND_CLOSE, FRIEND_NORMAL};

struct User
{
    int id;
    char name[64];
    char password_hash[128];
    enum user_type type;
    enum user_vis vis;
};

struct Post
{
    int id;
    int author_id;
    char author_name[64];
    enum post_visibility vis;
    char content[1024];
    int created_at;
};

struct Session
{
    int client_fd;
    int user_id;
};

struct Friendship
{
    int user_id_1;
    int user_id_2;
    enum friend_type type;
};


#endif
