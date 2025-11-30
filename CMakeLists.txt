cmake_minimum_required(VERSION 3.28.3)
project(VirtualSoc C)

set(CMAKE_C_STANDARD 11)

include_directories(include)

add_executable(server
        server/main_server.c
        server/server_core.c
        server/command_dispatch.c
        server/auth.c
        server/posts.c
        server/friends.c
        server/messages.c
        server/storage.c
        server/utils_server.c
        common/buffer.c
        common/helpers.c
)


add_executable(client
        client/main_client.c
        client/client_core.c
        client/ui_cli.c
        client/protocol_client.c
        client/utils_client.c
        common/buffer.c
        common/helpers.c
)

