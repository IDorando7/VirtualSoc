CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude

# ------------------------------
# FILES
# ------------------------------

COMMON_SRC = common/buffer.c common/helpers.c

SERVER_SRC = \
    server/main_server.c \
    server/server_core.c \
    server/command_dispatch.c \
    server/auth.c \
    server/models.c \
    server/posts.c \
    server/friends.c \
    server/messages.c \
    server/storage.c \
    server/utils_server.c \
    server/sessions.c \
    client/utils_client.c\
    $(COMMON_SRC)

CLIENT_SRC = \
    client/main_client.c \
    client/client_core.c \
    client/ui_cli.c \
    client/protocol_client.c \
    client/utils_client.c \
    $(COMMON_SRC)

# ------------------------------
# OUTPUT BINARIES
# ------------------------------

SERVER_BIN = server_app
CLIENT_BIN = client_app

LDFLAGS_SERVER = -lsqlite3 -lsodium -lpthread

# ------------------------------
# BUILD RULES
# ------------------------------

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_SERVER)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $^

# ------------------------------
# CLEAN
# ------------------------------

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN) *.o */*.o common/*.o

.PHONY: all clean
