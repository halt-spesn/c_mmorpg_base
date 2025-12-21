game coded by gemini, gpt-5.1-codex-max, claude sonnet 4.5.


current issues: game(unable to walk), server(won't launch) does not runs in wine; window decors on mac os are black


to build on linux, install SDL2(_image, _mixer, _ttf), sqlite3, and run make.


to build windows version on linux, install mingw, and mingw versions of stuff above.


macos/ios can be built using xcode(arm64 ios/x86_64 mac os)


android port is available in the android/ directory - open project in android stuio and build apk(arm64, x86_64 is only supported architectures)


port can be assigned to server with a -p flag on launch


## Mobile Features (iOS/Android)

### Text Formatting Menu
On mobile devices, long-press (hold for 500ms) on any text input field to show a context menu with:
- **Cut**: Copy and delete selected text
- **Copy**: Copy selected text to clipboard
- **Paste**: Insert clipboard text
- **Clear**: Delete selected text or clear field

The menu works on all text fields including username, password, IP, chat, and settings fields.
