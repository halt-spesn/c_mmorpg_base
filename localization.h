#ifndef LOCALIZATION_H
#define LOCALIZATION_H

// Supported languages
typedef enum {
    LANG_ENGLISH = 0,
    LANG_UKRAINIAN = 1,
    LANG_COUNT = 2
} Language;

// String IDs for all UI text
typedef enum {
    STR_SETTINGS,
    STR_SHOW_DEBUG_INFO,
    STR_SHOW_FPS,
    STR_SHOW_COORDINATES,
    STR_SHOW_UNREAD_COUNTER,
    STR_USE_VULKAN,
    STR_USE_NVIDIA_GPU,
    STR_RESTART_REQUIRED,
    STR_MANAGE_BLOCKED_PLAYERS,
    STR_MY_ID,
    STR_STATUS,
    STR_CHANGE_NICKNAME,
    STR_CHANGE_AVATAR,
    STR_CHANGE_PASSWORD,
    STR_NAME_COLOR_START,
    STR_NAME_COLOR_END,
    STR_MUSIC_VOLUME,
    STR_AUTO_AFK,
    STR_UI_SCALE,
    STR_GAME_ZOOM,
    STR_DISCONNECT_LOGOUT,
    STR_DOCS,
    STR_STAFF,
    STR_CREDITS,
    STR_VIEW_MY_WARNINGS,
    STR_LOGIN,
    STR_REGISTER,
    STR_USERNAME,
    STR_PASSWORD,
    STR_C_MMO_LOGIN,
    STR_LANGUAGE,
    STR_COUNT
} StringID;

// Global current language
extern Language current_language;

// Get localized string
const char* get_string(StringID id);

// Get language name
const char* get_language_name(Language lang);

// Set current language
void set_language(Language lang);

#endif // LOCALIZATION_H
