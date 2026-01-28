@echo off
REM Windows Build Script for MMO Game (using MinGW)
REM Requires MinGW-w64 installed and in PATH

echo ======================================
echo Building Windows Game Client and Server
echo ======================================

REM Path to the extracted libraries
set SDL2_PATH=win_libs\SDL2-2.28.5\x86_64-w64-mingw32
set IMG_PATH=win_libs\SDL2_image-2.8.2\x86_64-w64-mingw32
set TTF_PATH=win_libs\SDL2_ttf-2.22.0\x86_64-w64-mingw32
set MIX_PATH=win_libs\SDL2_mixer-2.8.0\x86_64-w64-mingw32

REM 1. Create folder for release
if not exist windows_release mkdir windows_release
echo Creating folder windows_release...

REM 2. Compile Client
echo.
echo Compiling game client...
gcc client_sdl.c client_network.c client_config.c client_audio.c client_utils.c client_input.c renderer_vulkan.c localization.c -o windows_release\game_client.exe ^
    -DUSE_VULKAN ^
    -I%SDL2_PATH%\include\SDL2 ^
    -I%SDL2_PATH%\include ^
    -I%IMG_PATH%\include ^
    -I%TTF_PATH%\include ^
    -I%MIX_PATH%\include ^
    -L%SDL2_PATH%\lib ^
    -L%IMG_PATH%\lib ^
    -L%TTF_PATH%\lib ^
    -L%MIX_PATH%\lib ^
    -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer ^
    -lws2_32 -lcomdlg32 -lopengl32 -lvulkan-1 -static-libgcc -static-libstdc++ -mwindows

if errorlevel 1 (
    echo ERROR: Client compilation failed!
    pause
    exit /b 1
)
echo Client compiled successfully!

REM 3. Compile Server
echo.
echo Compiling game server...
gcc server.c -o windows_release\server.exe ^
    -I%SDL2_PATH%\include\SDL2 ^
    -I%TTF_PATH%\include ^
    -L%SDL2_PATH%\lib ^
    -L%TTF_PATH%\lib ^
    -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf ^
    -lws2_32 -lsqlite3 -lm -lpthread -lpsapi -lgdi32

if errorlevel 1 (
    echo ERROR: Server compilation failed!
    pause
    exit /b 1
)
echo Server compiled successfully!

REM 4. Copy resources
echo.
echo Copying resources...
if exist map.jpg copy map.jpg windows_release\ >nul
if exist dungeon.png copy dungeon.png windows_release\ >nul
if exist player.png copy player.png windows_release\ >nul
if exist triggers.txt copy triggers.txt windows_release\ >nul
if exist DejaVuSans.ttf copy DejaVuSans.ttf windows_release\ >nul

REM 5. Copy music folder
if exist music (
    if not exist windows_release\music mkdir windows_release\music
    xcopy /E /I /Y music windows_release\music >nul
    echo Music folder copied.
)

REM 6. Copy DLL libraries from win_libs
echo Copying DLL libraries...

REM SDL2 DLLs
xcopy /Y "%SDL2_PATH%\bin\*.dll" windows_release\ >nul
REM SDL2_image DLLs (along with libpng, zlib etc)
xcopy /Y "%IMG_PATH%\bin\*.dll" windows_release\ >nul
REM SDL2_ttf DLLs (along with libfreetype)
xcopy /Y "%TTF_PATH%\bin\*.dll" windows_release\ >nul
REM SDL2_mixer DLLs (along with libogg, libvorbis etc)
xcopy /Y "%MIX_PATH%\bin\*.dll" windows_release\ >nul

echo.
echo ======================================
echo Build completed successfully!
echo ======================================
echo.
echo Windows build is located in windows_release directory
echo Run game_client.exe to start the client
echo Run server.exe to start the server
echo.
pause
