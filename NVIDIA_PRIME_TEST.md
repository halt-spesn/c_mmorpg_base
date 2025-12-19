# NVIDIA Prime GPU Selection Test

This document describes how to verify that the fix for NVIDIA Prime GPU selection is working correctly.

## Prerequisites

- Linux system with both Intel iGPU and NVIDIA discrete GPU (Optimus laptop)
- NVIDIA drivers installed
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
- **With NVIDIA Prime environment variables**: Game runs on NVIDIA GPU
- The fix ensures SDL2 properly respects the `__NV_PRIME_RENDER_OFFLOAD` and `__GLX_VENDOR_LIBRARY_NAME` environment variables

## Technical Details

The fix works by:

1. Detecting the NVIDIA Prime environment variables before SDL initialization
2. Setting `SDL_HINT_VIDEO_X11_FORCE_EGL` to "0" to ensure GLX (not EGL) is used, as EGL may not properly respect NVIDIA Prime offloading on some systems
3. Setting `SDL_GL_ACCELERATED_VISUAL` to 1 to request a hardware-accelerated OpenGL context
4. These settings are applied before any SDL initialization or OpenGL context creation

## Troubleshooting

If the fix doesn't work:

1. Ensure NVIDIA drivers are properly installed: `nvidia-smi` should work
2. Verify the environment variables are set correctly
3. Check that the system supports NVIDIA Prime render offload (NVIDIA driver >= 435.17)
4. Try running with `DRI_PRIME=1` as an alternative (older method)
5. Check system logs for any OpenGL or graphics driver errors
