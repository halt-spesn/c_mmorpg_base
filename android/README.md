# Android Port for C MMO Client

This directory contains the Android port of the C MMO game client.

## Prerequisites

Before building the Android version, you need to:

1. **Android Studio** - Install Android Studio with Android SDK
2. **Android NDK** - Install NDK version 25 or later
3. **SDL2 Libraries** - Build SDL2 and related libraries statically for Android

## Building SDL2 Libraries for Android

The game requires the following SDL2 libraries to be built for Android:
- SDL2
- SDL2_image
- SDL2_ttf
- SDL2_mixer

You can build these libraries using the official SDL2 Android build system:

1. Download SDL2 source from https://www.libsdl.org/
2. Download SDL2_image, SDL2_ttf, and SDL2_mixer sources
3. Follow the Android build instructions for each library
4. Copy the built `.so` files to `android/app/src/main/jniLibs/[arch]/`
   - Where [arch] is: armeabi-v7a, arm64-v8a, x86, x86_64

Alternatively, you can use SDL2 prebuilt libraries or build scripts available in the SDL2 repositories.

## Project Structure

```
android/
├── app/
│   ├── src/main/
│   │   ├── assets/          # Game assets (read-only)
│   │   │   ├── music/       # Music files (.mp3, .ogg, .wav)
│   │   │   ├── DejaVuSans.ttf  # Font file
│   │   │   ├── triggers.txt    # Game triggers
│   │   │   ├── player.png      # Player sprite (needs to be added)
│   │   │   ├── map.jpg         # Game map (needs to be added)
│   │   │   ├── config.txt      # Initial config (will be copied to writable storage)
│   │   │   └── servers.txt     # Initial server list (will be copied to writable storage)
│   │   ├── java/            # Java source files
│   │   │   └── com/mmo/client/
│   │   │       └── MainActivity.java
│   │   ├── jniLibs/         # Native libraries (SDL2 .so files)
│   │   ├── res/             # Android resources
│   │   └── AndroidManifest.xml
│   ├── CMakeLists.txt       # CMake build configuration
│   └── build.gradle         # App build configuration
├── build.gradle             # Project build configuration
├── settings.gradle          # Project settings
└── gradlew                  # Gradle wrapper script
```

## Asset Management

The Android port uses a dual-path system for assets:

### Read-only Assets (from assets/ folder):
- `music/` - Music files
- `DejaVuSans.ttf` - Font file
- `triggers.txt` - Game triggers configuration
- `player.png` - Player sprite image
- `map.jpg` - Game map image

These files are packaged with the app and accessed via `SDL_GetBasePath()`.

### Writable Files (in app-specific storage):
- `config.txt` - User configuration (color settings, volume, etc.)
- `servers.txt` - Server list (can be modified by user)

These files are stored in the app's private directory accessed via `SDL_GetPrefPath()`.
On first launch, the initial versions from assets/ are copied to the writable location.

## Building the APK

1. Open the `android/` directory in Android Studio, or use the command line:

```bash
cd android
./gradlew assembleDebug
```

2. The APK will be generated in: `android/app/build/outputs/apk/debug/app-debug.apk`

3. For release builds:

```bash
./gradlew assembleRelease
```

## Adding Game Assets

Before building, you need to add the following image files to `android/app/src/main/assets/`:

1. **player.png** - The player character sprite (32x32 or larger)
2. **map.jpg** - The game map image (2000x2000 as defined in common.h)

You can create simple placeholder images or use actual game graphics.

## Running on Device/Emulator

1. Connect an Android device or start an emulator
2. Enable USB debugging on the device
3. Run:

```bash
./gradlew installDebug
```

Or use Android Studio's Run button.

## Platform-Specific Code

The client code (`client_sdl.c`) already includes Android support:
- `__ANDROID__` preprocessor checks for platform-specific code
- `SDL_GetPrefPath()` for writable storage on mobile
- `SDL_GetBasePath()` for read-only assets
- OpenGL ES 2.0 rendering support

## Permissions

The app requests the following permissions (defined in AndroidManifest.xml):
- `INTERNET` - For network connectivity to game servers
- `ACCESS_NETWORK_STATE` - To check network availability
- `WRITE_EXTERNAL_STORAGE` - For older Android versions (API < 29)
- `READ_EXTERNAL_STORAGE` - For older Android versions (API < 29)

On Android 10+, the app uses scoped storage and doesn't need external storage permissions.

## Troubleshooting

### SDL2 Libraries Not Found
Make sure you've built and placed the SDL2 `.so` files in the correct directories:
- `android/app/src/main/jniLibs/armeabi-v7a/libSDL2.so`
- `android/app/src/main/jniLibs/arm64-v8a/libSDL2.so`
- etc.

### Assets Not Loading
Check that all required assets are in `android/app/src/main/assets/`:
- DejaVuSans.ttf
- triggers.txt
- music/ folder with at least one music file
- player.png
- map.jpg

### Build Errors
1. Check that you have Android SDK and NDK installed
2. Verify CMake version 3.22.1 or later is available
3. Make sure Java 8 or later is configured

## SDL2 Java Files

The MainActivity extends `org.libsdl.app.SDLActivity`, which requires SDL2's Java files. These are typically included in the SDL2 Android build or can be copied from the SDL2 Android project template.

## Notes

- The game runs in landscape orientation by default
- Fullscreen mode is enabled for immersive gameplay
- Hardware acceleration is enabled for better performance
- The app supports all major Android architectures (ARM, ARM64, x86, x86_64)
