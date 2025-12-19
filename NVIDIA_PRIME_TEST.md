# NVIDIA Prime GPU Selection Test

This document describes how to verify that the fix for NVIDIA Prime GPU selection is working correctly.

## Prerequisites

- Linux system with both Intel iGPU and NVIDIA discrete GPU (Optimus laptop)
- NVIDIA drivers installed:
  - For X11: Driver >= 435.17 (Prime render offload support)
  - For Wayland: Driver >= 495 (Wayland EGL support)
- SDL2, SDL2_image, SDL2_ttf, and SDL2_mixer libraries installed
- Client binary built

## Testing Steps

### 1. Build the Client

```bash
make clean
make client
```

### 2. Test with Intel iGPU (Default)

Run the client without environment variables:

```bash
./client
```

Once the game window opens, check the renderer info in the UI or terminal output. It should show Intel GPU.

### 3. Test with NVIDIA GPU

Run the client with NVIDIA Prime environment variables:

```bash
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia ./client
```

Once the game window opens, check the renderer info. It should now show NVIDIA GPU.

### 4. Verify GPU Selection

You can also verify which GPU is being used with:

```bash
# In another terminal while the game is running:
nvidia-smi
```

When running with the NVIDIA environment variables, you should see the `client` process listed in the nvidia-smi output.

Alternatively, check the OpenGL renderer string that the game displays or logs during startup.

## Expected Results

- **Without environment variables**: Game runs on Intel iGPU
- **With NVIDIA Prime environment variables**: Game runs on NVIDIA GPU on both X11 and Wayland
- The fix ensures SDL2 properly respects the `__NV_PRIME_RENDER_OFFLOAD` and `__GLX_VENDOR_LIBRARY_NAME` environment variables

## Technical Details

The fix works by:

1. Detecting the NVIDIA Prime environment variables before SDL initialization
2. **Auto-detecting the display server** (Wayland or X11) using `WAYLAND_DISPLAY` and `XDG_SESSION_TYPE` environment variables
3. **On Wayland**: Forces Wayland video driver with EGL backend (requires NVIDIA driver >= 495)
4. **On X11**: Forces X11 video driver with GLX backend and disables EGL
5. Setting `SDL_GL_ACCELERATED_VISUAL` to 1 to request a hardware-accelerated OpenGL context
6. These settings are applied before any SDL initialization or OpenGL context creation

**Display Server Support:**
- **X11**: Uses GLX (OpenGL on X11) - traditional method, works with all NVIDIA drivers that support Prime
- **Wayland**: Uses EGL (OpenGL on Wayland) - requires NVIDIA driver >= 495 with Wayland support

The code automatically detects which display server you're running and configures SDL appropriately.

## Troubleshooting

If the fix doesn't work:

1. Ensure NVIDIA drivers are properly installed: `nvidia-smi` should work
2. Verify the environment variables are set correctly
3. Check that the system supports NVIDIA Prime render offload (NVIDIA driver >= 435.17)
4. **For Wayland**: Ensure NVIDIA driver >= 495 for Wayland support
5. **For X11**: Verify X11 is available (not Wayland-only)
6. Try running with `DRI_PRIME=1` as an alternative (older method for AMD/Intel)
7. Check system logs for any OpenGL or graphics driver errors
8. Run `glxinfo | grep -i "renderer"` (X11) or `es2_info | grep -i "renderer"` (Wayland) with and without the environment variables to verify GPU selection
3. Check that the system supports NVIDIA Prime render offload (NVIDIA driver >= 435.17)
4. **Verify X11 is available**: The fix requires X11. If you're on Wayland-only, X11 compatibility layer (XWayland) must be available.
5. Try running with `DRI_PRIME=1` as an alternative (older method for AMD/Intel)
6. Check system logs for any OpenGL or graphics driver errors
7. Run `glxinfo | grep -i "renderer"` with and without the environment variables to verify GPU selection
