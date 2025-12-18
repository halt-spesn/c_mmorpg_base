# Android Port - Quick Start

This is a quick reference for building the Android version of C MMO Client.

## Prerequisites

✓ Android Studio with SDK and NDK installed
✓ Git installed
✓ SDL2 libraries built for Android (or available pre-built)

## Quick Build Steps

### 1. Prepare SDL2 Libraries

You need SDL2, SDL2_image, SDL2_ttf, and SDL2_mixer built for Android.

**Option A: Use pre-built libraries** (recommended)
- Find or build SDL2 Android libraries
- Copy `.so` files to `android/app/src/main/jniLibs/[arch]/`

**Option B: Build from source**
```bash
# Clone SDL2
git clone https://github.com/libsdl-org/SDL.git
git clone https://github.com/libsdl-org/SDL_image.git
git clone https://github.com/libsdl-org/SDL_ttf.git
git clone https://github.com/libsdl-org/SDL_mixer.git

# Build each for Android (follow their build instructions)
# Copy resulting .so files to jniLibs/
```

### 2. Copy SDL2 Java Files

```bash
# Clone SDL2 if not done
git clone https://github.com/libsdl-org/SDL.git

# Copy Java files
cp -r SDL/android-project/app/src/main/java/org/libsdl \
      android/app/src/main/java/org/
```

### 3. Add Game Graphics

Replace placeholder files with actual game graphics:
- `android/app/src/main/assets/player.png` - Player sprite (32x32+)
- `android/app/src/main/assets/map.jpg` - Game map (2000x2000)

### 4. Build

**Using Android Studio:**
1. Open `android/` directory in Android Studio
2. Wait for Gradle sync
3. Click Run (Shift+F10)

**Using Command Line:**
```bash
cd android
./gradlew assembleDebug
adb install app/build/outputs/apk/debug/app-debug.apk
```

## Verify Setup

Run the setup checker:
```bash
cd android
./setup.sh
```

This will check for missing dependencies and guide you through setup.

## Troubleshooting

**SDL2 not found?**
- Ensure `.so` files are in `jniLibs/[arch]/libSDL2.so`, etc.
- Check architecture matches your device (arm64-v8a is most common)

**SDLActivity not found?**
- Copy SDL2 Java files to `java/org/libsdl/app/`

**Build fails?**
- Clean build: `./gradlew clean`
- Check Android SDK/NDK paths in Android Studio settings
- Ensure CMake 3.22.1+ is installed

**App crashes?**
- Check logs: `adb logcat | grep SDL`
- Verify all assets are present in `assets/` directory
- Test on physical device (emulators can be problematic)

## Directory Structure

```
android/
├── app/
│   ├── src/main/
│   │   ├── assets/           # Game assets (music, font, triggers, images)
│   │   ├── java/            # Java code (MainActivity + SDL classes)
│   │   ├── jniLibs/         # Native libraries (.so files)
│   │   └── res/             # Android resources
│   ├── CMakeLists.txt       # Native build config
│   └── build.gradle         # App build config
└── ...
```

## Assets Checklist

Required in `android/app/src/main/assets/`:
- [x] DejaVuSans.ttf - Font (included)
- [x] triggers.txt - Game triggers (included)
- [x] config.txt - Initial config (included)
- [x] servers.txt - Server list (included)
- [x] music/1.mp3 - At least one music file (included)
- [ ] player.png - Player sprite (NEEDS REPLACEMENT)
- [ ] map.jpg - Game map (NEEDS REPLACEMENT)

## Required Libraries

Must be in `android/app/src/main/jniLibs/[arch]/`:
- [ ] libSDL2.so
- [ ] libSDL2_image.so
- [ ] libSDL2_ttf.so
- [ ] libSDL2_mixer.so

Where `[arch]` is: armeabi-v7a, arm64-v8a, x86, x86_64

## Required Java Files

Must be in `android/app/src/main/java/org/libsdl/app/`:
- [ ] SDLActivity.java
- [ ] SDLSurface.java
- [ ] (and other SDL Java files)

## Testing

Minimum test device: Android 5.0 (API 21)
Recommended: Android 10+ (API 29+)

The game uses:
- Landscape orientation
- Touch controls
- Network connectivity (for multiplayer)
- Audio playback
- OpenGL ES 2.0 rendering

## Support

For detailed instructions, see:
- `BUILD_GUIDE.md` - Complete step-by-step guide
- `README.md` - General information
- Setup checker: `./setup.sh`

## File Paths on Android

The game uses two paths:

**Read-only assets** (via SDL_GetBasePath()):
- Location: `/data/app/com.mmo.client-*/base.apk/assets/`
- Contains: music, font, triggers.txt, images
- Cannot be modified at runtime

**Writable data** (via SDL_GetPrefPath()):
- Location: `/data/data/com.mmo.client/files/`
- Contains: config.txt, servers.txt (copied from assets on first run)
- Can be modified and persists across app restarts

## Next Steps

1. Run `./setup.sh` to check your setup
2. Follow any instructions to complete missing steps
3. Build and test on a device
4. Report any issues

Happy building! 🎮
