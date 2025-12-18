# SDL2 Android Libraries

This directory should contain the SDL2 prebuilt libraries for Android, or you should build them yourself.

**Note:** This project only supports ARM64 (arm64-v8a) architecture.

## Required Libraries

Place the following `.so` files in the arm64-v8a directory:

```
jniLibs/
└── arm64-v8a/
    ├── libSDL2.so
    ├── libSDL2_image.so
    ├── libSDL2_ttf.so
    └── libSDL2_mixer.so
```

## Building SDL2 for Android

### Option 1: Use SDL2 Android Project Template

1. Download SDL2 source from https://github.com/libsdl-org/SDL
2. Use the Android project template in `android-project/`
3. Build using Android Studio or gradlew

### Option 2: Build from Source

You can build each library separately:

**SDL2:**
```bash
git clone https://github.com/libsdl-org/SDL.git SDL2
cd SDL2/build-scripts
./androidbuild.sh org.libsdl /path/to/android/ndk
```

**SDL2_image:**
```bash
git clone https://github.com/libsdl-org/SDL_image.git SDL2_image
# Follow the Android build instructions in the repository
```

**SDL2_ttf:**
```bash
git clone https://github.com/libsdl-org/SDL_ttf.git SDL2_ttf
# Follow the Android build instructions in the repository
```

**SDL2_mixer:**
```bash
git clone https://github.com/libsdl-org/SDL_mixer.git SDL2_mixer
# Follow the Android build instructions in the repository
```

### Option 3: Pre-built Libraries

You may find pre-built SDL2 Android libraries at:
- https://github.com/libsdl-org/SDL/releases
- Community-maintained builds

## Additional SDL2 Java Files

You'll also need to copy SDL2's Java source files to your project:

From SDL2 source, copy:
```
SDL2/android-project/app/src/main/java/org/libsdl/app/*
```

To:
```
android/app/src/main/java/org/libsdl/app/
```

These Java files include:
- SDLActivity.java - Main SDL activity
- SDLAudioManager.java - Audio management
- SDLControllerManager.java - Game controller support
- SDLSurface.java - SDL rendering surface
- And other supporting files

## Notes

- Only ARM64 (arm64-v8a) architecture is supported by this project
- The libraries should be built with Android NDK version compatible with your project
- You may need to adjust CMakeLists.txt if using a different SDL2 build system
