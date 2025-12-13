CC = gcc
CFLAGS = -Wall
SERVER_LIBS = -lsqlite3 -lm
CLIENT_LIBS = -lSDL2 -lSDL2_ttf -lSDL2_image -lm -lGL

all: server client

server: server.c common.h
	$(CC) $(CFLAGS) server.c -o server $(SERVER_LIBS)

client: client_sdl.c common.h
	$(CC) $(CFLAGS) client_sdl.c -o client $(CLIENT_LIBS)

clean:
	rm -f server client