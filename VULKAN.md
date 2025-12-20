# Vulkan Rendering Support - Implementation Guide

This document describes the Vulkan rendering support added to the C MMO client.

## Overview

The C MMO client now supports two rendering backends:
- **OpenGL/GLES** (default) - Cross-platform, works on all devices
- **Vulkan** (optional) - Modern graphics API, better performance on desktop

## Architecture

### Dual Backend Design

The implementation uses a hybrid approach:
- **Vulkan**: Handles the main rendering loop and presentation
- **SDL_Renderer**: Still used for UI elements, text, and textures

This approach maintains compatibility while adding modern rendering support.

### Files Structure

```
renderer_vulkan.h     - Vulkan renderer interface
renderer_vulkan.c     - Vulkan implementation (~850 lines)
client_sdl.c          - Integration and backend selection
Makefile              - Build configuration
build.sh              - Helper build script
```

## Building

### Linux

```bash
# Install dependencies
sudo apt-get install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev \
                     libsdl2-mixer-dev libvulkan-dev

# Build with Vulkan support
make USE_VULKAN=1

# Or use the build script
./build.sh --vulkan
```

### Windows (MinGW)

```bash
# Install Vulkan SDK from LunarG
# Then build
make USE_VULKAN=1
```

### macOS

Vulkan support on macOS requires MoltenVK (Vulkan over Metal).

```bash
# Install Vulkan SDK
brew install vulkan-sdk

# Build
make USE_VULKAN=1
```

## Running

### Command Line Options

```bash
# Use Vulkan rendering
./client --vulkan
./client -vk

# Use OpenGL rendering (default)
./client
```

### Configuration File

The rendering backend preference is saved in `config.txt` (field 15).

Format: `... UI_SCALE GAME_ZOOM RENDER_BACKEND`

Where RENDER_BACKEND is:
- `0` = OpenGL
- `1` = Vulkan

## Features

### Automatic GPU Selection
- Enumerates all available GPUs
- Selects first suitable GPU with Vulkan support
- Validates required extensions (VK_KHR_swapchain)

### Validation Layers
- Enabled in debug builds (`NDEBUG` not defined)
- Provides error checking and warnings
- Helps catch Vulkan API misuse

### Frame Synchronization
- Double buffering (2 frames in flight)
- Proper semaphore/fence synchronization
- Prevents tearing and frame drops

### Window Resize Handling
- Automatic swapchain recreation
- Handles VK_ERROR_OUT_OF_DATE_KHR
- Graceful degradation on resize

### Fallback Mechanism
- If Vulkan initialization fails, falls back to OpenGL
- No user intervention needed
- Seamless transition

## Debug Information

When debug overlay is enabled (press a key in settings), it shows:
- Active rendering backend (OpenGL/Vulkan)
- Vulkan shown in cyan color
- GPU information
- System information

## Implementation Details

### Initialization Sequence

1. Parse command line arguments
2. Load config file (get backend preference)
3. Initialize SDL and libraries
4. Create window
5. If Vulkan requested:
   - Create Vulkan instance
   - Create surface
   - Select physical device
   - Create logical device
   - Create swapchain
   - Create render pass
   - Create framebuffers
   - Create command buffers
   - Create sync objects
6. If Vulkan fails or not requested, create SDL_Renderer

### Frame Rendering Loop

```c
// Vulkan frame
vulkan_begin_frame()      // Acquire image, begin command buffer
  // Vulkan rendering commands go here
vulkan_end_frame()        // Submit, present

// SDL_Renderer for UI
render_auth_screen() or render_game()
```

### Memory Management

All memory allocations are checked for NULL:
```c
ptr = malloc(size);
if (!ptr) {
    // Handle error
    return 0;
}
```

### Error Handling

- All Vulkan functions check return codes
- Detailed error messages with printf
- Graceful degradation on failure
- Proper cleanup on error paths

## Performance Considerations

### Vulkan Advantages
- Lower CPU overhead
- Better multi-threading support
- Explicit control over GPU operations
- Modern graphics features

### Current Limitations
- UI still uses SDL_Renderer (CPU blitting)
- Basic render pass (single subpass)
- No shader customization yet
- Limited to 2D rendering

### Future Improvements
- Direct Vulkan texture rendering
- Custom shaders for effects
- Compute shaders for game logic
- Multi-threaded command buffer recording

## Platform Support

| Platform | OpenGL | Vulkan | Notes |
|----------|--------|--------|-------|
| Linux    | ✅     | ✅     | Full support |
| Windows  | ✅     | ✅     | Requires Vulkan SDK |
| macOS    | ✅     | ⚠️     | Via MoltenVK |
| Android  | ✅     | ⚠️     | Native Vulkan API available |
| iOS      | ✅     | ⚠️     | Via MoltenVK |

## Troubleshooting

### Vulkan initialization fails
- Check if Vulkan SDK is installed
- Update graphics drivers
- Check `VK_LAYER_PATH` environment variable
- Client will fall back to OpenGL automatically

### Black screen with Vulkan
- Check if GPU supports Vulkan 1.0+
- Verify swapchain creation succeeded
- Enable validation layers for debugging

### Poor performance
- Check if using integrated vs dedicated GPU
- Verify using `--vulkan` flag
- Check debug overlay for active backend

## API Reference

### renderer_vulkan.h

```c
// Initialize Vulkan renderer
int vulkan_init(SDL_Window *window, VulkanRenderer *vk_renderer);

// Begin frame rendering
int vulkan_begin_frame(VulkanRenderer *vk_renderer, uint32_t *image_index);

// End frame rendering and present
int vulkan_end_frame(VulkanRenderer *vk_renderer, uint32_t image_index);

// Handle window resize
void vulkan_handle_resize(VulkanRenderer *vk_renderer);

// Cleanup Vulkan resources
void vulkan_cleanup(VulkanRenderer *vk_renderer);
```

## Contributing

When adding Vulkan features:
1. Check for NULL on all malloc calls
2. Validate Vulkan function return codes
3. Test fallback to OpenGL
4. Update this documentation
5. Test on multiple platforms

## License

Same as parent project.

## Credits

- Implementation: Claude Sonnet 4.5
- Testing: Project team
- Vulkan SDK: LunarG
- SDL2: Sam Lantinga and contributors
