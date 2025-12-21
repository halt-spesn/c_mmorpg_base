#ifndef LOCALIZATION_H
#define LOCALIZATION_H

// Supported languages
typedef enum {
    GAME_LANG_ENGLISH = 0,
    GAME_LANG_UKRAINIAN = 1,
    GAME_LANG_COUNT = 2
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
    // Chat and Friends
    STR_CHAT,
    STR_CLOSE,
    STR_FRIENDS,
    STR_FRIENDS_LIST,
    STR_ADD_FRIEND,
    STR_ADD_FRIEND_BY_ID,
    STR_MESSAGE,
    STR_REMOVE,
    STR_ONLINE,
    STR_LAST,
    // Friend requests
    STR_PENDING_REQUESTS,
    STR_NO_NEW_REQUESTS,
    STR_FRIEND_REQUEST_FROM,
    STR_ACCEPT,
    STR_DENY,
    STR_YES,
    STR_NO,
    STR_ADD,
    STR_CANCEL,
    // Warnings
    STR_MY_WARNINGS,
    STR_NO_WARNINGS,
    // Change nickname
    STR_NEW_NAME,
    STR_TYPE_CONFIRM,
    STR_CURRENT_PASSWORD,
    STR_SUBMIT,
    STR_SHOW,
    // Change password
    STR_NEW_PASSWORD,
    STR_CONFIRM_PASSWORD,
    STR_CHANGE,
    // Blocked players
    STR_BLOCKED_PLAYERS,
    STR_NO_HIDDEN_PLAYERS,
    STR_HIDE,
    STR_UNHIDE,
    // Profile/player actions
    STR_SANCTION,
    // Sanction popup
    STR_SANCTION_PLAYER,
    STR_WARN,
    STR_BAN,
    STR_REASON,
    STR_TIME_FORMAT,
    STR_THREE_WARNS_AUTO_BAN,
    STR_EXECUTE,
    // Server/Profile lists
    STR_SERVER_IP,
    STR_PORT,
    STR_SAVED_SERVERS,
    STR_SAVED_PROFILES,
    STR_SELECT_SERVER,
    STR_SELECT_PROFILE,
    STR_SAVE_CURRENT_IP,
    STR_SAVE_CURRENT,
    // Debug overlay
    STR_FPS,
    STR_PING,
    // Documentation
    STR_PROJECT_CONTRIBUTORS,
    STR_YOU_FOR_PLAYING,
    STR_MAIN_DEVELOPER,
    STR_AI_ASSISTANT,
    STR_MULTIPLAYER_TESTS,
    // Staff list
    STR_SERVER_STAFF_LIST,
    STR_LOADING,
    // Misc UI
    STR_X_CLOSE,
    STR_DEL,
    STR_PLUS_ID,
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
