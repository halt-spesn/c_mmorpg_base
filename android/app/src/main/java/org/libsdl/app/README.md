# SDL2 Java Source Files

This directory should contain the SDL2 Java source files from the SDL2 Android project.

## Required Files

Copy these files from the SDL2 source repository:

From: `SDL2/android-project/app/src/main/java/org/libsdl/app/`

Files needed:
- SDLActivity.java
- SDLAudioManager.java  
- SDLControllerManager.java
- SDLSurface.java
- HIDDevice.java
- HIDDeviceBLESteamController.java
- HIDDeviceManager.java
- HIDDeviceUSB.java
- SDL.java
- SDLDummyEdit.java
- SDLGenericMotionListener_API12.java
- SDLGenericMotionListener_API24.java
- SDLGenericMotionListener_API26.java
- SDLHapticHandler.java
- SDLHapticHandler_API26.java
- SDLJoystickHandler.java
- SDLJoystickHandler_API12.java
- SDLJoystickHandler_API16.java
- SDLJoystickHandler_API19.java
- SDLMain.java

## How to Get SDL2 Java Files

### Option 1: Clone SDL2 Repository
```bash
git clone https://github.com/libsdl-org/SDL.git
cp -r SDL/android-project/app/src/main/java/org/libsdl android/app/src/main/java/
```

### Option 2: Download from GitHub
1. Go to https://github.com/libsdl-org/SDL
2. Navigate to `android-project/app/src/main/java/org/libsdl/app/`
3. Download all `.java` files
4. Place them in `android/app/src/main/java/org/libsdl/app/`

### Option 3: Use SDL2 Release
Download a release from https://github.com/libsdl-org/SDL/releases and extract the Java files.

## Note

The MainActivity.java in this project extends `org.libsdl.app.SDLActivity`, so these files are required for the build to succeed.
