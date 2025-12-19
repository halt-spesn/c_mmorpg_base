game coded by gemini, gpt-5.1-codex-max, claude sonnet 4.5.


current issues: game(unable to walk), server(won't launch) does not runs in wine; window decors on mac os are black


to build on linux, install SDL2(_image, _mixer, _ttf), sqlite3, and run make.


to build windows version on linux, install mingw, and mingw versions of stuff above.


macos/ios can be built using xcode(arm64 ios/x86_64 mac os)


android port is available in the android/ directory - open project in android stuio and build apk(arm64, x86_64 is only supported architectures)


port can be assigned to server with a -p flag on launch