# Building C MMO Client for Android - Complete Guide

This guide walks you through building the Android version of the C MMO Client from scratch.

## Prerequisites

### Required Software

1. **Android Studio** (latest stable version)
   - Download from: https://developer.android.com/studio
   - Install with default options

2. **Android SDK** (automatically installed with Android Studio)
   - Minimum SDK: API 21 (Android 5.0)
   - Target SDK: API 34 (Android 14)
   - Install via Android Studio SDK Manager

3. **Android NDK** (Native Development Kit)
   - Minimum version: NDK r25 or later
   - Install via Android Studio SDK Manager:
     - Tools → SDK Manager → SDK Tools tab
     - Check "NDK (Side by side)" and "CMake"
     - Click Apply to install

4. **Git** (to clone SDL2 sources)
   - Linux: `sudo apt-get install git`
   - macOS: `brew install git`
   - Windows: Download from https://git-scm.com/

5. **Python 3** (optional, for helper scripts)

## Step 1: Prepare SDL2 Libraries

The game requires SDL2 and its extension libraries. You have two options:

### Option A: Use Pre-built SDL2 Libraries (Recommended)

If you can find pre-built SDL2 Android libraries (.so files), skip to Step 2.

### Option B: Build SDL2 from Source

#### 1. Clone SDL2 Repositories

```bash
cd /tmp
git clone https://github.com/libsdl-org/SDL.git SDL2
git clone https://github.com/libsdl-org/SDL_image.git SDL2_image
git clone https://github.com/libsdl-org/SDL_ttf.git SDL2_ttf
git clone https://github.com/libsdl-org/SDL_mixer.git SDL2_mixer
```

#### 2. Build SDL2 for Android

Each SDL2 library comes with Android build scripts. For each library:

```bash
cd SDL2
# Follow the instructions in android-project/README.md
# Or use the automated build script:
./build-scripts/androidbuild.sh org.libsdl /path/to/your/ndk
```

Repeat for SDL2_image, SDL2_ttf, and SDL2_mixer.

#### 3. Extract Built Libraries

After building, locate the `.so` files in:
```
SDL2/build-android/libs/[architecture]/libSDL2.so
```

**Note:** This project only supports ARM64 (arm64-v8a) architecture.

## Step 2: Copy SDL2 Libraries to Project

Copy the built `.so` files to the project:

```bash
cd c_mmorpg_base/android/app/src/main/jniLibs

# Create ARM64 architecture directory if it doesn't exist
mkdir -p arm64-v8a

# Copy libraries for ARM64
cp /path/to/SDL2/build-android/libs/arm64-v8a/libSDL2.so arm64-v8a/
cp /path/to/SDL2_image/build-android/libs/arm64-v8a/libSDL2_image.so arm64-v8a/
cp /path/to/SDL2_ttf/build-android/libs/arm64-v8a/libSDL2_ttf.so arm64-v8a/
cp /path/to/SDL2_mixer/build-android/libs/arm64-v8a/libSDL2_mixer.so arm64-v8a/
```

## Step 3: Copy SDL2 Java Files

SDL2 includes Java wrapper classes needed for Android:

```bash
cd c_mmorpg_base/android/app/src/main/java

# Copy SDL2 Java files
cp -r /path/to/SDL2/android-project/app/src/main/java/org/libsdl .
```

The following files should now exist:
```
org/libsdl/app/
├── SDLActivity.java
├── SDLAudioManager.java
├── SDLControllerManager.java
├── SDLSurface.java
└── (other SDL Java files)
```

## Step 4: Add Game Assets

Ensure all required assets are in `android/app/src/main/assets/`:

```bash
cd c_mmorpg_base/android/app/src/main/assets

# Check required files:
ls -l DejaVuSans.ttf    # Font file
ls -l triggers.txt      # Game triggers
ls -l player.png        # Player sprite (needs to be actual game graphic)
ls -l map.jpg          # Game map (needs to be actual game graphic)
ls -l music/           # Music folder with at least one music file
```

**Important:** Replace the placeholder `player.png` and `map.jpg` with actual game graphics:
- `player.png`: 32x32 pixel sprite (or larger)
- `map.jpg`: 2000x2000 pixel game map

## Step 5: Build the APK

### Using Android Studio (Recommended)

1. Open Android Studio
2. Click "Open an existing project"
3. Navigate to `c_mmorpg_base/android` and click OK
4. Wait for Gradle sync to complete
5. Connect an Android device or start an emulator
6. Click the green "Run" button (or press Shift+F10)
7. Select your device and click OK

### Using Command Line

```bash
cd c_mmorpg_base/android

# Build debug APK
./gradlew assembleDebug

# Or build release APK
./gradlew assembleRelease

# Install to connected device
./gradlew installDebug
```

The APK will be generated at:
- Debug: `app/build/outputs/apk/debug/app-debug.apk`
- Release: `app/build/outputs/apk/release/app-release.apk`

## Step 6: Run on Device

### Enable Developer Options

1. On your Android device, go to Settings → About Phone
2. Tap "Build Number" 7 times to enable Developer Options
3. Go to Settings → Developer Options
4. Enable "USB Debugging"

### Install and Run

```bash
# Connect device via USB
adb devices  # Verify device is connected

# Install APK
cd c_mmorpg_base/android
./gradlew installDebug

# Or manually install:
adb install app/build/outputs/apk/debug/app-debug.apk
```

## Troubleshooting

### Build Errors

#### "SDL2 not found"
- Verify SDL2 `.so` files are in `jniLibs/[arch]/`
- Check CMakeLists.txt paths are correct

#### "SDLActivity cannot be resolved"
- Ensure SDL2 Java files are copied to `java/org/libsdl/app/`
- Clean and rebuild: `./gradlew clean build`

#### "NDK not configured"
- Install NDK via Android Studio SDK Manager
- Set NDK path in `local.properties`: `ndk.dir=/path/to/ndk`

#### "CMake version mismatch"
- Install CMake 3.22.1 or later via SDK Manager

### Runtime Errors

#### "App crashes on launch"
- Check logcat: `adb logcat | grep MMO`
- Verify all `.so` files are present for device architecture
- Check that assets are properly packaged in APK

#### "Assets not loading"
- Verify assets are in `src/main/assets/` before building
- Check file names match exactly (case-sensitive)
- Use `adb shell` to inspect files in device

#### "Network connection fails"
- Verify `INTERNET` permission in AndroidManifest.xml
- Check server IP is accessible from device
- Try using 10.0.2.2 for emulator (host machine's localhost)

### Debugging

View logs in real-time:
```bash
adb logcat | grep -i "SDL\|MMO\|client"
```

Check app-specific storage:
```bash
adb shell
cd /data/data/com.mmo.client/files
ls -la
```

## Advanced Configuration

### Architecture Support

This project is configured to build only for ARM64 (arm64-v8a) architecture, which covers the vast majority of modern Android devices.

The configuration in `app/build.gradle` is already set:
```gradle
ndk {
    abiFilters 'arm64-v8a'  // Only ARM64
}
```

### Signing Release APK

For release builds, you need a keystore:

```bash
# Generate keystore
keytool -genkey -v -keystore my-release-key.jks -keyalg RSA -keysize 2048 -validity 10000 -alias my-alias

# Add to app/build.gradle:
android {
    signingConfigs {
        release {
            storeFile file("my-release-key.jks")
            storePassword "password"
            keyAlias "my-alias"
            keyPassword "password"
        }
    }
    buildTypes {
        release {
            signingConfig signingConfigs.release
        }
    }
}
```

## Optimizing APK Size

The project is already optimized for ARM64 only. Additional optimizations:

1. Enable ProGuard/R8 minification
2. Use APK splits if supporting multiple architectures in the future
3. Compress assets (JPG instead of PNG for large images)

## Building a Complete Distribution

For a production release:

1. Replace placeholder assets with final graphics
2. Test on ARM64 devices
3. Sign APK with release keystore
4. Run lint checks: `./gradlew lint`
5. Generate release APK: `./gradlew assembleRelease`
6. Test release APK thoroughly
7. Prepare for Google Play Store:
   - Create screenshots
   - Write description
   - Set up content rating
   - Upload APK or App Bundle

## Resources

- SDL2 for Android: https://wiki.libsdl.org/SDL2/README/android
- Android Developer Guide: https://developer.android.com/guide
- SDL2 GitHub: https://github.com/libsdl-org/SDL
- NDK Documentation: https://developer.android.com/ndk

## Quick Reference

```bash
# Common commands
./gradlew clean                  # Clean build
./gradlew assembleDebug         # Build debug APK
./gradlew installDebug          # Build and install
adb logcat | grep SDL           # View SDL logs
adb devices                     # List connected devices
adb install app.apk             # Install APK manually
adb uninstall com.mmo.client   # Uninstall app
```

## Notes

- First build will take longer (downloads dependencies)
- Emulator performance may vary; test on real device when possible
- ARM64 is the most common architecture for modern Android devices
- The game will use landscape orientation by default
- Config and server files will be stored in app-private storage and persist across app restarts
