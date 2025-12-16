#!/bin/bash

# 1. Compile Server (Standard gcc/clang)
clang server.c -o server -lsqlite3 -lpthread

# 2. Compile Client (Link against SDL2 Frameworks)
# We use sdl2-config to automatically find the include paths and flags
clang client_sdl.c -o client \
    $(sdl2-config --cflags --libs) \
    -lSDL2_image -lSDL2_ttf -lSDL2_mixer

echo "Build complete."