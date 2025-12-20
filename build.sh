#!/bin/bash
# Build script for C MMO Client with optional Vulkan support

set -e

echo "======================================"
echo "  C MMO Client Build Script"
echo "======================================"
echo

# Check if Vulkan support is requested
VULKAN_FLAG=""
if [ "$1" == "--vulkan" ] || [ "$1" == "-vk" ]; then
    echo "Building with Vulkan support..."
    VULKAN_FLAG="USE_VULKAN=1"
    
    # Check if Vulkan SDK is available
    if ! pkg-config --exists vulkan 2>/dev/null; then
        echo "Warning: Vulkan SDK not found. Install libvulkan-dev (Debian/Ubuntu) or vulkan-devel (Fedora)"
        echo "Continuing anyway - build may fail if Vulkan headers are missing"
    else
        echo "Vulkan SDK found: $(pkg-config --modversion vulkan)"
    fi
else
    echo "Building with OpenGL support (default)"
    echo "To build with Vulkan, run: $0 --vulkan"
fi

echo
echo "Building server..."
make server

echo
echo "Building client..."
make client $VULKAN_FLAG

echo
echo "======================================"
echo "  Build Complete!"
echo "======================================"
echo
echo "Run the server:"
echo "  ./server"
echo
echo "Run the client:"
if [ -n "$VULKAN_FLAG" ]; then
    echo "  ./client --vulkan   (use Vulkan)"
    echo "  ./client            (fallback to OpenGL)"
else
    echo "  ./client"
fi
echo
