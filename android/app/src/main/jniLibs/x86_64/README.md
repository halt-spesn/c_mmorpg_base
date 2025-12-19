# x86_64 Architecture Libraries

This directory should contain SDL2 prebuilt libraries for x86_64 architecture.

## Required Libraries

Place the following `.so` files in this directory:

- `libSDL2.so`
- `libSDL2_image.so`
- `libSDL2_ttf.so`
- `libSDL2_mixer.so`

## Note

These libraries will be added by the project maintainer. The x86_64 architecture is used for:
- Android emulators running on x86_64 hosts
- x86_64 Android devices (rare but supported)

Build these libraries from the same SDL2 sources as arm64-v8a, but targeting the x86_64 ABI.
