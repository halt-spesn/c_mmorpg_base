cmake_minimum_required(VERSION 3.22.1)

project(c_mmo_client C)

# Set C standard
set(CMAKE_C_STANDARD 11)

# Include directories for SDL2 headers and project root (for common.h)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

# Add the main client source files
add_library(client SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/../../client_sdl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../client_network.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../client_config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../client_audio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../client_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../client_input.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../localization.c
)

# Define the path to the prebuilt SDL2 libraries
set(SDL2_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI})

# Import SDL2 as a shared library
add_library(SDL2 SHARED IMPORTED)
set_target_properties(SDL2 PROPERTIES IMPORTED_LOCATION ${SDL2_LIB_DIR}/libSDL2.so)

# Import SDL2_image as a shared library
add_library(SDL2_image SHARED IMPORTED)
set_target_properties(SDL2_image PROPERTIES IMPORTED_LOCATION ${SDL2_LIB_DIR}/libSDL2_image.so)

# Import SDL2_ttf as a shared library
add_library(SDL2_ttf SHARED IMPORTED)
set_target_properties(SDL2_ttf PROPERTIES IMPORTED_LOCATION ${SDL2_LIB_DIR}/libSDL2_ttf.so)

# Import SDL2_mixer as a shared library
add_library(SDL2_mixer SHARED IMPORTED)
set_target_properties(SDL2_mixer PROPERTIES IMPORTED_LOCATION ${SDL2_LIB_DIR}/libSDL2_mixer.so)

# Link against SDL2 libraries and Android libraries
target_link_libraries(client
    SDL2
    SDL2_image
    SDL2_ttf
    SDL2_mixer
    GLESv2
    log
    android
    m
)

# Define Android platform
target_compile_definitions(client PRIVATE __ANDROID__)
