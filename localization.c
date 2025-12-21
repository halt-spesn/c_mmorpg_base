#include "localization.h"
#include <string.h>

// Current language (default: English)
Language current_language = LANG_ENGLISH;

// Translation table
static const char* translations[LANG_COUNT][STR_COUNT] = {
    // LANG_ENGLISH
    {
        "Settings",                         // STR_SETTINGS
        "Show Debug Info",                  // STR_SHOW_DEBUG_INFO
        "Show FPS",                         // STR_SHOW_FPS
        "Show Coordinates",                 // STR_SHOW_COORDINATES
        "Show Unread Counter",              // STR_SHOW_UNREAD_COUNTER
        "Use Vulkan (restart required)",    // STR_USE_VULKAN
        "Use NVIDIA GPU (restart required)", // STR_USE_NVIDIA_GPU
        "(restart required)",               // STR_RESTART_REQUIRED
        "Manage Blocked Players",           // STR_MANAGE_BLOCKED_PLAYERS
        "My ID: %d",                        // STR_MY_ID
        "Status: %s",                       // STR_STATUS
        "Change Nickname",                  // STR_CHANGE_NICKNAME
        "Change Avatar",                    // STR_CHANGE_AVATAR
        "Change Password",                  // STR_CHANGE_PASSWORD
        "Name Color (Start)",               // STR_NAME_COLOR_START
        "Name Color (End)",                 // STR_NAME_COLOR_END
        "Music Volume",                     // STR_MUSIC_VOLUME
        "Auto-AFK: %d min",                 // STR_AUTO_AFK
        "UI Scale: %.1fx",                  // STR_UI_SCALE
        "Game Zoom: %.1fx",                 // STR_GAME_ZOOM
        "Disconnect / Logout",              // STR_DISCONNECT_LOGOUT
        "Docs",                             // STR_DOCS
        "Staff",                            // STR_STAFF
        "Credits",                          // STR_CREDITS
        "View My Warnings",                 // STR_VIEW_MY_WARNINGS
        "Login",                            // STR_LOGIN
        "Register",                         // STR_REGISTER
        "Username",                         // STR_USERNAME
        "Password",                         // STR_PASSWORD
        "C-MMO Login",                      // STR_C_MMO_LOGIN
        "Language",                         // STR_LANGUAGE
    },
    // LANG_UKRAINIAN
    {
        "Налаштування",                     // STR_SETTINGS
        "Показати інформацію налагодження", // STR_SHOW_DEBUG_INFO
        "Показати FPS",                     // STR_SHOW_FPS
        "Показати координати",              // STR_SHOW_COORDINATES
        "Показати лічильник непрочитаних",  // STR_SHOW_UNREAD_COUNTER
        "Використати Vulkan (потрібен перезапуск)", // STR_USE_VULKAN
        "Використати GPU NVIDIA (потрібен перезапуск)", // STR_USE_NVIDIA_GPU
        "(потрібен перезапуск)",            // STR_RESTART_REQUIRED
        "Керувати заблокованими гравцями",  // STR_MANAGE_BLOCKED_PLAYERS
        "Мій ID: %d",                       // STR_MY_ID
        "Статус: %s",                       // STR_STATUS
        "Змінити нікнейм",                  // STR_CHANGE_NICKNAME
        "Змінити аватар",                   // STR_CHANGE_AVATAR
        "Змінити пароль",                   // STR_CHANGE_PASSWORD
        "Колір імені (Початок)",            // STR_NAME_COLOR_START
        "Колір імені (Кінець)",             // STR_NAME_COLOR_END
        "Гучність музики",                  // STR_MUSIC_VOLUME
        "Авто-AFK: %d хв",                  // STR_AUTO_AFK
        "Масштаб UI: %.1fx",                // STR_UI_SCALE
        "Масштаб гри: %.1fx",               // STR_GAME_ZOOM
        "Відключитися / Вийти",             // STR_DISCONNECT_LOGOUT
        "Документація",                     // STR_DOCS
        "Персонал",                         // STR_STAFF
        "Автори",                           // STR_CREDITS
        "Переглянути мої попередження",     // STR_VIEW_MY_WARNINGS
        "Увійти",                           // STR_LOGIN
        "Зареєструватися",                  // STR_REGISTER
        "Ім'я користувача",                 // STR_USERNAME
        "Пароль",                           // STR_PASSWORD
        "C-MMO Вхід",                       // STR_C_MMO_LOGIN
        "Мова",                             // STR_LANGUAGE
    }
};

// Language names
static const char* language_names[LANG_COUNT] = {
    "English",
    "Українська"
};

const char* get_string(StringID id) {
    if (id >= STR_COUNT) return "";
    if (current_language >= LANG_COUNT) return "";
    return translations[current_language][id];
}

const char* get_language_name(Language lang) {
    if (lang >= LANG_COUNT) return "Unknown";
    return language_names[lang];
}

void set_language(Language lang) {
    if (lang < LANG_COUNT) {
        current_language = lang;
    }
}
