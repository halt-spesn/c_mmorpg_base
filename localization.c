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
        // Chat and Friends
        "Chat",                             // STR_CHAT
        "Close",                            // STR_CLOSE
        "Friends",                          // STR_FRIENDS
        "Friends List",                     // STR_FRIENDS_LIST
        "+ Add Friend",                     // STR_ADD_FRIEND
        "Add Friend by ID",                 // STR_ADD_FRIEND_BY_ID
        "Message",                          // STR_MESSAGE
        "- Remove",                         // STR_REMOVE
        "Online",                           // STR_ONLINE
        "Last",                             // STR_LAST
        // Friend requests
        "Pending Requests",                 // STR_PENDING_REQUESTS
        "No new requests.",                 // STR_NO_NEW_REQUESTS
        "Friend Request from %s",           // STR_FRIEND_REQUEST_FROM
        "Accept",                           // STR_ACCEPT
        "Deny",                             // STR_DENY
        "Yes",                              // STR_YES
        "No",                               // STR_NO
        "Add",                              // STR_ADD
        "Cancel",                           // STR_CANCEL
        // Warnings
        "My Warnings",                      // STR_MY_WARNINGS
        "No warnings on record.",           // STR_NO_WARNINGS
        // Change nickname
        "New Name:",                        // STR_NEW_NAME
        "Type 'CONFIRM':",                  // STR_TYPE_CONFIRM
        "Current Password:",                // STR_CURRENT_PASSWORD
        "Submit",                           // STR_SUBMIT
        "Show",                             // STR_SHOW
        // Change password
        "New Password:",                    // STR_NEW_PASSWORD
        "Confirm Password:",                // STR_CONFIRM_PASSWORD
        "Change",                           // STR_CHANGE
        // Blocked players
        "Blocked Players",                  // STR_BLOCKED_PLAYERS
        "(No hidden players)",              // STR_NO_HIDDEN_PLAYERS
        "Hide",                             // STR_HIDE
        "Unhide",                           // STR_UNHIDE
        // Profile/player actions
        "SANCTION",                         // STR_SANCTION
        // Sanction popup
        "Sanction Player",                  // STR_SANCTION_PLAYER
        "WARN",                             // STR_WARN
        "BAN",                              // STR_BAN
        "Reason:",                          // STR_REASON
        "Time (1h, 1d, 1w):",               // STR_TIME_FORMAT
        "(3 Warns = Auto Ban)",             // STR_THREE_WARNS_AUTO_BAN
        "EXECUTE",                          // STR_EXECUTE
        // Server/Profile lists
        "Server IP:",                       // STR_SERVER_IP
        "Port:",                            // STR_PORT
        "Saved Servers",                    // STR_SAVED_SERVERS
        "Saved Profiles",                   // STR_SAVED_PROFILES
        "Select Server",                    // STR_SELECT_SERVER
        "Select Profile",                   // STR_SELECT_PROFILE
        "Save Current IP",                  // STR_SAVE_CURRENT_IP
        "Save Current",                     // STR_SAVE_CURRENT
        // Debug overlay
        "FPS: %d",                          // STR_FPS
        "Ping: %d ms",                      // STR_PING
        // Documentation
        "Project Contributors",             // STR_PROJECT_CONTRIBUTORS
        "You, for playing this game!",      // STR_YOU_FOR_PLAYING
        "#Main Developer#",                 // STR_MAIN_DEVELOPER
        "#AI Assistant#",                   // STR_AI_ASSISTANT
        "#Multiplayer Tests#",              // STR_MULTIPLAYER_TESTS
        // Staff list
        "Server Staff List",                // STR_SERVER_STAFF_LIST
        "Loading...",                       // STR_LOADING
        // Misc UI
        "X",                                // STR_X_CLOSE
        "Del",                              // STR_DEL
        "+ ID",                             // STR_PLUS_ID
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
        // Chat and Friends
        "Чат",                              // STR_CHAT
        "Закрити",                          // STR_CLOSE
        "Друзі",                            // STR_FRIENDS
        "Список друзів",                    // STR_FRIENDS_LIST
        "+ Додати друга",                   // STR_ADD_FRIEND
        "Додати друга за ID",               // STR_ADD_FRIEND_BY_ID
        "Повідомлення",                     // STR_MESSAGE
        "- Видалити",                       // STR_REMOVE
        "Онлайн",                           // STR_ONLINE
        "Останній",                         // STR_LAST
        // Friend requests
        "Запити в друзі",                   // STR_PENDING_REQUESTS
        "Немає нових запитів.",             // STR_NO_NEW_REQUESTS
        "Запит у друзі від %s",             // STR_FRIEND_REQUEST_FROM
        "Прийняти",                         // STR_ACCEPT
        "Відхилити",                        // STR_DENY
        "Так",                              // STR_YES
        "Ні",                               // STR_NO
        "Додати",                           // STR_ADD
        "Скасувати",                        // STR_CANCEL
        // Warnings
        "Мої попередження",                 // STR_MY_WARNINGS
        "Попереджень немає.",               // STR_NO_WARNINGS
        // Change nickname
        "Нове ім'я:",                       // STR_NEW_NAME
        "Введіть 'CONFIRM':",               // STR_TYPE_CONFIRM
        "Поточний пароль:",                 // STR_CURRENT_PASSWORD
        "Надіслати",                        // STR_SUBMIT
        "Показати",                         // STR_SHOW
        // Change password
        "Новий пароль:",                    // STR_NEW_PASSWORD
        "Підтвердити пароль:",              // STR_CONFIRM_PASSWORD
        "Змінити",                          // STR_CHANGE
        // Blocked players
        "Заблоковані гравці",               // STR_BLOCKED_PLAYERS
        "(Немає прихованих гравців)",       // STR_NO_HIDDEN_PLAYERS
        "Приховати",                        // STR_HIDE
        "Показати",                         // STR_UNHIDE
        // Profile/player actions
        "ПОКАРАТИ",                         // STR_SANCTION
        // Sanction popup
        "Покарати гравця",                  // STR_SANCTION_PLAYER
        "ПОПЕРЕДИТИ",                       // STR_WARN
        "ЗАБАНИТИ",                         // STR_BAN
        "Причина:",                         // STR_REASON
        "Час (1h, 1d, 1w):",                // STR_TIME_FORMAT
        "(3 попередження = Авто бан)",      // STR_THREE_WARNS_AUTO_BAN
        "ВИКОНАТИ",                         // STR_EXECUTE
        // Server/Profile lists
        "IP сервера:",                      // STR_SERVER_IP
        "Порт:",                            // STR_PORT
        "Збережені сервери",                // STR_SAVED_SERVERS
        "Збережені профілі",                // STR_SAVED_PROFILES
        "Вибрати сервер",                   // STR_SELECT_SERVER
        "Вибрати профіль",                  // STR_SELECT_PROFILE
        "Зберегти поточний IP",             // STR_SAVE_CURRENT_IP
        "Зберегти поточний",                // STR_SAVE_CURRENT
        // Debug overlay
        "FPS: %d",                          // STR_FPS
        "Пінг: %d мс",                      // STR_PING
        // Documentation
        "Учасники проекту",                 // STR_PROJECT_CONTRIBUTORS
        "Ви, за те що граєте!",             // STR_YOU_FOR_PLAYING
        "#Головний розробник#",             // STR_MAIN_DEVELOPER
        "#Штучний інтелект#",               // STR_AI_ASSISTANT
        "#Тести мультиплеєра#",             // STR_MULTIPLAYER_TESTS
        // Staff list
        "Список персоналу сервера",         // STR_SERVER_STAFF_LIST
        "Завантаження...",                  // STR_LOADING
        // Misc UI
        "X",                                // STR_X_CLOSE
        "Вид",                              // STR_DEL
        "+ ID",                             // STR_PLUS_ID
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
