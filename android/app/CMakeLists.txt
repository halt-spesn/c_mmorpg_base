cmake_minimum_required(VERSION 3.22.1)

project(c_mmo_client C)

# Set C standard
set(CMAKE_C_STANDARD 11)

# Add SDL2 libraries (assuming they will be built statically and provided)
# User will need to build SDL2, SDL2_image, SDL2_ttf, SDL2_mixer for Android
# and place them in the appropriate location

# Include directories for SDL2 headers
# The user should set SDL2_PATH to point to their SDL2 Android build
set(SDL2_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../SDL2" CACHE PATH "Path to SDL2 Android build")

# If SDL2_PATH doesn't exist, try to use system includes
if(EXISTS "${SDL2_PATH}/include")
    include_directories(${SDL2_PATH}/include)
else()
    # Fallback to standard locations
    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/../../SDL2/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../SDL2_image
        ${CMAKE_CURRENT_SOURCE_DIR}/../../SDL2_ttf
        ${CMAKE_CURRENT_SOURCE_DIR}/../../SDL2_mixer
    )
endif()

# Add the main client source file
add_library(client SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/../../client_sdl.c
)

# Find and link SDL2 libraries
# User needs to build these libraries for Android and make them available
find_library(SDL2_LIBRARY SDL2)
find_library(SDL2_IMAGE_LIBRARY SDL2_image)
find_library(SDL2_TTF_LIBRARY SDL2_ttf)
find_library(SDL2_MIXER_LIBRARY SDL2_mixer)

# Link against SDL2 libraries
target_link_libraries(client
    ${SDL2_LIBRARY}
    ${SDL2_IMAGE_LIBRARY}
    ${SDL2_TTF_LIBRARY}
    ${SDL2_MIXER_LIBRARY}
    GLESv2
    log
    android
    m
)

# Define Android platform
target_compile_definitions(client PRIVATE __ANDROID__)
