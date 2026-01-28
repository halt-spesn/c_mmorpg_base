CC = clang
CFLAGS = -Wall -O2 -march=x86-64-v2

# --- VULKAN SUPPORT (always enabled) ---
# Vulkan rendering support is always built-in
# Requires Vulkan SDK installed (libvulkan-dev on Linux)
USE_VULKAN ?= 1
ifeq ($(USE_VULKAN),1)
    VULKAN_CFLAGS = -DUSE_VULKAN
    VULKAN_LDFLAGS = -lvulkan
    VULKAN_SRC = renderer_vulkan.c
else
    VULKAN_CFLAGS =
    VULKAN_LDFLAGS =
    VULKAN_SRC =
endif

# --- SERVER CONFIG ---
SERVER_SRC = server.c
SERVER_OUT = server
# Link everything dynamically(later static): sqlite3, math, threads, sdl2
SERVER_CFLAGS = $(shell pkg-config --cflags sdl2 SDL2_ttf)
SERVER_LDFLAGS = -lsqlite3 -lm -lpthread $(shell pkg-config --libs sdl2 SDL2_ttf) -ferror-limit=0

# --- CLIENT CONFIG ---
CLIENT_SRC = client_sdl.c client_network.c client_config.c client_audio.c client_utils.c client_input.c localization.c $(VULKAN_SRC)
CLIENT_OUT = client

# 1. client compilation flags
CLIENT_CFLAGS = $(shell pkg-config --cflags sdl2 SDL2_image SDL2_ttf) -ferror-limit=0 $(VULKAN_CFLAGS)

# 2. client linking flags
CLIENT_LDFLAGS = $(shell pkg-config --libs sdl2 SDL2_image SDL2_ttf) -lSDL2_mixer -lm -lGL -ferror-limit=0 $(VULKAN_LDFLAGS)

all: server client

server: $(SERVER_SRC) common.h
	@echo "Building Server..."
	$(CC) $(CFLAGS) $(SERVER_CFLAGS) $(SERVER_SRC) -o $(SERVER_OUT) $(SERVER_LDFLAGS)

client: $(CLIENT_SRC) common.h client.h
	@echo "Building Client..."
	$(CC) $(CFLAGS) $(CLIENT_CFLAGS) $(CLIENT_SRC) -o $(CLIENT_OUT) $(CLIENT_LDFLAGS)

clean:
	rm -f server client