#!/bin/bash

# Android Build Setup Helper Script
# This script helps prepare the Android build environment

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=========================================="
echo "C MMO Client - Android Build Setup"
echo "=========================================="
echo ""

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check prerequisites
echo "Checking prerequisites..."
echo ""

# Check for Android Studio / adb
if command_exists adb; then
    echo "✓ Android SDK found (adb available)"
    ADB_VERSION=$(adb version | head -1)
    echo "  $ADB_VERSION"
else
    echo "✗ Android SDK not found"
    echo "  Please install Android Studio: https://developer.android.com/studio"
    exit 1
fi

# Check for git
if command_exists git; then
    echo "✓ Git found"
else
    echo "✗ Git not found"
    echo "  Please install Git: https://git-scm.com/"
    exit 1
fi

echo ""
echo "=========================================="
echo "Setup Steps"
echo "=========================================="
echo ""

# Check for SDL2 libraries
echo "1. Checking SDL2 libraries..."
SDL_LIBS_FOUND=0
if [ -d "$SCRIPT_DIR/app/src/main/jniLibs/arm64-v8a" ]; then
    if [ -f "$SCRIPT_DIR/app/src/main/jniLibs/arm64-v8a/libSDL2.so" ]; then
        echo "   ✓ SDL2 libraries found"
        SDL_LIBS_FOUND=1
    fi
fi

if [ $SDL_LIBS_FOUND -eq 0 ]; then
    echo "   ✗ SDL2 libraries not found"
    echo ""
    echo "   You need to build or obtain SDL2 libraries for Android."
    echo "   Please see BUILD_GUIDE.md for detailed instructions."
    echo ""
    echo "   Quick start:"
    echo "   1. Clone SDL2 repositories:"
    echo "      git clone https://github.com/libsdl-org/SDL.git /tmp/SDL2"
    echo "      git clone https://github.com/libsdl-org/SDL_image.git /tmp/SDL2_image"
    echo "      git clone https://github.com/libsdl-org/SDL_ttf.git /tmp/SDL2_ttf"
    echo "      git clone https://github.com/libsdl-org/SDL_mixer.git /tmp/SDL2_mixer"
    echo ""
    echo "   2. Build each library for Android (see BUILD_GUIDE.md)"
    echo ""
    echo "   3. Copy .so files to:"
    echo "      $SCRIPT_DIR/app/src/main/jniLibs/[arch]/"
    echo ""
fi

# Check for SDL2 Java files
echo ""
echo "2. Checking SDL2 Java files..."
SDL_JAVA_FOUND=0
if [ -f "$SCRIPT_DIR/app/src/main/java/org/libsdl/app/SDLActivity.java" ]; then
    echo "   ✓ SDL2 Java files found"
    SDL_JAVA_FOUND=1
fi

if [ $SDL_JAVA_FOUND -eq 0 ]; then
    echo "   ✗ SDL2 Java files not found"
    echo ""
    echo "   You need to copy SDL2 Java files to the project."
    echo ""
    echo "   Quick start:"
    echo "   1. Clone SDL2 if not already done:"
    echo "      git clone https://github.com/libsdl-org/SDL.git /tmp/SDL2"
    echo ""
    echo "   2. Copy Java files:"
    echo "      cp -r /tmp/SDL2/android-project/app/src/main/java/org/libsdl \\"
    echo "            $SCRIPT_DIR/app/src/main/java/org/"
    echo ""
fi

# Check for game assets
echo ""
echo "3. Checking game assets..."
ASSETS_COMPLETE=1

if [ ! -f "$SCRIPT_DIR/app/src/main/assets/DejaVuSans.ttf" ]; then
    echo "   ✗ Font file missing: DejaVuSans.ttf"
    ASSETS_COMPLETE=0
fi

if [ ! -f "$SCRIPT_DIR/app/src/main/assets/triggers.txt" ]; then
    echo "   ✗ Triggers file missing: triggers.txt"
    ASSETS_COMPLETE=0
fi

if [ ! -s "$SCRIPT_DIR/app/src/main/assets/player.png" ]; then
    echo "   ⚠ Player sprite is placeholder or missing: player.png"
    echo "     Please replace with actual game graphic (32x32 or larger)"
    ASSETS_COMPLETE=0
fi

if [ ! -s "$SCRIPT_DIR/app/src/main/assets/map.jpg" ]; then
    echo "   ⚠ Map image is placeholder or missing: map.jpg"
    echo "     Please replace with actual game map (2000x2000)"
    ASSETS_COMPLETE=0
fi

MUSIC_COUNT=$(find "$SCRIPT_DIR/app/src/main/assets/music" -type f \( -name "*.mp3" -o -name "*.ogg" -o -name "*.wav" \) 2>/dev/null | wc -l)
if [ "$MUSIC_COUNT" -lt 1 ]; then
    echo "   ⚠ No music files found in music/ folder"
else
    echo "   ✓ Found $MUSIC_COUNT music file(s)"
fi

if [ $ASSETS_COMPLETE -eq 1 ]; then
    echo "   ✓ All required assets present"
fi

# Summary
echo ""
echo "=========================================="
echo "Summary"
echo "=========================================="
echo ""

if [ $SDL_LIBS_FOUND -eq 1 ] && [ $SDL_JAVA_FOUND -eq 1 ] && [ $ASSETS_COMPLETE -eq 1 ]; then
    echo "✓ Build environment is ready!"
    echo ""
    echo "Next steps:"
    echo "1. Open Android Studio"
    echo "2. Open the 'android' directory as a project"
    echo "3. Wait for Gradle sync"
    echo "4. Click Run to build and install"
    echo ""
    echo "Or use command line:"
    echo "  cd $SCRIPT_DIR"
    echo "  ./gradlew assembleDebug"
    echo ""
else
    echo "⚠ Build environment needs setup"
    echo ""
    echo "Please complete the missing steps above."
    echo "For detailed instructions, see: BUILD_GUIDE.md"
    echo ""
fi

echo "For complete build instructions:"
echo "  cat $SCRIPT_DIR/BUILD_GUIDE.md"
echo ""
