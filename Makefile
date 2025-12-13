CC = clang
CFLAGS = -Wall -O3

# --- SERVER CONFIG ---
SERVER_SRC = server.c
SERVER_OUT = server
# Лінкуємо все динамічно: sqlite3, math, threads
SERVER_LDFLAGS = -lsqlite3 -lm -lpthread

# --- CLIENT CONFIG ---
CLIENT_SRC = client_sdl.c
CLIENT_OUT = client

# 1. Отримуємо прапори компіляції (шляхи до заголовків)
CLIENT_CFLAGS = $(shell pkg-config --cflags sdl2 SDL2_image SDL2_ttf)

# 2. Отримуємо прапори лінкування (шляхи до .so бібліотек)
# pkg-config автоматично додасть -lSDL2 -lSDL2_image -lSDL2_ttf
CLIENT_LDFLAGS = $(shell pkg-config --libs sdl2 SDL2_image SDL2_ttf) -lSDL2_mixer -lm -lGL

all: server client

server: $(SERVER_SRC) common.h
	@echo "Building Server..."
	$(CC) $(CFLAGS) $(SERVER_SRC) -o $(SERVER_OUT) $(SERVER_LDFLAGS)

client: $(CLIENT_SRC) common.h
	@echo "Building Client..."
	$(CC) $(CFLAGS) $(CLIENT_CFLAGS) $(CLIENT_SRC) -o $(CLIENT_OUT) $(CLIENT_LDFLAGS)

clean:
	rm -f server client