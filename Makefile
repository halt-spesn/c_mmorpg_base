CC = clang
CFLAGS = -Wall -O2 -march=x86-64-v2

# --- SERVER CONFIG ---
SERVER_SRC = server.c
SERVER_OUT = server
# Link everything dynamically(later static): sqlite3, math, threads
SERVER_LDFLAGS = -lsqlite3 -lm -lpthread -ferror-limit=0

# --- CLIENT CONFIG ---
CLIENT_SRC = client_sdl.c
CLIENT_OUT = client

# 1. client compilation flags
CLIENT_CFLAGS = $(shell pkg-config --cflags sdl2 SDL2_image SDL2_ttf) -ferror-limit=0

# 2. client linking flags
CLIENT_LDFLAGS = $(shell pkg-config --libs sdl2 SDL2_image SDL2_ttf) -lSDL2_mixer -lm -lGL -ferror-limit=0

all: server client

server: $(SERVER_SRC) common.h
	@echo "Building Server..."
	$(CC) $(CFLAGS) $(SERVER_SRC) -o $(SERVER_OUT) $(SERVER_LDFLAGS)

client: $(CLIENT_SRC) common.h
	@echo "Building Client..."
	$(CC) $(CFLAGS) $(CLIENT_CFLAGS) $(CLIENT_SRC) -o $(CLIENT_OUT) $(CLIENT_LDFLAGS)

clean:
	rm -f server client