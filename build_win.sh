#!/bin/bash

# Path to the extracted libraries (adjust versions if you downloaded newer ones)
SDL2_PATH="win_libs/SDL2-2.28.5/x86_64-w64-mingw32"
IMG_PATH="win_libs/SDL2_image-2.8.2/x86_64-w64-mingw32"
TTF_PATH="win_libs/SDL2_ttf-2.22.0/x86_64-w64-mingw32"
MIX_PATH="win_libs/SDL2_mixer-2.8.0/x86_64-w64-mingw32"

# 1. Create folder for release
mkdir -p windows_release
echo "Creating folder windows_release..."

# Compile Command
x86_64-w64-mingw32-gcc client_sdl.c -o windows_release/game_client.exe \
    -I$SDL2_PATH/include/SDL2 \
    -I$SDL2_PATH/include \
    -I$IMG_PATH/include \
    -I$TTF_PATH/include \
    -I$MIX_PATH/include \
    -L$SDL2_PATH/lib \
    -L$IMG_PATH/lib \
    -L$TTF_PATH/lib \
    -L$MIX_PATH/lib \
    -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer \
    -lws2_32 -lcomdlg32 -lopengl32 -static-libgcc -static-libstdc++

x86_64-w64-mingw32-gcc server.c -o windows_release/server.exe -lws2_32 -lsqlite3 -lm -lpthread

# 3. Copy resources
cp map.jpg windows_release/
cp dungeon.png windows_release/  # or any other map
cp player.png windows_release/
cp triggers.txt windows_release/

# 4. Copy font
cp DejaVuSans.ttf windows_release/DejaVuSans.ttf

# 5. Copy DLL libs from win_libs
# SDL2
cp win_libs/SDL2-*/x86_64-w64-mingw32/bin/*.dll windows_release/
# SDL2_image (along with libpng, zlib etc)
cp win_libs/SDL2_image-*/x86_64-w64-mingw32/bin/*.dll windows_release/
# SDL2_ttf (along with libfreetype)
cp win_libs/SDL2_ttf-*/x86_64-w64-mingw32/bin/*.dll windows_release/
# SDL2_mixer (along with libogg, libvorbis etc)
cp win_libs/SDL2_mixer-*/x86_64-w64-mingw32/bin/*.dll windows_release/

# 6. Thread library
if [ -f /usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll ]; then
    cp /usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll windows_release/
else
    echo "libwinpthread-1.dll not found. Game may not launch."
fi

# 6. Thread library
if [ -f /usr/x86_64-w64-mingw32/bin/libsqlite3-0.dll ]; then
    cp /usr/x86_64-w64-mingw32/bin/libsqlite3-0.dll windows_release/
    cp /usr/x86_64-w64-mingw32/bin/libssp-0.dll windows_release/
else
    echo "libsqlite3-0.dll not found. Server may not launch."
fi

echo "Done. Windows build is located in windows_release directory"
