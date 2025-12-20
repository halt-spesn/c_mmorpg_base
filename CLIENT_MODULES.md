# Client Code Organization

## Overview
The client code has been refactored from a single large file (client_sdl.c with 5005 lines) into multiple modular source files for better maintainability and organization.

## Module Structure

### client.h (457 lines)
Common header file with shared declarations:
- Type definitions (enums, structs)
- Global variable declarations (extern)
- Function prototypes for all modules
- Platform-specific macros
- Constants and configuration values

### client_network.c (111 lines)
Network communication functionality:
- `send_all()` - Reliable data sending
- `send_packet()` - Packet transmission wrapper
- `recv_total()` - Complete packet reception
- `try_connect()` - Connection establishment
- `get_os_info()` - OS information gathering
- `send_telemetry()` - Telemetry data transmission

### client_config.c (205 lines)
Configuration and persistence management:
- `get_path()` - Platform-specific path resolution
- `ensure_save_file()` - Save file initialization
- `load_config()` / `save_config()` - Settings persistence
- `load_servers()` / `save_servers()` - Server list management
- `add_server_to_list()` - Server entry addition
- `load_profiles()` / `save_profiles()` - Profile management
- `add_profile_to_list()` - Profile entry addition
- `load_triggers()` - Trigger system initialization
- `receive_triggers_from_server()` - Server-side trigger data handling

### client_audio.c (47 lines)
Audio system management:
- `init_audio()` - Audio subsystem initialization
- `play_next_track()` - Music playback control

### client_utils.c (259 lines)
Utility and helper functions:
- **Blocking System:**
  - `is_blocked()` - Check if user is blocked
  - `toggle_block()` - Toggle block status
  
- **Avatar Management:**
  - `reset_avatar_cache()` - Clear avatar cache
  
- **Chat System:**
  - `push_chat_line()` - Add message to chat history
  
- **Status & Colors:**
  - `get_status_color()` - Get color for player status
  - `get_color_from_code()` - Parse color codes (^1-^9)
  - `get_role_color()` - Get color for user role
  
- **UI Scaling:**
  - `scale_ui()` - Scale integer values
  - `scale_rect()` - Scale SDL rectangles
  
- **Text Rendering:**
  - `render_raw_text()` - Basic text rendering
  - `render_text_gradient()` - Gradient text rendering
  - `render_text()` - Rich text with colors and styles
  - `remove_last_utf8_char()` - UTF-8 string manipulation
  
- **Role System:**
  - `get_role_name()` - Get role display name
  
- **Map System:**
  - `switch_map()` - Map transition handling

### client_input.c (151 lines)
Input handling and text editing:
- `get_cursor_pos_from_click()` - Click-to-cursor position mapping
- `delete_selection()` - Delete selected text
- `handle_text_edit()` - Complete text editing logic:
  - UTF-8 text input
  - Copy/Paste (Ctrl+C/V)
  - Cut (Ctrl+X)
  - Select All (Ctrl+A)
  - Selection with Shift+arrows
  - Backspace handling

### client_sdl.c (4023 lines)
Main game client with:
- **Rendering Functions:**
  - UI components (menus, popups, overlays)
  - Game world (map, players, HUD)
  - Authentication screen
  - Settings menu
  - Debug overlays
  
- **Event Handling:**
  - Mouse clicks
  - Keyboard input
  - Touch input (mobile)
  - Window events
  
- **Game Loop:**
  - Initialization
  - Main loop
  - Rendering coordination
  - Network polling
  - Cleanup

## Building

The Makefile has been updated to compile all modules:

```makefile
CLIENT_SRC = client_sdl.c client_network.c client_config.c \
             client_audio.c client_utils.c client_input.c $(VULKAN_SRC)
```

Build command:
```bash
make client
```

## Key Principles

1. **Minimal Changes:** Code logic remains unchanged
2. **Shared State:** Global variables defined in client_sdl.c, declared extern in client.h
3. **Clean Dependencies:** Each module includes only client.h
4. **Modular Design:** Related functions grouped by functionality
5. **Maintainability:** Easier to navigate and modify specific features

## Benefits

- **Better Organization:** Related code grouped together
- **Easier Navigation:** Find specific functionality faster
- **Improved Maintainability:** Changes isolated to specific modules
- **Clearer Dependencies:** Explicit function interfaces
- **Team Collaboration:** Multiple developers can work on different modules

## Future Improvements

The client_sdl.c file (4023 lines) could be further split:
- `client_ui_render.c` - UI rendering functions
- `client_game_render.c` - Game world rendering
- `client_events.c` - Event handling and click handlers

However, these contain highly interconnected code with the main game loop, so the current organization provides a good balance between modularity and maintainability.
