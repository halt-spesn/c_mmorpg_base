game coded by gemini.
current issues: does not runs in wine; window decors on mac os are black
xcode client: static SDL2 libs live in mach-o_deps/. Place SDL2 headers in mach-o_deps/include and open mmotest.xcodeproj; all library references are now relative to the repo.
windows server: build with a MinGW64 environment (MSYS2 recommended) using `gcc server.c -o server.exe -lsqlite3 -lws2_32 -lpthread`. Keep the repo files together so avatars/triggers/etc. are found at runtime.
