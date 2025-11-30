#ifndef MODELS_H
#define MODELS_H

#define MAX_POSTS 10000
#define MAX_ID_THREADS 10000

enum user_type {USER_NORMAL, USER_ADMIN};
enum user_vis {USER_PUBLIC, USER_PRIVATE}; // private, only friends can see posts
enum post_visibility {VIS_PUBLIC, VIS_FRIENDS, VIS_CLOSE_FRIENDS};
enum friend_type {FRIEND_CLOSE, FRIEND_NORMAL, FRIEND_ACQUAINTANCE};

struct User
{
    int id;
    char name[64];
    enum user_type type;
    enum user_vis vis;
};

struct Post
{
    int id;
    int author_id;
    enum post_visibility vis;
    char content[1024];
};



#endif
