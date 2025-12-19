# SDL2 Android Libraries

This directory contains the SDL2 prebuilt libraries for Android.

**Supported Architectures:**
- ARM64 (arm64-v8a) - For ARM64 Android devices
- x86_64 - For x86_64 Android emulators and devices

## Required Libraries

Place the following `.so` files in each architecture directory:

```
jniLibs/
├── arm64-v8a/
│   ├── libSDL2.so
│   ├── libSDL2_image.so
│   ├── libSDL2_ttf.so
│   └── libSDL2_mixer.so
└── x86_64/
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
4. Build for both arm64-v8a and x86_64 ABIs

### Option 2: Build from Source

You can build each library separately for each architecture:

**SDL2:**
```bash
git clone https://github.com/libsdl-org/SDL.git SDL2
cd SDL2/build-scripts
# Build for arm64-v8a
./androidbuild.sh org.libsdl /path/to/android/ndk arm64-v8a
# Build for x86_64
./androidbuild.sh org.libsdl /path/to/android/ndk x86_64
```

**SDL2_image, SDL2_ttf, SDL2_mixer:**
```bash
# Clone each repository
git clone https://github.com/libsdl-org/SDL_image.git SDL2_image
git clone https://github.com/libsdl-org/SDL_ttf.git SDL2_ttf
git clone https://github.com/libsdl-org/SDL_mixer.git SDL2_mixer
# Follow the Android build instructions in each repository
# Make sure to build for both arm64-v8a and x86_64
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

- This project supports ARM64 (arm64-v8a) and x86_64 architectures
- ARM64 is for physical Android devices
- x86_64 is for Android emulators and x86_64 Android devices
- The libraries should be built with Android NDK version compatible with your project
- You may need to adjust CMakeLists.txt if using a different SDL2 build system
- Both architectures must use the same SDL2 version to ensure compatibility
