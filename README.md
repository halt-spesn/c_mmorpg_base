game coded by gemini, gpt-5.1-codex-max, claude sonnet 4.5.


current issues: game(unable to walk), server(won't launch) does not runs in wine; window decors on mac os are black


to build on linux, install SDL2(_image, _mixer, _ttf), sqlite3, and run make.


to build windows version on linux, install mingw, and mingw versions of stuff above.


macos/ios can be built using xcode(arm64 ios/x86_64 mac os)


android port is available in the android/ directory - open project in android stuio and build apk(arm64, x86_64 is only supported architectures)


port can be assigned to server with a -p flag on launch

## Telemetry

The server automatically collects GL renderer and OS information from connected clients to help track hardware/OS usage statistics.

### Telemetry Files

Two files are created in the server directory:

- **telemetryGL.txt** - Lists GPU/renderer information with unique user counts
  - Example: `Intel UHD Graphics: 1 user`, `NVIDIA GeForce RTX 3060: 3 users`

- **telemetryOS.txt** - Lists operating system information with unique user counts
  - Example: `Windows: 2 users`, `Linux 5.15.0: 1 user`

### How It Works

- Telemetry data is automatically sent when a user successfully logs in
- Each unique combination of user ID + GL renderer/OS is tracked separately
- If the same user logs in from different hardware/OS, both entries are recorded
- Duplicate entries (same user, same hardware) are automatically prevented
- Files are updated in real-time as users log in
- Telemetry data accumulates during server runtime (resets on server restart)