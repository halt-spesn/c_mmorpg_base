# Localization System Documentation

## Overview
This C-based MMO game now supports multiple languages through a simple localization system. The system is designed to be easily extensible and supports language switching at runtime via the settings menu.

## Supported Languages
- English (default)
- Ukrainian (Українська)

## Files
- `localization.h` - Header file with language enums and function declarations
- `localization.c` - Implementation with translation tables

## How It Works

### Translation Storage
All UI text is stored in a 2D array indexed by:
1. Language (LANG_ENGLISH, LANG_UKRAINIAN, etc.)
2. String ID (STR_SETTINGS, STR_LOGIN, etc.)

### Language Selection
Users can cycle through available languages by clicking the "Language" button in the settings menu. The selected language is:
1. Applied immediately to all visible UI
2. Saved to the config file
3. Loaded automatically on next game start

### Configuration Storage
The language preference is stored in `config.txt` as the 18th parameter (index 17):
```
r g b r2 g2 b2 afk_min debug fps coords vol unread ui_scale zoom backend use_vk use_nv lang
```

## For Developers

### Adding New Strings
1. Add a new enum value to `StringID` in `localization.h`
2. Add translations for all languages in the `translations` array in `localization.c`
3. Update `STR_COUNT` to reflect the total number of strings
4. Use `get_string(STR_YOUR_NEW_STRING)` in your code

Example:
```c
// In localization.h
typedef enum {
    // ... existing strings ...
    STR_NEW_FEATURE,
    STR_COUNT
} StringID;

// In localization.c
static const char* translations[LANG_COUNT][STR_COUNT] = {
    // LANG_ENGLISH
    {
        // ... existing translations ...
        "New Feature",
    },
    // LANG_UKRAINIAN
    {
        // ... existing translations ...
        "Нова функція",
    }
};

// In your code
render_text(renderer, get_string(STR_NEW_FEATURE), x, y, color, center);
```

### Adding New Languages
1. Add a new enum value to `Language` in `localization.h`
2. Update `LANG_COUNT`
3. Add a new column to the `translations` array
4. Add the language name to the `language_names` array

Example for adding Spanish:
```c
// In localization.h
typedef enum {
    LANG_ENGLISH = 0,
    LANG_UKRAINIAN = 1,
    LANG_SPANISH = 2,
    LANG_COUNT = 3
} Language;

// In localization.c
static const char* language_names[LANG_COUNT] = {
    "English",
    "Українська",
    "Español"
};

// Add Spanish translations to the translations array
```

## API Reference

### Functions

#### `const char* get_string(StringID id)`
Retrieves the localized string for the current language.
- **Parameters**: `id` - The string identifier
- **Returns**: The localized string, or empty string if invalid

#### `const char* get_language_name(Language lang)`
Gets the native name of a language.
- **Parameters**: `lang` - The language identifier
- **Returns**: The language name (e.g., "English", "Українська")

#### `void set_language(Language lang)`
Sets the current active language.
- **Parameters**: `lang` - The language to activate

### Global Variables

#### `Language current_language`
The currently active language. Can be read to check current language or written to change it (though `set_language()` is preferred).

## Localized UI Elements

The following UI elements are currently localized:
- Settings menu title and all options
- Login screen (title, username, password, buttons)
- HUD buttons (Settings, Friends, etc.)
- Debug info toggles
- Name color settings
- Music volume
- Auto-AFK settings
- UI scale and game zoom
- Disconnect button
- Documentation, Staff, Credits buttons
- Warnings viewer
- Language selector itself

## Testing

A simple test program is available in `/tmp/test_localization.c` that verifies:
- Both languages load correctly
- Strings are properly translated
- Language cycling works
- No memory issues

Run with:
```bash
gcc -o test_localization test_localization.c localization.c -I.
./test_localization
```

## Future Improvements

Potential enhancements:
1. Add more languages (Spanish, French, German, etc.)
2. Support dynamic language packs loaded from external files
3. Add right-to-left language support
4. Implement plural forms handling
5. Add date/time formatting per locale
6. Support for formatted strings (e.g., "Score: %d")
