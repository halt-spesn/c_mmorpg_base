game coded by gemini, gpt-5.1-codex-max, claude sonnet 4.5.


current issues: game(unable to walk), server(won't launch) does not runs in wine; window decors on mac os are black


## Building

### Linux

To build on linux, install SDL2(_image, _mixer, _ttf), sqlite3, and run make.

For **Vulkan support** (optional), install Vulkan SDK (libvulkan-dev) and build with:
```bash
make USE_VULKAN=1
```

To run with Vulkan rendering:
```bash
./client --vulkan
```
Or use the short form:
```bash
./client -vk
```

### Windows

To build windows version on linux, install mingw, and mingw versions of stuff above.

### macOS/iOS

macos/ios can be built using xcode(arm64 ios/x86_64 mac os)

### Android

android port is available in the android/ directory - open project in android stuio and build apk(arm64, x86_64 is only supported architectures)

Android supports Vulkan rendering on devices with Vulkan 1.0+ support.


## Server

port can be assigned to server with a -p flag on launch

## Rendering Backends

The client supports two rendering backends:
- **OpenGL/GLES** (default) - Works on all platforms including mobile
- **Vulkan** (optional) - Better performance on desktop, requires Vulkan SDK

Vulkan support is compile-time optional and runtime selectable. The client will automatically fall back to OpenGL if Vulkan initialization fails.