#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <psapi.h>
#include <direct.h>
#include <io.h>
#include <sys/stat.h>
#include <errno.h>
#define close closesocket
#define mkdir(path, mode) _mkdir(path)
#define strcasecmp _stricmp
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#endif
#include <pthread.h> // Requires winpthreads on Windows (provided by MinGW)
#include <sqlite3.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "common.h"
#include <time.h>
#include <stdarg.h>

// File logging
FILE *log_file = NULL;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// --- Server GUI State ---
#define GUI_LOG_LINES 20
#define GUI_LOG_MAX_LEN 128
#define MEMORY_HISTORY_COUNT 60
char gui_log_buffer[GUI_LOG_LINES][GUI_LOG_MAX_LEN];
int gui_log_head = 0;
float memory_history[MEMORY_HISTORY_COUNT];
int memory_history_head = 0;
time_t server_start_time;
pthread_t gui_thread_handle;
int gui_running = 1;

// Cross-platform memory usage helper (Returns MB)
float get_memory_usage() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize / (1024.0f * 1024.0f);
    }
#else
    FILE* fp = fopen("/proc/self/statm", "r");
    if (fp) {
        long pages;
        if (fscanf(fp, "%*s %ld", &pages) == 1) {
            fclose(fp);
            return (pages * sysconf(_SC_PAGESIZE)) / (1024.0f * 1024.0f);
        }
        fclose(fp);
    }
#endif
    return 0.0f;
}

// Get current timestamp string
void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "[%Y-%m-%d %H:%M:%S]", tm_info);
}

// Log with timestamp to both console and file
void log_message(const char *format, ...) {
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    char formatted_msg[256];
    va_list args;
    va_start(args, format);
    vsnprintf(formatted_msg, sizeof(formatted_msg), format, args);
    va_end(args);

    // Console output
    printf("%s %s", timestamp, formatted_msg);
    
    // GUI Buffer output
    pthread_mutex_lock(&log_mutex);
    snprintf(gui_log_buffer[gui_log_head], GUI_LOG_MAX_LEN, "%s", formatted_msg);
    gui_log_head = (gui_log_head + 1) % GUI_LOG_LINES;
    pthread_mutex_unlock(&log_mutex);
    
    // File output
    if (log_file) {
        pthread_mutex_lock(&log_mutex);
        fprintf(log_file, "%s %s", timestamp, formatted_msg);
        fflush(log_file);
        pthread_mutex_unlock(&log_mutex);
    }
}

#define LOG(...) log_message(__VA_ARGS__)
#include <stdarg.h>

#ifdef _WIN32
typedef SOCKET socket_t;
#define SOCKET_INVALID INVALID_SOCKET
#define SOCKET_IS_VALID(s) ((s) != SOCKET_INVALID)
#define usleep(x) Sleep((DWORD)(((x) + 999) / 1000))
#else
typedef int socket_t;
#define SOCKET_INVALID -1
#define SOCKET_IS_VALID(s) ((s) != SOCKET_INVALID)
#endif

Player players[MAX_CLIENTS];
socket_t client_sockets[MAX_CLIENTS];
sqlite3 *db;
int next_player_id = 100;

// Quest storage separate from Player struct (to keep broadcast_state packet small)
Quest player_quests[MAX_CLIENTS][10];  // Each player can have up to 10 active quests
int32_t player_quest_counts[MAX_CLIENTS];  // Number of quests each player has

// Triggers data
TriggerData server_triggers[20];
int server_trigger_count = 0;

// Ground items
GroundItem ground_items[MAX_GROUND_ITEMS];
int ground_item_count = 0;

// NPCs
NPC server_npcs[50];
int server_npc_count = 0;

// Enemies
Enemy server_enemies[50];
int server_enemy_count = 0;
int next_enemy_id = 1;

// Trade state per player
typedef struct {
    int partner_id;           // ID of trade partner (-1 if not trading)
    Item offered_items[10];   // Items offered
    int offered_count;        // Number of items offered
    int offered_gold;         // Gold offered
    int confirmed;            // 1 if player confirmed trade
} TradeState;
TradeState player_trades[MAX_CLIENTS];

pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

// --- Server GUI Core (SDL2) ---
void render_server_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y, SDL_Color color) {
    if (!text || text[0] == '\0') return;
    SDL_Surface *surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

void* gui_thread(void* arg) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        LOG("SDL_Init Error: %s\n", SDL_GetError());
        return NULL;
    }
    if (TTF_Init() < 0) {
        LOG("TTF_Init Error: %s\n", TTF_GetError());
        return NULL;
    }

    SDL_Window *window = SDL_CreateWindow("MMORPG Server Monitor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        LOG("SDL_CreateWindow Error: %s\n", SDL_GetError());
        return NULL;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    TTF_Font *font = TTF_OpenFont("DejaVuSans.ttf", 14);
    if (!font) {
        LOG("Warning: Could not load DejaVuSans.ttf for Server GUI\n");
    }

    SDL_Color col_white = {255, 255, 255, 255};
    SDL_Color col_green = {0, 255, 0, 255};
    SDL_Color col_gray = {150, 150, 150, 255};
    SDL_Color col_yellow = {255, 255, 0, 255};

    Uint32 last_update = 0;

    SDL_Event e;
    while (gui_running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                gui_running = 0;
            }
        }

        Uint32 now_ms = SDL_GetTicks();
        if (now_ms - last_update >= 1000) {
            float mb = get_memory_usage();
            memory_history[memory_history_head] = mb;
            memory_history_head = (memory_history_head + 1) % MEMORY_HISTORY_COUNT;
            last_update = now_ms;
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);

        int w, h;
        SDL_GetWindowSize(window, &w, &h);

        // 1. Header (Uptime)
        char info[256];
        time_t now_time = time(NULL);
        int uptime = (int)difftime(now_time, server_start_time);
        int active_players = 0;
        pthread_mutex_lock(&state_mutex);
        for(int i=0; i<MAX_CLIENTS; i++) if(players[i].active) active_players++;
        snprintf(info, sizeof(info), "Uptime: %02d:%02d:%02d | Active Players: %d | Memory: %.1f MB", uptime/3600, (uptime%3600)/60, uptime%60, active_players, memory_history[(memory_history_head + MEMORY_HISTORY_COUNT - 1) % MEMORY_HISTORY_COUNT]);
        if (font) render_server_text(renderer, font, info, 10, 10, col_white);
        
        // Boundaries
        int mid_x = w / 2;
        int mid_y = h / 2;
        int header_h = 40;

        // 2. Memory Graph (Top Left)
        SDL_Rect rect_graph = {10, header_h, mid_x - 15, mid_y - header_h - 10};
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_RenderDrawRect(renderer, &rect_graph);
        if (font) render_server_text(renderer, font, "Memory History (60s)", 15, header_h + 5, col_gray);
        
        SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
        float max_mem = 100.0f; // Default scale max
        for(int i=0; i<MEMORY_HISTORY_COUNT; i++) if(memory_history[i] > max_mem) max_mem = memory_history[i];
        
        for(int i=0; i<MEMORY_HISTORY_COUNT-1; i++) {
            int idx1 = (memory_history_head + i) % MEMORY_HISTORY_COUNT;
            int idx2 = (memory_history_head + i + 1) % MEMORY_HISTORY_COUNT;
            int x1 = rect_graph.x + (i * rect_graph.w / (MEMORY_HISTORY_COUNT-1));
            int x2 = rect_graph.x + ((i+1) * rect_graph.w / (MEMORY_HISTORY_COUNT-1));
            int y1 = rect_graph.y + rect_graph.h - (int)(memory_history[idx1] / max_mem * (rect_graph.h - 20)) - 10;
            int y2 = rect_graph.y + rect_graph.h - (int)(memory_history[idx2] / max_mem * (rect_graph.h - 20)) - 10;
            SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        }

        // 3. Player List (Top Right)
        SDL_Rect rect_players = {mid_x + 5, header_h, w - mid_x - 15, mid_y - header_h - 10};
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_RenderDrawRect(renderer, &rect_players);
        if (font) render_server_text(renderer, font, "Connected Players:", mid_x + 10, header_h + 5, col_gray);
        int py = header_h + 25;
        for(int i=0; i<MAX_CLIENTS; i++) {
            if(players[i].active) {
                char pinfo[64]; snprintf(pinfo, 64, "- %s (ID: %d)", players[i].username, players[i].id);
                if (font) render_server_text(renderer, font, pinfo, mid_x + 15, py, col_white);
                py += 18;
                if (py > rect_players.y + rect_players.h - 20) break;
            }
        }
        pthread_mutex_unlock(&state_mutex);

        // 4. Logs (Bottom Full Width)
        SDL_Rect rect_logs = {10, mid_y, w - 20, h - mid_y - 10};
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_RenderDrawRect(renderer, &rect_logs);
        if (font) render_server_text(renderer, font, "Recent Logs:", 15, mid_y + 5, col_yellow);
        int ly = mid_y + 25;
        pthread_mutex_lock(&log_mutex);
        for(int i=0; i<GUI_LOG_LINES; i++) {
            int idx = (gui_log_head + i) % GUI_LOG_LINES;
            if(gui_log_buffer[idx][0] != '\0') {
                if (font) render_server_text(renderer, font, gui_log_buffer[idx], 15, ly, col_white);
                ly += 16;
                if (ly > rect_logs.y + rect_logs.h - 20) break;
            }
        }
        pthread_mutex_unlock(&log_mutex);

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60fps
    }

    if (font) TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    LOG("Server GUI closed, shutting down...\n");
    exit(0);
    return NULL;
}

// --- Prototypes ---
void broadcast_state();
int send_all(socket_t sockfd, void *buf, size_t len, int flags);
void update_collect_quest_progress(int player_index, int item_id, int quantity);
void update_kill_quest_progress(int player_index, int enemy_type);
void update_quest_progress(int player_index, int npc_id);

// --- Database ---
int get_role_id_from_name(char *name) {
    if (strcasecmp(name, "Admin") == 0) return ROLE_ADMIN;
    if (strcasecmp(name, "Dev") == 0) return ROLE_DEVELOPER;
    if (strcasecmp(name, "Contrib") == 0) return ROLE_CONTRIBUTOR;
    if (strcasecmp(name, "VIP") == 0) return ROLE_VIP;
    return -1; 
}

void init_db() {
    // Close any existing connection first
    if (db) {
        sqlite3_close(db);
        db = NULL;
    }
    
    int rc = sqlite3_open("mmorpg.db", &db);
    if (rc) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        if (db) sqlite3_close(db);
        exit(1);
    }
    
    // Use DELETE journal mode (cross-platform, avoids file lock issues)
    char *err_msg = NULL;
    rc = sqlite3_exec(db, "PRAGMA journal_mode=DELETE;", 0, 0, &err_msg);
    if (rc != SQLITE_OK && err_msg) {
        fprintf(stderr, "Failed to set journal mode: %s\n", err_msg);
        sqlite3_free(err_msg);
    }
    sqlite3_exec(db, "PRAGMA synchronous=FULL;", 0, 0, 0);  // FULL for safety
    sqlite3_exec(db, "PRAGMA locking_mode=NORMAL;", 0, 0, 0);
    sqlite3_exec(db, "PRAGMA temp_store=MEMORY;", 0, 0, 0);
    LOG("Database opened successfully\n");
 char *sql_users = "CREATE TABLE IF NOT EXISTS users("
                "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
                "USERNAME TEXT NOT NULL UNIQUE,"
                "PASSWORD TEXT NOT NULL,"
                "X REAL NOT NULL,"
                "Y REAL NOT NULL,"
                "R INTEGER DEFAULT 255,"
                "G INTEGER DEFAULT 255,"
                "B INTEGER DEFAULT 0,"
                "ROLE INTEGER DEFAULT 0,"
                "LAST_LOGIN TEXT DEFAULT 'Never',"
                "LAST_NICK_CHANGE INTEGER DEFAULT 0);"; // NEW COLUMN (Unix Timestamp)
    sqlite3_exec(db, sql_users, 0, 0, 0);
    char *sql_friends = "CREATE TABLE IF NOT EXISTS friends(USER_ID INT, FRIEND_ID INT, STATUS INT, PRIMARY KEY (USER_ID, FRIEND_ID));";
    sqlite3_exec(db, sql_friends, 0, 0, 0);
    char *alter1 = "ALTER TABLE users ADD COLUMN WARN_COUNT INTEGER DEFAULT 0;";
    sqlite3_exec(db, alter1, 0, 0, 0);
    char *alter2 = "ALTER TABLE users ADD COLUMN BAN_EXPIRE INTEGER DEFAULT 0;";
    sqlite3_exec(db, alter2, 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN R2 INTEGER DEFAULT 255;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN G2 INTEGER DEFAULT 255;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN B2 INTEGER DEFAULT 255;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN MAP TEXT DEFAULT 'map.jpg';", 0, 0, 0);

    // 2. Create Warnings Table
    char *sql_warn = 
        "CREATE TABLE IF NOT EXISTS warnings ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "USER_ID INTEGER, "
        "REASON TEXT, "
        "TIMESTAMP INTEGER);";
    sqlite3_exec(db, sql_warn, 0, 0, 0);

    // 3. Create Items Table
    char *sql_items = 
        "CREATE TABLE IF NOT EXISTS items ("
        "ITEM_ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "NAME TEXT NOT NULL, "
        "TYPE INTEGER NOT NULL, "
        "ICON TEXT DEFAULT 'item.png', "
        "MAX_STACK INTEGER DEFAULT 1, "
        "DESCRIPTION TEXT);";
    sqlite3_exec(db, sql_items, 0, 0, 0);

    // 4. Create Player Inventories Table
    // Simply drop and recreate if it exists with old schema
    sqlite3_exec(db, "DROP TABLE IF EXISTS player_inventory_old;", 0, 0, 0);
    
    char *sql_inventory = 
        "CREATE TABLE IF NOT EXISTS player_inventory ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "USER_ID INTEGER, "
        "ITEM_ID INTEGER, "
        "QUANTITY INTEGER DEFAULT 1, "
        "SLOT_INDEX INTEGER, "
        "IS_EQUIPPED INTEGER DEFAULT 0);";
    sqlite3_exec(db, sql_inventory, 0, 0, 0);
    LOG("Player inventory table ready\n");

    // 5. Create Ground Items Table
    char *sql_ground = 
        "CREATE TABLE IF NOT EXISTS ground_items ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "ITEM_ID INTEGER, "
        "X REAL, "
        "Y REAL, "
        "MAP_NAME TEXT, "
        "QUANTITY INTEGER, "
        "SPAWN_TIME INTEGER);";
    sqlite3_exec(db, sql_ground, 0, 0, 0);

    // 6. Create NPCs Table
    char *sql_npcs =
        "CREATE TABLE IF NOT EXISTS npcs ("
        "NPC_ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "NAME TEXT NOT NULL, "
        "X REAL NOT NULL, "
        "Y REAL NOT NULL, "
        "MAP_NAME TEXT DEFAULT 'map.jpg', "
        "NPC_TYPE INTEGER DEFAULT 0, "  // 0=quest, 1=merchant, 2=generic
        "DIALOGUE_ID INTEGER, "
        "ICON TEXT DEFAULT 'npc.png');";
    sqlite3_exec(db, sql_npcs, 0, 0, 0);

    // 7. Create Dialogues Table
    char *sql_dialogues =
        "CREATE TABLE IF NOT EXISTS dialogues ("
        "DIALOGUE_ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "NPC_ID INTEGER, "
        "TEXT TEXT NOT NULL, "
        "NEXT_DIALOGUE_ID INTEGER);";  // For dialogue chains
    sqlite3_exec(db, sql_dialogues, 0, 0, 0);

    // 8. Create Quests Table
    char *sql_quests =
        "CREATE TABLE IF NOT EXISTS quests ("
        "QUEST_ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "NAME TEXT NOT NULL, "
        "DESCRIPTION TEXT, "
        "NPC_ID INTEGER, "  // Quest giver
        "REWARD_GOLD INTEGER DEFAULT 0, "
        "REWARD_XP INTEGER DEFAULT 0, "
        "REWARD_ITEM_ID INTEGER, "
        "REWARD_ITEM_QTY INTEGER DEFAULT 1, "
        "LEVEL_REQUIRED INTEGER DEFAULT 1);";
    sqlite3_exec(db, sql_quests, 0, 0, 0);

    // 9. Create Quest Objectives Table
    char *sql_objectives =
        "CREATE TABLE IF NOT EXISTS quest_objectives ("
        "OBJECTIVE_ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "QUEST_ID INTEGER, "
        "OBJECTIVE_TYPE INTEGER, "  // 0=kill, 1=collect, 2=visit
        "TARGET_ID INTEGER, "  // NPC/Item/Location ID
        "TARGET_NAME TEXT, "
        "REQUIRED_COUNT INTEGER DEFAULT 1);";
    sqlite3_exec(db, sql_objectives, 0, 0, 0);

    // 10. Create Player Quests Table
    char *sql_player_quests =
        "CREATE TABLE IF NOT EXISTS player_quests ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "USER_ID INTEGER, "
        "QUEST_ID INTEGER, "
        "STATUS INTEGER DEFAULT 0, "  // 0=active, 1=completed, 2=failed
        "PROGRESS TEXT, "  // JSON or CSV of objective progress
        "ACCEPTED_TIME INTEGER, "
        "COMPLETED_TIME INTEGER);";
    sqlite3_exec(db, sql_player_quests, 0, 0, 0);

    // 11. Create Shop Items Table
    char *sql_shops =
        "CREATE TABLE IF NOT EXISTS shop_items ("
        "SHOP_ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "NPC_ID INTEGER, "
        "ITEM_ID INTEGER, "
        "BUY_PRICE INTEGER, "
        "SELL_PRICE INTEGER, "
        "STOCK INTEGER DEFAULT -1);";  // -1 = infinite
    sqlite3_exec(db, sql_shops, 0, 0, 0);

    // 12. Add currency column to users table
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN GOLD INTEGER DEFAULT 100;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN XP INTEGER DEFAULT 0;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN LEVEL INTEGER DEFAULT 1;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN STR INTEGER DEFAULT 10;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN AGI INTEGER DEFAULT 10;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN INT INTEGER DEFAULT 10;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN SKILL_POINTS INTEGER DEFAULT 0;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN PVP_ENABLED INTEGER DEFAULT 0;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN PARTY_ID INTEGER DEFAULT -1;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN CLAN_ID INTEGER DEFAULT -1;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN CLAN_ROLE INTEGER DEFAULT 0;", 0, 0, 0);

    // 13. Create Clan System Tables
    char *sql_clans = 
        "CREATE TABLE IF NOT EXISTS clans ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "NAME TEXT NOT NULL UNIQUE, "
        "OWNER_ID INTEGER, "
        "GOLD_STORAGE INTEGER DEFAULT 0, "
        "CREATED_AT INTEGER);";
    sqlite3_exec(db, sql_clans, 0, 0, 0);

    char *sql_clan_storage = 
        "CREATE TABLE IF NOT EXISTS clan_storage_items ("
        "CLAN_ID INTEGER, "
        "SLOT_INDEX INTEGER, "
        "ITEM_ID INTEGER, "
        "QUANTITY INTEGER DEFAULT 1, "
        "PRIMARY KEY (CLAN_ID, SLOT_INDEX));";
    sqlite3_exec(db, sql_clan_storage, 0, 0, 0);

    // Insert some default items if table is empty
    sqlite3_stmt *check_stmt;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM items;", -1, &check_stmt, 0) == SQLITE_OK) {
        if (sqlite3_step(check_stmt) == SQLITE_ROW) {
            int count = sqlite3_column_int(check_stmt, 0);
            sqlite3_finalize(check_stmt);
            
            if (count == 0) {
                // Add starter items
                sqlite3_exec(db, "INSERT INTO items (NAME, TYPE, ICON, MAX_STACK) VALUES ('Health Potion', 0, 'potion.png', 10);", 0, 0, 0);
                sqlite3_exec(db, "INSERT INTO items (NAME, TYPE, ICON, MAX_STACK) VALUES ('Iron Sword', 1, 'sword.png', 1);", 0, 0, 0);
                sqlite3_exec(db, "INSERT INTO items (NAME, TYPE, ICON, MAX_STACK) VALUES ('Leather Armor', 2, 'armor.png', 1);", 0, 0, 0);
                sqlite3_exec(db, "INSERT INTO items (NAME, TYPE, ICON, MAX_STACK) VALUES ('Iron Helmet', 2, 'helmet.png', 1);", 0, 0, 0);
                sqlite3_exec(db, "INSERT INTO items (NAME, TYPE, ICON, MAX_STACK) VALUES ('Leather Boots', 2, 'boots.png', 1);", 0, 0, 0);
                sqlite3_exec(db, "INSERT INTO items (NAME, TYPE, ICON, MAX_STACK) VALUES ('Magic Ring', 3, 'ring.png', 1);", 0, 0, 0);
                sqlite3_exec(db, "INSERT INTO items (NAME, TYPE, ICON, MAX_STACK) VALUES ('Wood', 4, 'wood.png', 50);", 0, 0, 0);
                sqlite3_exec(db, "INSERT INTO items (NAME, TYPE, ICON, MAX_STACK) VALUES ('Stone', 4, 'stone.png', 50);", 0, 0, 0);
                sqlite3_exec(db, "INSERT INTO items (NAME, TYPE, ICON, MAX_STACK) VALUES ('Iron Leggings', 2, 'legs.png', 1);", 0, 0, 0);
                LOG("Created default items\n");
            } else {
                // Check if item 9 exists, if not add it (for existing databases)
                sqlite3_stmt *check_item9;
                if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM items WHERE ITEM_ID = 9;", -1, &check_item9, NULL) == SQLITE_OK) {
                    if (sqlite3_step(check_item9) == SQLITE_ROW && sqlite3_column_int(check_item9, 0) == 0) {
                        sqlite3_exec(db, "INSERT INTO items (ITEM_ID, NAME, TYPE, ICON, MAX_STACK) VALUES (9, 'Iron Leggings', 2, 'legs.png', 1);", 0, 0, 0);
                        LOG("Added Iron Leggings to existing database\n");
                    }
                    sqlite3_finalize(check_item9);
                }
            }
        } else {
            sqlite3_finalize(check_stmt);
        }
    }
    
    // Insert default dialogues if empty
    sqlite3_stmt *check_dlg;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM dialogues;", -1, &check_dlg, NULL) == SQLITE_OK) {
        if (sqlite3_step(check_dlg) == SQLITE_ROW && sqlite3_column_int(check_dlg, 0) == 0) {
            sqlite3_exec(db, "INSERT INTO dialogues (NPC_ID, TEXT) VALUES (1, 'Welcome to my shop! I have the finest goods in town. Come back when you are ready to trade!');", 0, 0, 0);
            sqlite3_exec(db, "INSERT INTO dialogues (NPC_ID, TEXT) VALUES (2, 'Greetings, adventurer! I have a quest for you. There are dangerous creatures in the forest that need to be dealt with. Will you help?');", 0, 0, 0);
            sqlite3_exec(db, "INSERT INTO dialogues (NPC_ID, TEXT) VALUES (3, 'Need your weapons sharpened? Or perhaps some armor repaired? I am the best blacksmith in the land!');", 0, 0, 0);
            LOG("Created default dialogues\n");
        }
        sqlite3_finalize(check_dlg);
    }
    
    // Insert default shop items if empty
    sqlite3_stmt *check_shop;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM shop_items;", -1, &check_shop, NULL) == SQLITE_OK) {
        if (sqlite3_step(check_shop) == SQLITE_ROW && sqlite3_column_int(check_shop, 0) == 0) {
            // Blacksmith John (NPC 2) sells weapons and armor
            sqlite3_exec(db, "INSERT INTO shop_items (NPC_ID, ITEM_ID, BUY_PRICE, SELL_PRICE) VALUES (2, 2, 100, 50);", 0, 0, 0); // Iron Sword
            sqlite3_exec(db, "INSERT INTO shop_items (NPC_ID, ITEM_ID, BUY_PRICE, SELL_PRICE) VALUES (2, 3, 150, 75);", 0, 0, 0); // Leather Armor
            sqlite3_exec(db, "INSERT INTO shop_items (NPC_ID, ITEM_ID, BUY_PRICE, SELL_PRICE) VALUES (2, 4, 80, 40);", 0, 0, 0);  // Iron Helmet
            sqlite3_exec(db, "INSERT INTO shop_items (NPC_ID, ITEM_ID, BUY_PRICE, SELL_PRICE) VALUES (2, 5, 60, 30);", 0, 0, 0);  // Leather Boots
            sqlite3_exec(db, "INSERT INTO shop_items (NPC_ID, ITEM_ID, BUY_PRICE, SELL_PRICE) VALUES (2, 9, 120, 60);", 0, 0, 0); // Iron Leggings
            sqlite3_exec(db, "INSERT INTO shop_items (NPC_ID, ITEM_ID, BUY_PRICE, SELL_PRICE) VALUES (2, 6, 200, 100);", 0, 0, 0); // Magic Ring
            
            // General Store (NPC 3) sells basic items
            sqlite3_exec(db, "INSERT INTO shop_items (NPC_ID, ITEM_ID, BUY_PRICE, SELL_PRICE) VALUES (3, 1, 20, 10);", 0, 0, 0); // Health Potion
            sqlite3_exec(db, "INSERT INTO shop_items (NPC_ID, ITEM_ID, BUY_PRICE, SELL_PRICE) VALUES (3, 7, 5, 2);", 0, 0, 0);  // Wood
            sqlite3_exec(db, "INSERT INTO shop_items (NPC_ID, ITEM_ID, BUY_PRICE, SELL_PRICE) VALUES (3, 8, 10, 5);", 0, 0, 0); // Stone
            
            LOG("Created default shop items\n");
        }
        sqlite3_finalize(check_shop);
    }
    
    // Insert default quests if empty
    sqlite3_stmt *check_quests;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM quests;", -1, &check_quests, NULL) == SQLITE_OK) {
        if (sqlite3_step(check_quests) == SQLITE_ROW && sqlite3_column_int(check_quests, 0) == 0) {
            // Quest 1: Rat Problem (Kill quest) - offered by Quest Giver Sarah (NPC 1)
            sqlite3_exec(db, "INSERT INTO quests (QUEST_ID, NAME, DESCRIPTION, NPC_ID, REWARD_GOLD, REWARD_XP, REWARD_ITEM_ID, REWARD_ITEM_QTY, LEVEL_REQUIRED) "
                            "VALUES (1, 'Rat Problem', 'The town cellar is infested with rats. Kill 5 rats to help clean up the town.', 1, 50, 100, 1, 2, 1);", 0, 0, 0);
            sqlite3_exec(db, "INSERT INTO quest_objectives (QUEST_ID, OBJECTIVE_TYPE, TARGET_ID, TARGET_NAME, REQUIRED_COUNT) "
                            "VALUES (1, 0, 1, 'Rat', 5);", 0, 0, 0);
            
            // Quest 2: Gather Resources (Collect quest) - offered by Quest Giver Sarah (NPC 1)
            sqlite3_exec(db, "INSERT INTO quests (QUEST_ID, NAME, DESCRIPTION, NPC_ID, REWARD_GOLD, REWARD_XP, REWARD_ITEM_ID, REWARD_ITEM_QTY, LEVEL_REQUIRED) "
                            "VALUES (2, 'Gather Resources', 'I need materials for repairs. Bring me 10 Wood and 5 Stone.', 1, 75, 150, 2, 1, 1);", 0, 0, 0);
            sqlite3_exec(db, "INSERT INTO quest_objectives (QUEST_ID, OBJECTIVE_TYPE, TARGET_ID, TARGET_NAME, REQUIRED_COUNT) "
                            "VALUES (2, 1, 7, 'Wood', 10);", 0, 0, 0);
            sqlite3_exec(db, "INSERT INTO quest_objectives (QUEST_ID, OBJECTIVE_TYPE, TARGET_ID, TARGET_NAME, REQUIRED_COUNT) "
                            "VALUES (2, 1, 8, 'Stone', 5);", 0, 0, 0);
            
            // Quest 3: Meet the Blacksmith (Visit quest) - offered by Quest Giver Sarah (NPC 1)
            sqlite3_exec(db, "INSERT INTO quests (QUEST_ID, NAME, DESCRIPTION, NPC_ID, REWARD_GOLD, REWARD_XP, REWARD_ITEM_ID, REWARD_ITEM_QTY, LEVEL_REQUIRED) "
                            "VALUES (3, 'Meet the Blacksmith', 'Go speak with Blacksmith John. He may have work for you.', 1, 25, 50, 0, 0, 1);", 0, 0, 0);
            sqlite3_exec(db, "INSERT INTO quest_objectives (QUEST_ID, OBJECTIVE_TYPE, TARGET_ID, TARGET_NAME, REQUIRED_COUNT) "
                            "VALUES (3, 2, 2, 'Blacksmith John', 1);", 0, 0, 0);
            
            LOG("Created default quests\n");
        }
        sqlite3_finalize(check_quests);
    }
    
    LOG("Database initialization complete\n");
}

long parse_ban_duration(const char* str) {
    int val = atoi(str);
    char unit = str[strlen(str)-1];
    long multiplier = 1;
    
    switch(unit) {
        case 's': multiplier = 1; break;
        case 'm': multiplier = 60; break;
        case 'h': multiplier = 3600; break;
        case 'd': multiplier = 86400; break;
        case 'w': multiplier = 604800; break;
        case 'y': multiplier = 31536000; break;
        default: multiplier = 60; break; // Default to minutes if no unit
    }
    return val * multiplier;
}

void init_storage() {
    struct stat st = {0};
    if (stat("avatars", &st) == -1) mkdir("avatars", 0700);
}

int get_user_id(const char* username) {
    sqlite3_stmt *stmt; char sql[256];
    snprintf(sql, 256, "SELECT ID FROM users WHERE USERNAME=?;");
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) id = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt); return id;
}



AuthStatus register_user(const char *user, const char *pass) {
    char sql[256]; int count = 0; sqlite3_stmt *stmt;
    snprintf(sql, 256, "SELECT ID FROM users WHERE USERNAME=?;");
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0); sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) count = 1; sqlite3_finalize(stmt);
    if (count > 0) return AUTH_REGISTER_FAILED_EXISTS;
    snprintf(sql, 256, "INSERT INTO users (USERNAME, PASSWORD, X, Y, R, G, B) VALUES ('%s', '%s', 100.0, 100.0, 255, 255, 0);", user, pass);
    sqlite3_exec(db, sql, 0, 0, 0);
    
    // Give starter items to new player
    int user_id = get_user_id(user);
    if (user_id > 0) {
        // Give 3 health potions and 1 sword
        sqlite3_exec(db, "INSERT INTO player_inventory (USER_ID, ITEM_ID, QUANTITY, SLOT_INDEX, IS_EQUIPPED) VALUES (last_insert_rowid(), 1, 3, 0, 0);", 0, 0, 0);
        sqlite3_exec(db, "INSERT INTO player_inventory (USER_ID, ITEM_ID, QUANTITY, SLOT_INDEX, IS_EQUIPPED) VALUES (last_insert_rowid(), 2, 1, 1, 0);", 0, 0, 0);
        LOG("Gave starter items to new user %s\n", user);
    }
    
    return AUTH_REGISTER_SUCCESS;
}

// Update signature to accept 'long *ban_expire' and currency/level
int login_user(const char *username, const char *password, float *x, float *y, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *r2, uint8_t *g2, uint8_t *b2, int *role, long *ban_expire, char *map_name_out, int *gold, int *xp, int *level, int *str, int *agi, int *intel, int *skill_points, int *pvp_enabled, int *party_id, int *clan_id, int *clan_role) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT X, Y, R, G, B, ROLE, BAN_EXPIRE, R2, G2, B2, MAP, GOLD, XP, LEVEL, STR, AGI, INT, SKILL_POINTS, PVP_ENABLED, PARTY_ID, CLAN_ID, CLAN_ROLE FROM users WHERE USERNAME=? AND PASSWORD=?;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return 0;
    
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);
    
    int success = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *x = (float)sqlite3_column_double(stmt, 0);
        *y = (float)sqlite3_column_double(stmt, 1);
        *r = (uint8_t)sqlite3_column_int(stmt, 2);
        *g = (uint8_t)sqlite3_column_int(stmt, 3);
        *b = (uint8_t)sqlite3_column_int(stmt, 4);
        *role = sqlite3_column_int(stmt, 5);
        *ban_expire = (long)sqlite3_column_int64(stmt, 6); // Fetch Ban Time
        *r2 = (uint8_t)sqlite3_column_int(stmt, 7); // Index 7
        *g2 = (uint8_t)sqlite3_column_int(stmt, 8); // Index 8
        *b2 = (uint8_t)sqlite3_column_int(stmt, 9); // Index 9
        const unsigned char *m = sqlite3_column_text(stmt, 10); // Index 10
        if (m) strncpy(map_name_out, (const char*)m, 31);
        else strcpy(map_name_out, "map.jpg");
        *gold = sqlite3_column_int(stmt, 11);
        *xp = sqlite3_column_int(stmt, 12);
        *level = sqlite3_column_int(stmt, 13);
        *str = sqlite3_column_int(stmt, 14);
        *agi = sqlite3_column_int(stmt, 15);
        *intel = sqlite3_column_int(stmt, 16);
        *skill_points = sqlite3_column_int(stmt, 17);
        *pvp_enabled = sqlite3_column_int(stmt, 18);
        *party_id = sqlite3_column_int(stmt, 19);
        *clan_id = sqlite3_column_int(stmt, 20);
        *clan_role = sqlite3_column_int(stmt, 21);
        success = 1;
    }
    sqlite3_finalize(stmt);
    return success;
}

void save_player_location(const char *user, float x, float y) {
    char sql[256]; snprintf(sql, 256, "UPDATE users SET X=%f, Y=%f WHERE USERNAME='%s';", x, y, user); sqlite3_exec(db, sql, 0, 0, NULL);
}

// --- Inventory Management Functions ---

void load_player_inventory(int player_index) {
    Player *p = &players[player_index];
    p->inventory_count = 0;
    
    // Clear inventory
    memset(p->inventory, 0, sizeof(p->inventory));
    memset(p->equipment, 0, sizeof(p->equipment));
    
    sqlite3_stmt *stmt;
    const char *sql = "SELECT pi.ITEM_ID, pi.QUANTITY, pi.SLOT_INDEX, pi.IS_EQUIPPED, "
                      "i.NAME, i.TYPE, i.ICON FROM player_inventory pi "
                      "JOIN items i ON pi.ITEM_ID = i.ITEM_ID "
                      "WHERE pi.USER_ID = ? ORDER BY pi.SLOT_INDEX;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, p->id);
        
        int inv_idx = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW && inv_idx < MAX_INVENTORY_SLOTS) {
            int item_id = sqlite3_column_int(stmt, 0);
            int quantity = sqlite3_column_int(stmt, 1);
            int slot_index = sqlite3_column_int(stmt, 2);
            int is_equipped = sqlite3_column_int(stmt, 3);
            const char *name = (const char*)sqlite3_column_text(stmt, 4);
            int type = sqlite3_column_int(stmt, 5);
            const char *icon = (const char*)sqlite3_column_text(stmt, 6);
            
            if (is_equipped) {
                // Load into equipment slot
                if (slot_index >= 0 && slot_index < EQUIP_SLOT_COUNT) {
                    p->equipment[slot_index].item_id = item_id;
                    p->equipment[slot_index].quantity = quantity;
                    p->equipment[slot_index].type = type;
                    strncpy(p->equipment[slot_index].name, name, 31);
                    strncpy(p->equipment[slot_index].icon, icon, 15);
                }
            } else {
                // Load into inventory
                p->inventory[inv_idx].item.item_id = item_id;
                p->inventory[inv_idx].item.quantity = quantity;
                p->inventory[inv_idx].item.type = type;
                strncpy(p->inventory[inv_idx].item.name, name, 31);
                strncpy(p->inventory[inv_idx].item.icon, icon, 15);
                p->inventory[inv_idx].slot_index = slot_index;
                p->inventory[inv_idx].is_equipped = 0;
                inv_idx++;
            }
        }
        p->inventory_count = inv_idx;
        sqlite3_finalize(stmt);
        LOG("Loaded %d items for player %s\n", inv_idx, p->username);
    }
}

void save_player_inventory(int player_index) {
    Player *p = &players[player_index];
    
    // Delete existing inventory
    char sql[256];
    snprintf(sql, 256, "DELETE FROM player_inventory WHERE USER_ID = %d;", p->id);
    sqlite3_exec(db, sql, 0, 0, 0);
    
    // Save inventory items
    for (int i = 0; i < p->inventory_count; i++) {
        if (p->inventory[i].item.item_id > 0) {
            snprintf(sql, 256, "INSERT INTO player_inventory (USER_ID, ITEM_ID, QUANTITY, SLOT_INDEX, IS_EQUIPPED) "
                              "VALUES (%d, %d, %d, %d, 0);",
                     p->id, p->inventory[i].item.item_id, p->inventory[i].item.quantity, i);
            sqlite3_exec(db, sql, 0, 0, 0);
        }
    }
    
    // Save equipped items
    for (int i = 0; i < EQUIP_SLOT_COUNT; i++) {
        if (p->equipment[i].item_id > 0) {
            snprintf(sql, 256, "INSERT INTO player_inventory (USER_ID, ITEM_ID, QUANTITY, SLOT_INDEX, IS_EQUIPPED) "
                              "VALUES (%d, %d, %d, %d, 1);",
                     p->id, p->equipment[i].item_id, p->equipment[i].quantity, i);
            sqlite3_exec(db, sql, 0, 0, 0);
        }
    }
}

int give_item_to_player(int player_index, int item_id, int quantity) {
    Player *p = &players[player_index];
    int original_quantity = quantity;  // Track original amount for quest progress
    
    // Get item info from database
    sqlite3_stmt *stmt;
    const char *sql = "SELECT NAME, TYPE, ICON, MAX_STACK FROM items WHERE ITEM_ID = ?;";
    char name[32] = "";
    char icon[16] = "";
    int type = 0;
    int max_stack = 1;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, item_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            strncpy(name, (const char*)sqlite3_column_text(stmt, 0), 31);
            type = sqlite3_column_int(stmt, 1);
            strncpy(icon, (const char*)sqlite3_column_text(stmt, 2), 15);
            max_stack = sqlite3_column_int(stmt, 3);
        } else {
            sqlite3_finalize(stmt);
            LOG("Item ID %d not found in database\n", item_id);
            return 0; // Item not found
        }
        sqlite3_finalize(stmt);
    } else {
        LOG("Failed to query items table\n");
        return 0;
    }
    
    if (name[0] == '\0') return 0; // Item doesn't exist
    
    // Try to stack with existing items
    for (int i = 0; i < p->inventory_count; i++) {
        if (p->inventory[i].item.item_id == item_id && 
            p->inventory[i].item.quantity < max_stack) {
            int space = max_stack - p->inventory[i].item.quantity;
            int to_add = (quantity < space) ? quantity : space;
            p->inventory[i].item.quantity += to_add;
            quantity -= to_add;
            if (quantity == 0) {
                save_player_inventory(player_index);
                // Update quest progress for stacked items
                update_collect_quest_progress(player_index, item_id, original_quantity);
                return 1;
            }
        }
    }
    
    // Add to new slots
    while (quantity > 0 && p->inventory_count < MAX_INVENTORY_SLOTS) {
        int to_add = (quantity < max_stack) ? quantity : max_stack;
        p->inventory[p->inventory_count].item.item_id = item_id;
        p->inventory[p->inventory_count].item.quantity = to_add;
        p->inventory[p->inventory_count].item.type = type;
        strncpy(p->inventory[p->inventory_count].item.name, name, 31);
        strncpy(p->inventory[p->inventory_count].item.icon, icon, 15);
        p->inventory[p->inventory_count].slot_index = p->inventory_count;
        p->inventory[p->inventory_count].is_equipped = 0;
        p->inventory_count++;
        quantity -= to_add;
    }
    
    save_player_inventory(player_index);
    
    // Update collect quest progress - use what was actually added
    int actually_added = original_quantity - quantity;
    if (actually_added > 0) {
        update_collect_quest_progress(player_index, item_id, actually_added);
    }
    
    return (quantity == 0) ? 1 : 0;
}

int remove_item_from_player(int player_index, int item_id, int quantity) {
    Player *p = &players[player_index];
    
    // Remove from inventory
    for (int i = 0; i < p->inventory_count; i++) {
        if (p->inventory[i].item.item_id == item_id) {
            if (p->inventory[i].item.quantity > quantity) {
                p->inventory[i].item.quantity -= quantity;
                save_player_inventory(player_index);
                return 1;
            } else if (p->inventory[i].item.quantity == quantity) {
                // Remove this slot
                for (int j = i; j < p->inventory_count - 1; j++) {
                    p->inventory[j] = p->inventory[j + 1];
                }
                p->inventory_count--;
                save_player_inventory(player_index);
                return 1;
            } else {
                quantity -= p->inventory[i].item.quantity;
                // Remove this slot and continue
                for (int j = i; j < p->inventory_count - 1; j++) {
                    p->inventory[j] = p->inventory[j + 1];
                }
                p->inventory_count--;
                i--; // Re-check this index
            }
        }
    }
    
    return (quantity == 0) ? 1 : 0;
}

void send_inventory_update(int player_index) {
    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PACKET_INVENTORY_UPDATE;
    pkt.player_id = players[player_index].id;
    
    // Copy inventory data
    for (int i = 0; i < players[player_index].inventory_count; i++) {
        pkt.inventory_slots[i] = players[player_index].inventory[i];
    }
    pkt.inventory_count = players[player_index].inventory_count;
    
    // Copy equipment data
    for (int i = 0; i < EQUIP_SLOT_COUNT; i++) {
        pkt.equipment[i] = players[player_index].equipment[i];
    }
    
    send_all(client_sockets[player_index], &pkt, sizeof(Packet), 0);
}

// Quest System Functions
void load_player_quests(int player_index) {
    Player *p = &players[player_index];
    player_quest_counts[player_index] = 0;
    
    sqlite3_stmt *stmt;
    const char *sql = "SELECT pq.QUEST_ID, q.NAME, q.DESCRIPTION, q.NPC_ID, q.REWARD_GOLD, q.REWARD_XP, "
                     "q.REWARD_ITEM_ID, q.REWARD_ITEM_QTY, q.LEVEL_REQUIRED, pq.STATUS, pq.PROGRESS "
                     "FROM player_quests pq JOIN quests q ON pq.QUEST_ID = q.QUEST_ID "
                     "WHERE pq.USER_ID=? AND pq.STATUS=0 ORDER BY pq.ACCEPTED_TIME DESC;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, p->id);
        
        while (sqlite3_step(stmt) == SQLITE_ROW && player_quest_counts[player_index] < 10) {
            Quest *q = &player_quests[player_index][player_quest_counts[player_index]];
            q->quest_id = sqlite3_column_int(stmt, 0);
            strncpy(q->name, (const char*)sqlite3_column_text(stmt, 1), 63);
            strncpy(q->description, (const char*)sqlite3_column_text(stmt, 2), 255);
            q->npc_id = sqlite3_column_int(stmt, 3);
            q->reward_gold = sqlite3_column_int(stmt, 4);
            q->reward_xp = sqlite3_column_int(stmt, 5);
            q->reward_item_id = sqlite3_column_int(stmt, 6);
            q->reward_item_qty = sqlite3_column_int(stmt, 7);
            q->level_required = sqlite3_column_int(stmt, 8);
            q->status = sqlite3_column_int(stmt, 9);
            
            // Parse progress string (format: "0/5,0/10")
            const char *progress = (const char*)sqlite3_column_text(stmt, 10);
            
            // Load objectives for this quest
            sqlite3_stmt *obj_stmt;
            const char *obj_sql = "SELECT OBJECTIVE_ID, OBJECTIVE_TYPE, TARGET_ID, TARGET_NAME, REQUIRED_COUNT "
                                 "FROM quest_objectives WHERE QUEST_ID=? ORDER BY OBJECTIVE_ID;";
            q->objective_count = 0;
            
            if (sqlite3_prepare_v2(db, obj_sql, -1, &obj_stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(obj_stmt, 1, q->quest_id);
                
                while (sqlite3_step(obj_stmt) == SQLITE_ROW && q->objective_count < 5) {
                    QuestObjective *obj = &q->objectives[q->objective_count];
                    obj->objective_id = sqlite3_column_int(obj_stmt, 0);
                    obj->quest_id = q->quest_id;
                    obj->objective_type = sqlite3_column_int(obj_stmt, 1);
                    obj->target_id = sqlite3_column_int(obj_stmt, 2);
                    strncpy(obj->target_name, (const char*)sqlite3_column_text(obj_stmt, 3), 31);
                    obj->required_count = sqlite3_column_int(obj_stmt, 4);
                    
                    // Parse current progress from progress string
                    obj->current_count = 0;
                    if (progress && strlen(progress) > 0) {
                        int obj_idx = q->objective_count;
                        const char *ptr = progress;
                        for (int i = 0; i <= obj_idx && ptr; i++) {
                            if (i == obj_idx) {
                                sscanf(ptr, "%d", &obj->current_count);
                                break;
                            }
                            ptr = strchr(ptr, ',');
                            if (ptr) ptr++;
                        }
                    }
                    
                    q->objective_count++;
                }
                sqlite3_finalize(obj_stmt);
            }
            
            player_quest_counts[player_index]++;
        }
        sqlite3_finalize(stmt);
    }
    
    LOG("Loaded %d active quests for player %s\n", player_quest_counts[player_index], p->username);
}

void send_quest_list(int player_index) {
    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PACKET_QUEST_LIST;
    
    // Copy player's active quests
    for (int i = 0; i < player_quest_counts[player_index]; i++) {
        pkt.quests[i] = player_quests[player_index][i];
    }
    pkt.quest_count = player_quest_counts[player_index];
    
    send_all(client_sockets[player_index], &pkt, sizeof(Packet), 0);
}

void save_quest_progress(int player_index, int quest_id) {
    Player *p = &players[player_index];
    
    // Find quest in player's quest list
    Quest *quest = NULL;
    int quest_index = -1;
    for (int i = 0; i < player_quest_counts[player_index]; i++) {
        if (player_quests[player_index][i].quest_id == quest_id) {
            quest = &player_quests[player_index][i];
            quest_index = i;
            break;
        }
    }
    
    if (!quest) return;
    
    // Build progress string
    char progress[256] = "";
    for (int i = 0; i < quest->objective_count; i++) {
        char buf[16];
        snprintf(buf, 16, "%d", quest->objectives[i].current_count);
        strcat(progress, buf);
        if (i < quest->objective_count - 1) strcat(progress, ",");
    }
    
    // Update database
    char sql[512];
    snprintf(sql, 512, "UPDATE player_quests SET PROGRESS='%s' WHERE USER_ID=%d AND QUEST_ID=%d;",
            progress, p->id, quest_id);
    sqlite3_exec(db, sql, 0, 0, 0);
}

void update_quest_progress(int player_index, int npc_id) {
    Player *p = &players[player_index];
    int progress_made = 0;
    
    // Check all active quests for visit objectives matching this NPC
    for (int i = 0; i < player_quest_counts[player_index]; i++) {
        Quest *quest = &player_quests[player_index][i];
        
        for (int j = 0; j < quest->objective_count; j++) {
            QuestObjective *obj = &quest->objectives[j];
            
            // Check for visit objectives (type 2) targeting this NPC
            if (obj->objective_type == 2 && obj->target_id == npc_id) {
                if (obj->current_count < obj->required_count) {
                    obj->current_count++;
                    save_quest_progress(player_index, quest->quest_id);
                    progress_made = 1;
                    
                    // Send progress update to client
                    Packet progress_pkt;
                    memset(&progress_pkt, 0, sizeof(Packet));
                    progress_pkt.type = PACKET_QUEST_PROGRESS;
                    progress_pkt.quest_id = quest->quest_id;
                    progress_pkt.quests[0] = *quest;
                    progress_pkt.quest_count = 1;
                    send_all(client_sockets[player_index], &progress_pkt, sizeof(Packet), 0);
                    
                    // Check if quest is now complete
                    int all_complete = 1;
                    for (int k = 0; k < quest->objective_count; k++) {
                        if (quest->objectives[k].current_count < quest->objectives[k].required_count) {
                            all_complete = 0;
                            break;
                        }
                    }
                    
                    if (all_complete) {
                        Packet chat_pkt;
                        memset(&chat_pkt, 0, sizeof(Packet));
                        chat_pkt.type = PACKET_CHAT;
                        chat_pkt.player_id = -1;
                        snprintf(chat_pkt.msg, 64, "Quest '%s' ready to complete!", quest->name);
                        send_all(client_sockets[player_index], &chat_pkt, sizeof(Packet), 0);
                    }
                    
                    LOG("Player %s updated quest %d objective %d: %d/%d\n", 
                        p->username, quest->quest_id, j, obj->current_count, obj->required_count);
                }
            }
        }
    }
}

void update_collect_quest_progress(int player_index, int item_id, int quantity) {
    Player *p = &players[player_index];
    
    // Check all active quests for collect objectives matching this item
    for (int i = 0; i < player_quest_counts[player_index]; i++) {
        Quest *quest = &player_quests[player_index][i];
        
        for (int j = 0; j < quest->objective_count; j++) {
            QuestObjective *obj = &quest->objectives[j];
            
            // Check for collect objectives (type 1) targeting this item
            if (obj->objective_type == 1 && obj->target_id == item_id) {
                obj->current_count += quantity;
                if (obj->current_count > obj->required_count) {
                    obj->current_count = obj->required_count;
                }
                
                save_quest_progress(player_index, quest->quest_id);
                
                // Send progress update to client
                Packet progress_pkt;
                memset(&progress_pkt, 0, sizeof(Packet));
                progress_pkt.type = PACKET_QUEST_PROGRESS;
                progress_pkt.quest_id = quest->quest_id;
                progress_pkt.quests[0] = *quest;
                progress_pkt.quest_count = 1;
                send_all(client_sockets[player_index], &progress_pkt, sizeof(Packet), 0);
                
                LOG("Player %s updated quest %d collect objective: +%d %s (%d/%d)\n", 
                    p->username, quest->quest_id, quantity, obj->target_name, 
                    obj->current_count, obj->required_count);
                
                // Check if quest is now complete
                int all_complete = 1;
                for (int k = 0; k < quest->objective_count; k++) {
                    if (quest->objectives[k].current_count < quest->objectives[k].required_count) {
                        all_complete = 0;
                        break;
                    }
                }
                
                if (all_complete) {
                    Packet chat_pkt;
                    memset(&chat_pkt, 0, sizeof(Packet));
                    chat_pkt.type = PACKET_CHAT;
                    chat_pkt.player_id = -1;
                    snprintf(chat_pkt.msg, 64, "Quest '%s' ready to complete!", quest->name);
                    send_all(client_sockets[player_index], &chat_pkt, sizeof(Packet), 0);
                }
            }
        }
    }
}

void update_kill_quest_progress(int player_index, int enemy_type) {
    Player *p = &players[player_index];
    
    // Check all active quests for kill objectives matching this enemy type
    for (int i = 0; i < player_quest_counts[player_index]; i++) {
        Quest *quest = &player_quests[player_index][i];
        
        for (int j = 0; j < quest->objective_count; j++) {
            QuestObjective *obj = &quest->objectives[j];
            
            // Check for kill objectives (type 0) targeting this enemy type
            if (obj->objective_type == 0 && obj->target_id == enemy_type) {
                if (obj->current_count < obj->required_count) {
                    obj->current_count++;
                    save_quest_progress(player_index, quest->quest_id);
                    
                    // Send progress update to client
                    Packet progress_pkt;
                    memset(&progress_pkt, 0, sizeof(Packet));
                    progress_pkt.type = PACKET_QUEST_PROGRESS;
                    progress_pkt.quest_id = quest->quest_id;
                    progress_pkt.quests[0] = *quest;
                    progress_pkt.quest_count = 1;
                    send_all(client_sockets[player_index], &progress_pkt, sizeof(Packet), 0);
                    
                    LOG("Player %s updated quest %d kill objective: %s (%d/%d)\n", 
                        p->username, quest->quest_id, obj->target_name, 
                        obj->current_count, obj->required_count);
                    
                    // Check if quest is now complete
                    int all_complete = 1;
                    for (int k = 0; k < quest->objective_count; k++) {
                        if (quest->objectives[k].current_count < quest->objectives[k].required_count) {
                            all_complete = 0;
                            break;
                        }
                    }
                    
                    if (all_complete) {
                        Packet chat_pkt;
                        memset(&chat_pkt, 0, sizeof(Packet));
                        chat_pkt.type = PACKET_CHAT;
                        chat_pkt.player_id = -1;
                        snprintf(chat_pkt.msg, 64, "Quest '%s' ready to complete!", quest->name);
                        send_all(client_sockets[player_index], &chat_pkt, sizeof(Packet), 0);
                    }
                }
            }
        }
    }
}

void spawn_enemies() {
    server_enemy_count = 0;
    
    // Spawn 5 rats around the map
    for (int i = 0; i < 5 && server_enemy_count < 50; i++) {
        Enemy *rat = &server_enemies[server_enemy_count];
        rat->enemy_id = next_enemy_id++;
        rat->type = 1;  // 1 = Rat
        strcpy(rat->name, "Rat");
        rat->x = 300.0f + (rand() % 800);
        rat->y = 300.0f + (rand() % 600);
        rat->hp = 10;
        rat->max_hp = 10;
        rat->active = 1;
        strcpy(rat->map_name, "map.jpg");
        rat->attack_power = 2;
        rat->defense = 1;
        server_enemy_count++;
    }
    
    LOG("Spawned %d enemies\n", server_enemy_count);
}

void send_enemy_list(int player_index) {
    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PACKET_ENEMY_LIST;
    
    // Send all active enemies
    pkt.enemy_count = 0;
    for (int i = 0; i < server_enemy_count && pkt.enemy_count < 20; i++) {
        if (server_enemies[i].active) {
            pkt.enemies[pkt.enemy_count++] = server_enemies[i];
        }
    }
    
    send_all(client_sockets[player_index], &pkt, sizeof(Packet), 0);
    LOG("Sent %d enemies to player %d\n", pkt.enemy_count, player_index);
}

void broadcast_enemy_list() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (SOCKET_IS_VALID(client_sockets[i]) && players[i].active) {
            send_enemy_list(i);
        }
    }
}
void send_player_stats(int index) {
    Packet currency_pkt;
    memset(&currency_pkt, 0, sizeof(Packet));
    currency_pkt.type = PACKET_CURRENCY_UPDATE;
    currency_pkt.gold = players[index].gold;
    currency_pkt.xp = players[index].xp;
    currency_pkt.level = players[index].level;
    currency_pkt.hp = players[index].hp;
    currency_pkt.max_hp = players[index].max_hp;
    currency_pkt.mana = players[index].mana;
    currency_pkt.max_mana = players[index].max_mana;
    currency_pkt.str = players[index].str;
    currency_pkt.agi = players[index].agi;
    currency_pkt.intel = players[index].intel;
    currency_pkt.skill_points = players[index].skill_points;
    currency_pkt.pvp_enabled = players[index].pvp_enabled;
    currency_pkt.clan_id = players[index].clan_id;
    currency_pkt.clan_role = players[index].clan_role;
    send_all(client_sockets[index], &currency_pkt, sizeof(Packet), 0);
}


void load_triggers() {
    FILE *fp = fopen("triggers.txt", "r");
    if (!fp) {
        printf("WARNING: Could not open triggers.txt on server. No triggers loaded.\n");
        server_trigger_count = 0;
        return;
    }
    
    server_trigger_count = 0;
    while (server_trigger_count < 20 && 
           fscanf(fp, "%31s %d %d %d %d %31s %d %d",
                  server_triggers[server_trigger_count].src_map,
                  &server_triggers[server_trigger_count].rect_x,
                  &server_triggers[server_trigger_count].rect_y,
                  &server_triggers[server_trigger_count].rect_w,
                  &server_triggers[server_trigger_count].rect_h,
                  server_triggers[server_trigger_count].target_map,
                  &server_triggers[server_trigger_count].spawn_x,
                  &server_triggers[server_trigger_count].spawn_y) == 8) {
        // Ensure null termination (defensive programming)
        server_triggers[server_trigger_count].src_map[31] = '\0';
        server_triggers[server_trigger_count].target_map[31] = '\0';
        
        LOG("Server: Loaded Trigger %d: %s [%d,%d %dx%d] -> %s\n",
               server_trigger_count,
               server_triggers[server_trigger_count].src_map,
               server_triggers[server_trigger_count].rect_x,
               server_triggers[server_trigger_count].rect_y,
               server_triggers[server_trigger_count].rect_w,
               server_triggers[server_trigger_count].rect_h,
               server_triggers[server_trigger_count].target_map);
        server_trigger_count++;
    }
    LOG("Server: Total Triggers Loaded: %d\n", server_trigger_count);
    fclose(fp);
}

void load_npcs() {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT NPC_ID, NAME, X, Y, MAP_NAME, NPC_TYPE, ICON FROM npcs;";
    
    server_npc_count = 0;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW && server_npc_count < 50) {
            server_npcs[server_npc_count].npc_id = sqlite3_column_int(stmt, 0);
            strncpy(server_npcs[server_npc_count].name, (const char*)sqlite3_column_text(stmt, 1), 31);
            server_npcs[server_npc_count].name[31] = '\0';
            server_npcs[server_npc_count].x = (float)sqlite3_column_double(stmt, 2);
            server_npcs[server_npc_count].y = (float)sqlite3_column_double(stmt, 3);
            strncpy(server_npcs[server_npc_count].map_name, (const char*)sqlite3_column_text(stmt, 4), 31);
            server_npcs[server_npc_count].map_name[31] = '\0';
            server_npcs[server_npc_count].npc_type = sqlite3_column_int(stmt, 5);
            strncpy(server_npcs[server_npc_count].icon, (const char*)sqlite3_column_text(stmt, 6), 15);
            server_npcs[server_npc_count].icon[15] = '\0';
            
            LOG("Loaded NPC %d: %s at (%.1f, %.1f) on %s\n",
                server_npcs[server_npc_count].npc_id,
                server_npcs[server_npc_count].name,
                server_npcs[server_npc_count].x,
                server_npcs[server_npc_count].y,
                server_npcs[server_npc_count].map_name);
            
            server_npc_count++;
        }
        sqlite3_finalize(stmt);
    }
    
    LOG("Loaded %d NPCs from database\n", server_npc_count);
    
    // If no NPCs in database, create some default ones
    if (server_npc_count == 0) {
        LOG("No NPCs found, creating default NPCs...\n");
        sqlite3_exec(db, "INSERT INTO npcs (NPC_ID, NAME, X, Y, MAP_NAME, NPC_TYPE, ICON) VALUES (1, 'Quest Giver Sarah', 200.0, 200.0, 'map.jpg', 0, 'quest.png');", 0, 0, 0);
        sqlite3_exec(db, "INSERT INTO npcs (NPC_ID, NAME, X, Y, MAP_NAME, NPC_TYPE, ICON) VALUES (2, 'Blacksmith John', 250.0, 150.0, 'map.jpg', 1, 'blacksmith.png');", 0, 0, 0);
        sqlite3_exec(db, "INSERT INTO npcs (NPC_ID, NAME, X, Y, MAP_NAME, NPC_TYPE, ICON) VALUES (3, 'General Store', 150.0, 150.0, 'map.jpg', 1, 'merchant.png');", 0, 0, 0);
        
        // Reload after adding defaults
        load_npcs();
    }
}

void send_npc_list(int player_index) {
    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PACKET_NPC_LIST;
    
    // Copy NPCs into packet (max 20)
    pkt.npc_count = (server_npc_count > 20) ? 20 : server_npc_count;
    for (int i = 0; i < pkt.npc_count; i++) {
        pkt.npcs[i] = server_npcs[i];
    }
    
    send_all(client_sockets[player_index], &pkt, sizeof(Packet), 0);
    LOG("Sent %d NPCs to player %d\n", pkt.npc_count, player_index);
}

void spawn_random_ground_items() {
    // Spawn 10-15 random items on the default map
    ground_item_count = 0;
    int num_items = 10 + (rand() % 6); // 10-15 items
    
    for (int i = 0; i < num_items && ground_item_count < MAX_GROUND_ITEMS; i++) {
        int item_id = 1 + (rand() % 9); // Random item ID (1-9)
        ground_items[ground_item_count].item_id = item_id;
        ground_items[ground_item_count].x = 100 + (rand() % 1800); // Random X
        ground_items[ground_item_count].y = 100 + (rand() % 1800); // Random Y
        ground_items[ground_item_count].quantity = 1 + (rand() % 3); // 1-3 items
        strcpy(ground_items[ground_item_count].map_name, "map.jpg");
        ground_items[ground_item_count].spawn_time = time(NULL);
        
        // Get item name from database
        char sql[256];
        snprintf(sql, 256, "SELECT NAME FROM items WHERE ITEM_ID=%d;", item_id);
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                strncpy(ground_items[ground_item_count].name, (const char*)sqlite3_column_text(stmt, 0), 31);
                ground_items[ground_item_count].name[31] = '\0';
            } else {
                strcpy(ground_items[ground_item_count].name, "Unknown");
            }
            sqlite3_finalize(stmt);
        }
        
        ground_item_count++;
    }
    
    LOG("Spawned %d random ground items on map.jpg\n", ground_item_count);
}

void send_ground_items_to_client(int client_index) {
    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PACKET_GROUND_ITEMS;
    
    // Send only items on the player's current map
    pkt.ground_item_count = 0;
    for (int i = 0; i < ground_item_count && pkt.ground_item_count < MAX_GROUND_ITEMS; i++) {
        if (strcmp(ground_items[i].map_name, players[client_index].map_name) == 0) {
            pkt.ground_items[pkt.ground_item_count++] = ground_items[i];
        }
    }
    
    send_all(client_sockets[client_index], &pkt, sizeof(Packet), 0);
    LOG("Sent %d ground items to client %d\n", pkt.ground_item_count, client_index);
}

void broadcast_ground_items() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (SOCKET_IS_VALID(client_sockets[i]) && players[i].active) {
            send_ground_items_to_client(i);
        }
    }
}

// Updated to accept 'int flags' so it matches the send() signature
int send_all(socket_t sockfd, void *buf, size_t len, int flags) {
    size_t total = 0;
    size_t bytes_left = len;
    int n;
    while(total < len) {
        // Pass the 'flags' (usually 0) through to the real send
        n = send(sockfd, (char*)buf + total, bytes_left, flags);
        if (n == -1) { break; }
        total += n;
        bytes_left -= n;
    }
    return (n == -1) ? -1 : 0; 
}

void send_triggers_to_client(int client_index) {
    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PACKET_TRIGGERS_DATA;
    pkt.trigger_count = server_trigger_count;
    
    for (int i = 0; i < server_trigger_count; i++) {
        pkt.triggers[i] = server_triggers[i];
    }
    
    send_all(client_sockets[client_index], &pkt, sizeof(Packet), 0);
    LOG("Sent %d triggers to client %d\n", server_trigger_count, client_index);
}

void load_telemetry();  // Forward declaration

void init_game() {
    init_db(); init_storage(); load_triggers(); load_npcs(); load_telemetry(); spawn_enemies();
    // spawn_random_ground_items();  // Spawn items on server start - DISABLED for mobile testing
    for (int i = 0; i < MAX_CLIENTS; i++) { 
        client_sockets[i] = SOCKET_INVALID; 
        players[i].active = 0; 
        players[i].id = -1; 
        players[i].is_typing = 0;
        player_quest_counts[i] = 0;  // Initialize quest counts
        player_trades[i].partner_id = -1; // Initialize trade state
    }
}

void broadcast_state() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!SOCKET_IS_VALID(client_sockets[i]) || !players[i].active) continue;

        Packet pkt; 
        memset(&pkt, 0, sizeof(Packet));
        pkt.type = PACKET_UPDATE;
        
        // Filter players for this client
        int count = 0;
        for (int j = 0; j < MAX_CLIENTS; j++) {
            if (!players[j].active) continue;

            // Instancing logic:
            // 1. Same map.
            // 2. If it's an instanced map (dungeon.map), must be in same party.
            int same_map = (strcmp(players[i].map_name, players[j].map_name) == 0);
            int instanced = (strcmp(players[i].map_name, "dungeon_solo.png") == 0);
            
            if (same_map) {
                if (instanced) {
                    // Isolation: Only see self or party members
                    int my_party = players[i].party_id;
                    int their_party = players[j].party_id;

                    if (i == j) {
                        pkt.players[count++] = players[j];
                    } else if (my_party != -1 && my_party == their_party) {
                        pkt.players[count++] = players[j];
                    }
                } else if (strcmp(players[i].map_name, "clan_map") == 0 || strcmp(players[i].map_name, "house_clan") == 0) {
                     // Clan Map & Clan House: Only see self or clan members
                     int my_clan = players[i].clan_id;
                     int their_clan = players[j].clan_id;
                     
                     if (i == j) {
                         pkt.players[count++] = players[j];
                     } else if (my_clan != -1 && my_clan == their_clan) {
                         pkt.players[count++] = players[j];
                     }
                } else if (instanced || strcmp(players[i].map_name, "house_solo") == 0) {
                     // Solo/Party Dungeon & Solo House: Only see self or party members
                     int my_party = players[i].party_id;
                     int their_party = players[j].party_id;

                     if (i == j) {
                         pkt.players[count++] = players[j];
                     } else if (my_party != -1 && my_party == their_party) {
                         pkt.players[count++] = players[j];
                     }
                } else {
                    // Open map
                    pkt.players[count++] = players[j];
                }
            }
        }
        send_all(client_sockets[i], &pkt, sizeof(Packet), 0);
    }
}

void send_friend_list(int client_index) {
    int my_id = players[client_index].id; if (my_id == -1) return;
    Packet pkt; pkt.type = PACKET_FRIEND_LIST; pkt.friend_count = 0;
    char sql[512];
    snprintf(sql, 512, "SELECT f.FRIEND_ID, u.USERNAME, u.LAST_LOGIN FROM friends f JOIN users u ON f.FRIEND_ID = u.ID WHERE f.USER_ID=%d AND f.STATUS=1;", my_id);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW && pkt.friend_count < 20) {
            int fid = sqlite3_column_int(stmt, 0);
            const unsigned char *fname = sqlite3_column_text(stmt, 1);
            const unsigned char *flogin = sqlite3_column_text(stmt, 2);
            pkt.friends[pkt.friend_count].id = fid;
            strncpy(pkt.friends[pkt.friend_count].username, (const char*)fname, 31);
            if (flogin) strncpy(pkt.friends[pkt.friend_count].last_login, (const char*)flogin, 31);
            else strcpy(pkt.friends[pkt.friend_count].last_login, "Unknown");
            pkt.friends[pkt.friend_count].is_online = 0;
            for(int i=0; i<MAX_CLIENTS; i++) if(players[i].active && players[i].id == fid) pkt.friends[pkt.friend_count].is_online = 1;
            pkt.friend_count++;
        }
        sqlite3_finalize(stmt);
    }
    send_all(client_sockets[client_index], &pkt, sizeof(Packet), 0);
}


void broadcast_party_update(int party_id) {
    if (party_id == -1) return;

    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PACKET_PARTY_UPDATE;
    pkt.party_member_count = 0;

    // Find all members from database (includes offline members)
    sqlite3_stmt *stmt;
    const char *sql = "SELECT ID, USERNAME FROM users WHERE PARTY_ID = ? LIMIT 5;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, party_id);
        while (sqlite3_step(stmt) == SQLITE_ROW && pkt.party_member_count < 5) {
            pkt.party_member_ids[pkt.party_member_count] = sqlite3_column_int(stmt, 0);
            strncpy(pkt.party_member_names[pkt.party_member_count], (const char*)sqlite3_column_text(stmt, 1), 31);
            pkt.party_member_count++;
        }
        sqlite3_finalize(stmt);
    }

    // Broadcast to online members
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (players[i].active && players[i].party_id == party_id) {
            send_all(client_sockets[i], &pkt, sizeof(Packet), 0);
        }
    }
}

void broadcast_clan_update(int clan_id) {
    if (clan_id == -1) return;

    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PACKET_CLAN_UPDATE;
    pkt.clan_id = clan_id;
    pkt.clan_member_count = 0;

    sqlite3_stmt *stmt;
    // 1. Get clan info
    const char *sql_clan = "SELECT NAME, GOLD_STORAGE FROM clans WHERE ID = ?;";
    if (sqlite3_prepare_v2(db, sql_clan, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, clan_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            strncpy(pkt.clan_name, (const char*)sqlite3_column_text(stmt, 0), 31);
            pkt.clan_gold = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    // 2. Get members
    const char *sql_members = "SELECT ID, USERNAME FROM users WHERE CLAN_ID = ? LIMIT 50;";
    if (sqlite3_prepare_v2(db, sql_members, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, clan_id);
        while (sqlite3_step(stmt) == SQLITE_ROW && pkt.clan_member_count < 50) {
            pkt.clan_member_ids[pkt.clan_member_count] = sqlite3_column_int(stmt, 0);
            strncpy(pkt.clan_member_names[pkt.clan_member_count], (const char*)sqlite3_column_text(stmt, 1), 31);
            pkt.clan_member_count++;
        }
        sqlite3_finalize(stmt);
    }

    // 3. Get storage items
    const char *sql_storage = "SELECT csi.SLOT_INDEX, csi.ITEM_ID, i.NAME, i.TYPE, i.ICON, csi.QUANTITY "
                              "FROM clan_storage_items csi JOIN items i ON csi.ITEM_ID = i.ITEM_ID "
                              "WHERE csi.CLAN_ID = ? LIMIT 20;";
    if (sqlite3_prepare_v2(db, sql_storage, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, clan_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int slot = sqlite3_column_int(stmt, 0);
            if (slot >= 0 && slot < 20) {
                pkt.clan_storage_items[slot].item_id = sqlite3_column_int(stmt, 1);
                strncpy(pkt.clan_storage_items[slot].name, (const char*)sqlite3_column_text(stmt, 2), 31);
                pkt.clan_storage_items[slot].type = sqlite3_column_int(stmt, 3);
                strncpy(pkt.clan_storage_items[slot].icon, (const char*)sqlite3_column_text(stmt, 4), 15);
                pkt.clan_storage_items[slot].quantity = sqlite3_column_int(stmt, 5);
            }
        }
        sqlite3_finalize(stmt);
    }

    // 4. Broadcast to online members
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (players[i].active && players[i].clan_id == clan_id) {
            pkt.clan_role = players[i].clan_role;
            send_all(client_sockets[i], &pkt, sizeof(Packet), 0);
        }
    }
}

void broadcast_friend_update() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (SOCKET_IS_VALID(client_sockets[i]) && players[i].active) send_friend_list(i);
    }
}

void process_admin_command(int sender_idx, char *msg) {
    if (players[sender_idx].role != ROLE_ADMIN) {
        Packet err; err.type = PACKET_CHAT; err.player_id = -1; strcpy(err.msg, "Error: You do not have permission.");
        send_all(client_sockets[sender_idx], &err, sizeof(Packet), 0); return;
    }
    char cmd[16], arg1[32], arg2[32];
    int args = sscanf(msg, "%s %s %s", cmd, arg1, arg2);
    int target_id = -1; int new_role = ROLE_PLAYER;
    if (strcmp(cmd, "/demote") == 0 && args >= 2) { target_id = atoi(arg1); new_role = ROLE_PLAYER; }
    else if (strcmp(cmd, "/promote") == 0 && args >= 3) { new_role = get_role_id_from_name(arg1); target_id = atoi(arg2); if(new_role==-1)return; } else return;

    if (target_id != -1) {
        char sql[256]; snprintf(sql, 256, "UPDATE users SET ROLE=%d WHERE ID=%d;", new_role, target_id);
        sqlite3_exec(db, sql, 0, 0, 0);
        for(int i=0; i<MAX_CLIENTS; i++) {
            if (players[i].active && players[i].id == target_id) {
                players[i].role = new_role;
                Packet notif; notif.type = PACKET_CHAT; notif.player_id = -1; snprintf(notif.msg, 64, "Role updated.");
                send_all(client_sockets[i], &notif, sizeof(Packet), 0);
            }
        }
        broadcast_state();
    }
}

void process_nick_change(int index, Packet *pkt) {
    char *new_nick = pkt->username; // We reuse the username field for the NEW nick
    char *password = pkt->password; // We reuse password field
    char *confirm  = pkt->msg;      // We reuse msg field for "CONFIRM"

    // 1. Validation
    if (strcmp(confirm, "CONFIRM") != 0) {
        Packet resp; resp.type = PACKET_CHANGE_NICK_RESPONSE; resp.status = AUTH_FAILURE;
        strcpy(resp.msg, "Type CONFIRM to proceed.");
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
        return;
    }

    if (strlen(new_nick) < 3 || strlen(new_nick) > 31) {
        Packet resp; resp.type = PACKET_CHANGE_NICK_RESPONSE; resp.status = AUTH_FAILURE;
        strcpy(resp.msg, "Name too short/long.");
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
        return;
    }

    // 2. Check DB
    sqlite3_stmt *stmt;
    char sql[256];
    
    // Verify Password & Time Limit
    snprintf(sql, 256, "SELECT PASSWORD, LAST_NICK_CHANGE FROM users WHERE ID=%d;", players[index].id);
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *db_pass = (const char*)sqlite3_column_text(stmt, 0);
        int last_change = sqlite3_column_int(stmt, 1);
        
        if (strcmp(password, db_pass) != 0) {
            Packet resp; resp.type = PACKET_CHANGE_NICK_RESPONSE; resp.status = AUTH_FAILURE;
            strcpy(resp.msg, "Wrong Password.");
            send_all(client_sockets[index], &resp, sizeof(Packet), 0);
            sqlite3_finalize(stmt);
            return;
        }

        // Time Check (30 Days = 2592000 seconds)
        time_t now = time(NULL);
        if (now - last_change < 2592000 && last_change != 0) {
            Packet resp; resp.type = PACKET_CHANGE_NICK_RESPONSE; resp.status = AUTH_FAILURE;
            int days_left = (2592000 - (now - last_change)) / 86400;
            snprintf(resp.msg, 64, "Wait %d more days.", days_left);
            send_all(client_sockets[index], &resp, sizeof(Packet), 0);
            sqlite3_finalize(stmt);
            return;
        }
    }
    sqlite3_finalize(stmt);

    // 3. Check Uniqueness
    snprintf(sql, 256, "SELECT ID FROM users WHERE USERNAME=?;");
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, new_nick, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Packet resp; resp.type = PACKET_CHANGE_NICK_RESPONSE; resp.status = AUTH_FAILURE;
        strcpy(resp.msg, "Name taken.");
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_finalize(stmt);

    // 4. Update DB
    snprintf(sql, 256, "UPDATE users SET USERNAME='%s', LAST_NICK_CHANGE=%ld WHERE ID=%d;", new_nick, time(NULL), players[index].id);
    if (sqlite3_exec(db, sql, 0, 0, 0) == SQLITE_OK) {
        // Update Memory
        strncpy(players[index].username, new_nick, 31);
        
        Packet resp; resp.type = PACKET_CHANGE_NICK_RESPONSE; resp.status = AUTH_SUCCESS;
        strcpy(resp.msg, "Nickname Changed!");
        strncpy(resp.username, new_nick, 31); // Send back new name
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
        
        // Broadcast updates
        broadcast_state();
        broadcast_friend_update(); // Updates friend lists with new name
    } else {
        Packet resp; resp.type = PACKET_CHANGE_NICK_RESPONSE; resp.status = AUTH_FAILURE;
        strcpy(resp.msg, "Database Error.");
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
    }
}

void process_password_change(int index, Packet *pkt) {
    char *current_pass = pkt->password;  // Current password
    char *new_pass = pkt->username;      // New password
    char *confirm_pass = pkt->msg;       // Confirm password
    
    // 1. Validation
    if (strlen(new_pass) < 8) {
        Packet resp;
        resp.type = PACKET_CHANGE_PASSWORD_RESPONSE;
        resp.status = AUTH_FAILURE;
        strcpy(resp.msg, "New password must be 8+ chars.");
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
        return;
    }
    
    if (strcmp(new_pass, confirm_pass) != 0) {
        Packet resp;
        resp.type = PACKET_CHANGE_PASSWORD_RESPONSE;
        resp.status = AUTH_FAILURE;
        strcpy(resp.msg, "Passwords don't match.");
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
        return;
    }
    
    // 2. Verify current password
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "SELECT PASSWORD FROM users WHERE ID=?;", -1, &stmt, 0);
    sqlite3_bind_int(stmt, 1, players[index].id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *db_pass = (const char*)sqlite3_column_text(stmt, 0);
        
        if (strcmp(current_pass, db_pass) != 0) {
            Packet resp;
            resp.type = PACKET_CHANGE_PASSWORD_RESPONSE;
            resp.status = AUTH_FAILURE;
            strcpy(resp.msg, "Current password incorrect.");
            send_all(client_sockets[index], &resp, sizeof(Packet), 0);
            sqlite3_finalize(stmt);
            return;
        }
    } else {
        Packet resp;
        resp.type = PACKET_CHANGE_PASSWORD_RESPONSE;
        resp.status = AUTH_FAILURE;
        strcpy(resp.msg, "User not found.");
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_finalize(stmt);
    
    // 3. Update password in database using prepared statement
    sqlite3_prepare_v2(db, "UPDATE users SET PASSWORD=? WHERE ID=?;", -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, new_pass, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, players[index].id);
    
    int result = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (result == SQLITE_DONE) {
        Packet resp;
        resp.type = PACKET_CHANGE_PASSWORD_RESPONSE;
        resp.status = AUTH_SUCCESS;
        strcpy(resp.msg, "Password changed successfully!");
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
    } else {
        Packet resp;
        resp.type = PACKET_CHANGE_PASSWORD_RESPONSE;
        resp.status = AUTH_FAILURE;
        strcpy(resp.msg, "Database error.");
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
    }
}

void send_pending_requests(int client_index) {
    int my_id = players[client_index].id;
    
    // Find rows where FRIEND_ID is ME, and STATUS is 0 (Pending)
    char sql[512];
    snprintf(sql, 512, 
        "SELECT f.USER_ID, u.USERNAME "
        "FROM friends f "
        "JOIN users u ON f.USER_ID = u.ID "
        "WHERE f.FRIEND_ID=%d AND f.STATUS=0;", my_id);

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Packet req; 
            req.type = PACKET_FRIEND_INCOMING;
            req.player_id = sqlite3_column_int(stmt, 0); // The requester's ID
            strncpy(req.username, (const char*)sqlite3_column_text(stmt, 1), 31);
            
            send_all(client_sockets[client_index], &req, sizeof(Packet), 0);
        }
        sqlite3_finalize(stmt);
    }
}

// Telemetry structure to hold data in memory
typedef struct {
    int user_id;
    char gl_renderer[128];
    char os_info[128];
} TelemetryEntry;

TelemetryEntry gl_telemetry[MAX_CLIENTS * 10];  // Support multiple entries per user
TelemetryEntry os_telemetry[MAX_CLIENTS * 10];
int gl_telemetry_count = 0;
int os_telemetry_count = 0;

void load_telemetry() {
    // Load GL telemetry from raw data file
    FILE *fp = fopen("telemetryGL_raw.txt", "r");
    if (fp) {
        gl_telemetry_count = 0;
        char renderer[128];
        int user_id;
        while (fscanf(fp, "%d %127[^\n]\n", &user_id, renderer) == 2 && gl_telemetry_count < MAX_CLIENTS * 10) {
            gl_telemetry[gl_telemetry_count].user_id = user_id;
            strncpy(gl_telemetry[gl_telemetry_count].gl_renderer, renderer, 127);
            gl_telemetry[gl_telemetry_count].gl_renderer[127] = '\0';
            gl_telemetry_count++;
        }
        fclose(fp);
        LOG("Loaded %d GL telemetry entries\n", gl_telemetry_count);
    }
    
    // Load OS telemetry from raw data file
    fp = fopen("telemetryOS_raw.txt", "r");
    if (fp) {
        os_telemetry_count = 0;
        char os_info[128];
        int user_id;
        while (fscanf(fp, "%d %127[^\n]\n", &user_id, os_info) == 2 && os_telemetry_count < MAX_CLIENTS * 10) {
            os_telemetry[os_telemetry_count].user_id = user_id;
            strncpy(os_telemetry[os_telemetry_count].os_info, os_info, 127);
            os_telemetry[os_telemetry_count].os_info[127] = '\0';
            os_telemetry_count++;
        }
        fclose(fp);
        LOG("Loaded %d OS telemetry entries\n", os_telemetry_count);
    }
}

void save_telemetry() {
    // Save raw GL telemetry data for persistence
    FILE *fp = fopen("telemetryGL_raw.txt", "w");
    if (fp) {
        for (int i = 0; i < gl_telemetry_count; i++) {
            fprintf(fp, "%d %s\n", gl_telemetry[i].user_id, gl_telemetry[i].gl_renderer);
        }
        fclose(fp);
    }
    
    // Save aggregated GL telemetry - writes user counts per renderer
    fp = fopen("telemetryGL.txt", "w");
    if (fp) {
        // Count occurrences of each unique renderer
        for (int i = 0; i < gl_telemetry_count; i++) {
            int already_counted = 0;
            
            // Check if we already counted this renderer
            for (int j = 0; j < i; j++) {
                if (strcmp(gl_telemetry[i].gl_renderer, gl_telemetry[j].gl_renderer) == 0) {
                    already_counted = 1;
                    break;
                }
            }
            
            if (already_counted) continue;
            
            // Count how many unique users have this renderer
            int unique_users[MAX_CLIENTS * 10];
            int unique_count = 0;
            for (int j = 0; j < gl_telemetry_count; j++) {
                if (strcmp(gl_telemetry[i].gl_renderer, gl_telemetry[j].gl_renderer) == 0) {
                    // Check if user_id already in unique_users
                    int found = 0;
                    for (int k = 0; k < unique_count; k++) {
                        if (unique_users[k] == gl_telemetry[j].user_id) {
                            found = 1;
                            break;
                        }
                    }
                    if (!found && unique_count < MAX_CLIENTS * 10) {
                        unique_users[unique_count++] = gl_telemetry[j].user_id;
                    }
                }
            }
            
            fprintf(fp, "%s: %d user%s\n", gl_telemetry[i].gl_renderer, unique_count, unique_count == 1 ? "" : "s");
        }
        fclose(fp);
    }
    
    // Save raw OS telemetry data for persistence
    fp = fopen("telemetryOS_raw.txt", "w");
    if (fp) {
        for (int i = 0; i < os_telemetry_count; i++) {
            fprintf(fp, "%d %s\n", os_telemetry[i].user_id, os_telemetry[i].os_info);
        }
        fclose(fp);
    }
    
    // Save aggregated OS telemetry
    fp = fopen("telemetryOS.txt", "w");
    if (fp) {
        // Count occurrences of each unique OS
        for (int i = 0; i < os_telemetry_count; i++) {
            int already_counted = 0;
            
            // Check if we already counted this OS
            for (int j = 0; j < i; j++) {
                if (strcmp(os_telemetry[i].os_info, os_telemetry[j].os_info) == 0) {
                    already_counted = 1;
                    break;
                }
            }
            
            if (already_counted) continue;
            
            // Count how many unique users have this OS
            int unique_users[MAX_CLIENTS * 10];
            int unique_count = 0;
            for (int j = 0; j < os_telemetry_count; j++) {
                if (strcmp(os_telemetry[i].os_info, os_telemetry[j].os_info) == 0) {
                    // Check if user_id already in unique_users
                    int found = 0;
                    for (int k = 0; k < unique_count; k++) {
                        if (unique_users[k] == os_telemetry[j].user_id) {
                            found = 1;
                            break;
                        }
                    }
                    if (!found && unique_count < MAX_CLIENTS * 10) {
                        unique_users[unique_count++] = os_telemetry[j].user_id;
                    }
                }
            }
            
            fprintf(fp, "%s: %d user%s\n", os_telemetry[i].os_info, unique_count, unique_count == 1 ? "" : "s");
        }
        fclose(fp);
    }
}

void update_telemetry(int user_id, const char *gl_renderer, const char *os_info) {
    // Update GL telemetry - check if this exact combo exists
    int gl_found = 0;
    for (int i = 0; i < gl_telemetry_count; i++) {
        if (gl_telemetry[i].user_id == user_id && 
            strcmp(gl_telemetry[i].gl_renderer, gl_renderer) == 0) {
            gl_found = 1;
            break;
        }
    }
    
    if (!gl_found && gl_telemetry_count < MAX_CLIENTS * 10) {
        gl_telemetry[gl_telemetry_count].user_id = user_id;
        strncpy(gl_telemetry[gl_telemetry_count].gl_renderer, gl_renderer, 127);
        gl_telemetry[gl_telemetry_count].gl_renderer[127] = '\0';
        gl_telemetry_count++;
        LOG("New GL telemetry: User %d - %s\n", user_id, gl_renderer);
    }
    
    // Update OS telemetry - check if this exact combo exists
    int os_found = 0;
    for (int i = 0; i < os_telemetry_count; i++) {
        if (os_telemetry[i].user_id == user_id && 
            strcmp(os_telemetry[i].os_info, os_info) == 0) {
            os_found = 1;
            break;
        }
    }
    
    if (!os_found && os_telemetry_count < MAX_CLIENTS * 10) {
        os_telemetry[os_telemetry_count].user_id = user_id;
        strncpy(os_telemetry[os_telemetry_count].os_info, os_info, 127);
        os_telemetry[os_telemetry_count].os_info[127] = '\0';
        os_telemetry_count++;
        LOG("New OS telemetry: User %d - %s\n", user_id, os_info);
    }
    
    // Save updated telemetry
    save_telemetry();
}

void handle_client_message(int index, Packet *pkt) {
    if (players[index].id == -1) {
        Packet response; 
        memset(&response, 0, sizeof(Packet)); // Good practice to clear it
        response.type = PACKET_AUTH_RESPONSE;

        if (pkt->type == PACKET_REGISTER_REQUEST) {
        if (pkt->username[0] == '\0' || pkt->password[0] == '\0') {
                response.status = AUTH_FAILURE;
                strcpy(response.msg, "Username and password required.");
                LOG("Registration failed from client %d: %s\n", index, response.msg);
            } else if (strlen(pkt->username) < 5) {
                response.status = AUTH_FAILURE;
                strcpy(response.msg, "Username must be at least 5 characters.");
                LOG("Registration failed from client %d: %s\n", index, response.msg);
            } else if (strlen(pkt->password) < 8) {
                response.status = AUTH_FAILURE;
                strcpy(response.msg, "Password must be at least 8 characters.");
                LOG("Registration failed from client %d: %s\n", index, response.msg);
            } else {
                response.status = register_user(pkt->username, pkt->password);
                if (response.status == AUTH_REGISTER_SUCCESS) {
                    strcpy(response.msg, "Registered.");
                    LOG("User '%s' registered successfully\n", pkt->username);
                }
                else if (response.status == AUTH_REGISTER_FAILED_EXISTS) {
                    strcpy(response.msg, "User exists.");
                    LOG("Registration failed for '%s': User already exists\n", pkt->username);
                }
                else {
                    strcpy(response.msg, "Registration failed.");
                    LOG("Registration failed for '%s': Database error\n", pkt->username);
                }
            }
        } 
        else if (pkt->type == PACKET_LOGIN_REQUEST) {
            float x, y; 
            uint8_t r, g, b;
            uint8_t r2, g2, b2; 
            int role;
            long ban_expire = 0;
            char db_map[32] = "map.jpg";
            int gold, xp, level, p_str, p_agi, p_intel, p_skill_points, p_pvp_enabled, party_id, clan_id, clan_role;

            // Pass &ban_expire and currency fields to the function
            if (pkt->username[0] == '\0' || pkt->password[0] == '\0') {
                response.status = AUTH_FAILURE;
                strcpy(response.msg, "Username and password required.");
                LOG("Login failed from client %d: %s\n", index, response.msg);
            }
            else if (login_user(pkt->username, pkt->password, &x, &y, &r, &g, &b, &r2, &g2, &b2, &role, &ban_expire, db_map, &gold, &xp, &level, &p_str, &p_agi, &p_intel, &p_skill_points, &p_pvp_enabled, &party_id, &clan_id, &clan_role)) {
                
                // 1. Check Ban Logic
                if (time(NULL) < ban_expire) {
                    response.status = AUTH_FAILURE;
                    strcpy(response.msg, "Account is Banned."); // Fixed variable name
                    LOG("Login failed for '%s': Account is banned\n", pkt->username);
                    send_all(client_sockets[index], &response, sizeof(Packet), 0);
                    return;
                }

                // 2. Check Double Login
                int already = 0; 
                for(int i=0; i<MAX_CLIENTS; i++) {
                    if(players[i].active && strcmp(players[i].username, pkt->username) == 0) {
                        already = 1; break;
                    }
                }

                if(already) {
                    response.status = AUTH_FAILURE;
                    strcpy(response.msg, "Already logged in.");
                    LOG("Login failed for '%s': Already logged in\n", pkt->username);
                } 
                else {
                    response.status = AUTH_SUCCESS; 
                    players[index].active = 1; 
                    players[index].id = get_user_id(pkt->username); 
                    
                    // Auto-Admin ID 1 (Optional, generally safer to do in DB manually)
                    if (players[index].id == 1 && role != ROLE_ADMIN) { 
                        role = ROLE_ADMIN; 
                        sqlite3_exec(db, "UPDATE users SET ROLE=1 WHERE ID=1;", 0, 0, 0); 
                    }

                    players[index].x = x; 
                    players[index].y = y; 
                    players[index].r = r; 
                    players[index].g = g; 
                    players[index].b = b; 
                    players[index].r2 = r2; 
                    players[index].g2 = g2; 
                    players[index].b2 = b2;
                    players[index].role = role;
                    players[index].gold = gold;
                    players[index].xp = xp;
                    players[index].level = level;
                    players[index].skill_points = p_skill_points;
                    players[index].str = p_str;
                    players[index].agi = p_agi;
                    players[index].intel = p_intel;
                    players[index].skill_points = p_skill_points;
                    players[index].hp = 100;
                    players[index].max_hp = 100;
                    players[index].mana = 50;
                    players[index].max_mana = 50;
                    players[index].last_attack_time = 0;
                    
                    // Initialize HP/Mana based on level
                    int base_hp = 100;
                    int base_mana = 50;
                    players[index].max_hp = base_hp + (level - 1) * 10;  // +10 HP per level
                    players[index].max_mana = base_mana + (level - 1) * 5;  // +5 mana per level
                    players[index].hp = players[index].max_hp;  // Full HP on login
                    players[index].mana = players[index].max_mana;  // Full mana on login
                    players[index].party_id = party_id; 
                    players[index].clan_id = clan_id;
                    players[index].clan_role = clan_role;
                    
                    strncpy(players[index].username, pkt->username, 31);
                    strncpy(players[index].map_name, db_map, 31);
                    
                    LOG("User '%s' (ID: %d) logged in successfully (Gold: %d, Level: %d, HP: %d/%d, Clan: %d)\n", 
                        pkt->username, players[index].id, gold, level, players[index].hp, players[index].max_hp, clan_id);
                    
                    response.player_id = players[index].id;
                    response.gold = players[index].gold;
                    response.xp = players[index].xp;
                    response.level = players[index].level;
                    response.hp = players[index].hp;
                    response.max_hp = players[index].max_hp;
                    response.mana = players[index].mana;
                    response.max_mana = players[index].max_mana;
                    response.str = players[index].str;
                    response.agi = players[index].agi;
                    response.intel = players[index].intel;
                    response.skill_points = players[index].skill_points;
                    response.pvp_enabled = players[index].pvp_enabled;
                    response.clan_id = players[index].clan_id;
                    response.clan_role = players[index].clan_role;
                    send_all(client_sockets[index], &response, sizeof(Packet), 0);
                    
                    // Load player's inventory
                    load_player_inventory(index);
                    send_inventory_update(index);
                    
                    // Load player's active quests
                    load_player_quests(index);
                    // Don't send quest list on login - too large, send on demand instead
                    // send_quest_list(index);
                    
                    // Send NPCs to client
                    send_npc_list(index);
                    
                    // Send triggers to client after successful login
                    send_triggers_to_client(index);
                    
                    // Send ground items to client
                    send_ground_items_to_client(index);
                    
                    // Send enemies to client
                    send_enemy_list(index);
                    
                    broadcast_friend_update(); 
                    send_pending_requests(index); 
                    
                    if (players[index].party_id != -1) {
                        broadcast_party_update(players[index].party_id);
                    }
                    
                    broadcast_state(); 
                    if (players[index].clan_id != -1) {
                        broadcast_clan_update(players[index].clan_id);
                    }
                    return;
                }
            } else {
                response.status = AUTH_FAILURE;
                strcpy(response.msg, "Invalid username or password.");
                LOG("Login failed for '%s': Invalid credentials\n", pkt->username);
            }
        }
        send_all(client_sockets[index], &response, sizeof(Packet), 0); 
        return;
    }

    if (pkt->type == PACKET_MAP_CHANGE) {
        // Enforce Clan Map restrictions
        if (strcmp(pkt->target_map, "clan_map") == 0 || strcmp(pkt->target_map, "house_clan") == 0) {
           if (players[index].clan_id == -1) {
               Packet err; memset(&err, 0, sizeof(Packet));
               err.type = PACKET_CHAT; err.player_id = -1;
               strcpy(err.msg, "Only clan members can enter the Clan Hall.");
               send_all(client_sockets[index], &err, sizeof(Packet), 0);
               return;
           }
        }
        
        // Allow change
        strncpy(players[index].map_name, pkt->target_map, 31);
        players[index].x = pkt->dx;
        players[index].y = pkt->dy;
        broadcast_state(); // Notify others (so they disappear/appear)
        
        // Send ground items for new map
        send_ground_items_to_client(index);
        // Send NPCs for new map (if we had map-specific NPCs)
        // send_npc_list(index); 
    }

    if (pkt->type == PACKET_MOVE) {
        float length = sqrt(pkt->dx * pkt->dx + pkt->dy * pkt->dy);
        if (length > 0) {
            pkt->dx /= length; pkt->dy /= length;
            players[index].x += pkt->dx * PLAYER_SPEED; players[index].y += pkt->dy * PLAYER_SPEED;
            if (players[index].x < 0) players[index].x = 0; if (players[index].x > MAP_WIDTH - 32) players[index].x = MAP_WIDTH - 32;
            if (players[index].y < 0) players[index].y = 0; if (players[index].y > MAP_HEIGHT - 32) players[index].y = MAP_HEIGHT - 32;
        }
        // REMOVED: broadcast_state(); -- Handled by Tick Thread
    } 
    else if (pkt->type == PACKET_CHAT) {
        // --- ADMIN COMMANDS ---
        if (pkt->msg[0] == '/') {
            // Trim leading/trailing whitespace if needed (simplified here with strncmp logic)
            
            // 1. General Player Commands (No role required)
            if (strncmp(pkt->msg, "/pvp", 4) == 0 && (pkt->msg[4] == '\0' || pkt->msg[4] == ' ')) {
                players[index].pvp_enabled = !players[index].pvp_enabled;
                char sql[256];
                snprintf(sql, 256, "UPDATE users SET PVP_ENABLED=%d WHERE ID=%d;", players[index].pvp_enabled, players[index].id);
                sqlite3_exec(db, sql, 0, 0, 0);
                
                Packet resp; memset(&resp, 0, sizeof(Packet));
                resp.type = PACKET_CHAT; resp.player_id = -1;
                snprintf(resp.msg, 127, "PvP %s", players[index].pvp_enabled ? "ENABLED" : "DISABLED");
                send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                
                Packet pvp_pkt; memset(&pvp_pkt, 0, sizeof(Packet));
                pvp_pkt.type = PACKET_PVP_TOGGLE;
                pvp_pkt.pvp_enabled = players[index].pvp_enabled;
                send_all(client_sockets[index], &pvp_pkt, sizeof(Packet), 0);
                
                LOG("Player %s toggled PvP to %d\n", players[index].username, players[index].pvp_enabled);
                return;
            }

            // 2. Admin Commands check
            if (players[index].role < ROLE_ADMIN) {
                Packet err; memset(&err, 0, sizeof(Packet));
                err.type = PACKET_CHAT; err.player_id = -1;
                snprintf(err.msg, 127, "Unknown command: %s", pkt->msg); // Echo for debugging
                send_all(client_sockets[index], &err, sizeof(Packet), 0);
                return;
            }

            // 3. Admin Command Logic
            if (strncmp(pkt->msg, "/unban ", 7) == 0) {
                int target_id = atoi(pkt->msg + 7);
                if (target_id > 0) {
                    char sql[256];
                    // Reset ban expiration to 0
                    snprintf(sql, 256, "UPDATE users SET BAN_EXPIRE=0 WHERE ID=%d;", target_id);
                    sqlite3_exec(db, sql, 0, 0, 0);
                    
                    Packet resp; resp.type = PACKET_CHAT; resp.player_id = -1;
                    snprintf(resp.msg, 64, "ID %d has been UNBANNED.", target_id);
                    send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                }
            }
            // 3. /unwarn <ID> (Remove last warning)
            else if (strncmp(pkt->msg, "/unwarn ", 8) == 0) {
                int target_id = atoi(pkt->msg + 8);
                if (target_id > 0) {
                    char sql[256];
                    
                    // A. Decrement user count (prevent negative)
                    snprintf(sql, 256, "UPDATE users SET WARN_COUNT = MAX(0, WARN_COUNT - 1) WHERE ID=%d;", target_id);
                    sqlite3_exec(db, sql, 0, 0, 0);

                    // B. Delete most recent warning entry
                    // SQLite specific: Delete row with highest ID for this user
                    snprintf(sql, 256, "DELETE FROM warnings WHERE ID = (SELECT MAX(ID) FROM warnings WHERE USER_ID=%d);", target_id);
                    sqlite3_exec(db, sql, 0, 0, 0);

                    Packet resp; resp.type = PACKET_CHAT; resp.player_id = -1;
                    snprintf(resp.msg, 64, "ID %d: Last warning removed.", target_id);
                    send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                }
            }
            // 4. /give <player_id> <item_id> <quantity>
            else if (strncmp(pkt->msg, "/give ", 6) == 0) {
                int target_id, item_id, quantity;
                if (sscanf(pkt->msg + 6, "%d %d %d", &target_id, &item_id, &quantity) == 3) {
                    // Find target player by user ID
                    int target_index = -1;
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (players[i].active && players[i].id == target_id) {
                            target_index = i;
                            break;
                        }
                    }
                    
                    if (target_index >= 0) {
                        give_item_to_player(target_index, item_id, quantity);
                        send_inventory_update(target_index);
                        
                        Packet resp; resp.type = PACKET_CHAT; resp.player_id = -1;
                        snprintf(resp.msg, 127, "Gave %d x item %d to player ID %d (%s)", 
                                quantity, item_id, target_id, players[target_index].username);
                        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                        
                        LOG("Admin %s gave %d x item %d to player %s\n", 
                            players[index].username, quantity, item_id, players[target_index].username);
                    } else {
                        Packet resp; resp.type = PACKET_CHAT; resp.player_id = -1;
                        snprintf(resp.msg, 127, "Player ID %d not found or offline.", target_id);
                        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                    }
                } else {
                    Packet resp; resp.type = PACKET_CHAT; resp.player_id = -1;
                    strcpy(resp.msg, "Usage: /give <player_id> <item_id> <quantity>");
                    send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                }
            }
            // 5. /setgold <player_id> <amount>
            else if (strncmp(pkt->msg, "/setgold ", 9) == 0) {
                int target_id, amount;
                if (sscanf(pkt->msg + 9, "%d %d", &target_id, &amount) == 2) {
                    // Find target player by user ID
                    int target_index = -1;
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (players[i].active && players[i].id == target_id) {
                            target_index = i;
                            break;
                        }
                    }
                    
                    if (target_index >= 0 && amount >= 0) {
                        players[target_index].gold = amount;
                        
                        // Update database
                        char sql[256];
                        snprintf(sql, 256, "UPDATE users SET GOLD=%d WHERE ID=%d;", amount, target_id);
                        sqlite3_exec(db, sql, 0, 0, 0);
                        
                        send_player_stats(target_index);
                        
                        Packet resp; resp.type = PACKET_CHAT; resp.player_id = -1;
                        snprintf(resp.msg, 127, "Set gold to %d for player ID %d (%s)", 
                                amount, target_id, players[target_index].username);
                        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                        
                        LOG("Admin %s set gold to %d for player %s\n", 
                            players[index].username, amount, players[target_index].username);
                    } else {
                        Packet resp; resp.type = PACKET_CHAT; resp.player_id = -1;
                        snprintf(resp.msg, 127, "Player ID %d not found or invalid amount.", target_id);
                        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                    }
                } else {
                    Packet resp; resp.type = PACKET_CHAT; resp.player_id = -1;
                    strcpy(resp.msg, "Usage: /setgold <player_id> <amount>");
                    send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                }
            }
            // 6. /removeitem <player_id> <item_id> <quantity>
            else if (strncmp(pkt->msg, "/removeitem ", 12) == 0 || strncmp(pkt->msg, "/removeitm ", 11) == 0) {
                int offset = (pkt->msg[8] == 'e') ? 12 : 11;
                int target_id, item_id, quantity;
                if (sscanf(pkt->msg + offset, "%d %d %d", &target_id, &item_id, &quantity) == 3) {
                    // Find target player by user ID
                    int target_index = -1;
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (players[i].active && players[i].id == target_id) {
                            target_index = i;
                            break;
                        }
                    }
                    
                    if (target_index >= 0) {
                        int success = remove_item_from_player(target_index, item_id, quantity);
                        send_inventory_update(target_index);
                        
                        Packet resp; resp.type = PACKET_CHAT; resp.player_id = -1;
                        if (success) {
                            snprintf(resp.msg, 127, "Removed %d x item %d from player ID %d (%s)", 
                                    quantity, item_id, target_id, players[target_index].username);
                            LOG("Admin %s removed %d x item %d from player %s\n", 
                                players[index].username, quantity, item_id, players[target_index].username);
                        } else {
                            snprintf(resp.msg, 127, "Failed to remove item (insufficient quantity or not found)");
                        }
                        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                    } else {
                        Packet resp; resp.type = PACKET_CHAT; resp.player_id = -1;
                        snprintf(resp.msg, 127, "Player ID %d not found or offline.", target_id);
                        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                    }
                } else {
                    Packet resp; resp.type = PACKET_CHAT; resp.player_id = -1;
                    strcpy(resp.msg, "Usage: /removeitem <player_id> <item_id> <quantity>");
                    send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                }
            }
            // 4. /role <ID> <LEVEL> (Bonus: Set role via command)
            else if (strncmp(pkt->msg, "/role ", 6) == 0) {
                int target_id, level;
                if (sscanf(pkt->msg + 6, "%d %d", &target_id, &level) == 2) {
                    char sql[256];
                    snprintf(sql, 256, "UPDATE users SET ROLE=%d WHERE ID=%d;", level, target_id);
                    sqlite3_exec(db, sql, 0, 0, 0);
                    
                    // Update online player if active
                    for(int i=0; i<MAX_CLIENTS; i++) {
                        if(players[i].active && players[i].id == target_id) {
                            players[i].role = level;
                            broadcast_state(); // Update colors/tags for everyone
                        }
                    }
                    Packet resp; resp.type = PACKET_CHAT; resp.player_id = -1;
                    snprintf(resp.msg, 64, "ID %d Role set to %d.", target_id, level);
                    send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                }
            }
            // /setsp <player_id> <amount>
            else if (strncmp(pkt->msg, "/setsp ", 7) == 0) {
                int target_id, amount;
                if (sscanf(pkt->msg + 7, "%d %d", &target_id, &amount) == 2) {
                    int target_index = -1;
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (players[i].active && players[i].id == target_id) {
                            target_index = i;
                            break;
                        }
                    }
                    if (target_index >= 0) {
                        players[target_index].skill_points = amount;
                        char sql[256];
                        snprintf(sql, 256, "UPDATE users SET SKILL_POINTS=%d WHERE ID=%d;", amount, target_id);
                        sqlite3_exec(db, sql, 0, 0, 0);
                        send_player_stats(target_index);
                        Packet resp; resp.type = PACKET_CHAT; resp.player_id = -1;
                        snprintf(resp.msg, 127, "Set skill points to %d for ID %d", amount, target_id);
                        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                    }
                }
            }
            else {
                Packet err; err.type = PACKET_CHAT; err.player_id = -1;
                strcpy(err.msg, "Invalid command.");
                send_all(client_sockets[index], &err, sizeof(Packet), 0);
            }
            return; // Don't broadcast commands
        }
        // -----------------------

        // Normal Chat Broadcast
        pkt->player_id = players[index].id;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (SOCKET_IS_VALID(client_sockets[i]) && players[i].active) {
                // Local channel: only send to players on the same map
                if (pkt->chat_channel == CHAT_CHANNEL_LOCAL) {
                    if (strcmp(players[i].map_name, players[index].map_name) != 0) continue;
                }
                // Global, Trade, etc. are seen by everyone
                send_all(client_sockets[i], pkt, sizeof(Packet), 0);
            }
        }
    }
    else if (pkt->type == PACKET_FRIEND_REQUEST) {
        int target_id = pkt->target_id;
        int my_id = players[index].id;

        // 1. Prevent adding self
        if (target_id == my_id) {
            Packet err; err.type = PACKET_CHAT; err.player_id = -1;
            strcpy(err.msg, "Error: You cannot add yourself.");
            send_all(client_sockets[index], &err, sizeof(Packet), 0);
            return;
        }

        // 2. Check if Target ID exists in Database
        sqlite3_stmt *stmt;
        char sql[256];
        snprintf(sql, 256, "SELECT ID FROM users WHERE ID=%d;", target_id);
        sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
        
        int exists = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) exists = 1;
        sqlite3_finalize(stmt);

        if (!exists) {
            Packet err; err.type = PACKET_CHAT; err.player_id = -1;
            snprintf(err.msg, 64, "Error: User ID %d does not exist.", target_id);
            send_all(client_sockets[index], &err, sizeof(Packet), 0);
            return;
        }
        
        // 3. Check if already friends (Optional, prevents spamming DB)
        // Note: INSERT OR IGNORE below handles unique constraint, but logic here is safer
        
        // 4. Valid User -> Proceed with Request
        // Store in DB as Pending (Status 0)
        // INSERT OR IGNORE prevents duplicates if request already sent
        snprintf(sql, 256, "INSERT OR IGNORE INTO friends (USER_ID, FRIEND_ID, STATUS) VALUES (%d, %d, 0);", my_id, target_id);
        sqlite3_exec(db, sql, 0, 0, 0);

        // 5. If Online, notify target immediately
        int target_idx = -1;
        for(int i=0; i<MAX_CLIENTS; i++) if (players[i].active && players[i].id == target_id) target_idx = i;
        
        if (target_idx != -1) {
            Packet req; req.type = PACKET_FRIEND_INCOMING;
            req.player_id = players[index].id; 
            strcpy(req.username, players[index].username);
            send_all(client_sockets[target_idx], &req, sizeof(Packet), 0);
        }

        // 6. Confirm to Sender
        Packet succ; succ.type = PACKET_CHAT; succ.player_id = -1;
        snprintf(succ.msg, 64, "Friend request sent to ID %d.", target_id);
        send_all(client_sockets[index], &succ, sizeof(Packet), 0);
    }
    else if (pkt->type == PACKET_FRIEND_RESPONSE) {
        int requester_id = pkt->target_id;
        int my_id = players[index].id;
        char sql[256];

        if (pkt->response_accepted) {
            // A. Update the original request to Status 1
            snprintf(sql, 256, "UPDATE friends SET STATUS=1 WHERE USER_ID=%d AND FRIEND_ID=%d;", requester_id, my_id);
            sqlite3_exec(db, sql, 0, 0, 0);
            
            // B. Insert the reverse relationship as Status 1
            snprintf(sql, 256, "INSERT OR REPLACE INTO friends (USER_ID, FRIEND_ID, STATUS) VALUES (%d, %d, 1);", my_id, requester_id);
            sqlite3_exec(db, sql, 0, 0, 0);
            
            // Refresh
            send_friend_list(index);
            for(int i=0; i<MAX_CLIENTS; i++) if(players[i].id == requester_id) send_friend_list(i);
        } else {
            // Deny: Delete the pending request
            snprintf(sql, 256, "DELETE FROM friends WHERE USER_ID=%d AND FRIEND_ID=%d;", requester_id, my_id);
            sqlite3_exec(db, sql, 0, 0, 0);
        }
    }
    else if (pkt->type == PACKET_FRIEND_REMOVE) {
        int my_id = players[index].id; int target_id = pkt->target_id;
        // Delete both directions
        char sql[256]; snprintf(sql, 256, "DELETE FROM friends WHERE (USER_ID=%d AND FRIEND_ID=%d) OR (USER_ID=%d AND FRIEND_ID=%d);", my_id, target_id, target_id, my_id);
        sqlite3_exec(db, sql, 0, 0, 0);
        send_friend_list(index);
        for(int i=0; i<MAX_CLIENTS; i++) if(players[i].active && players[i].id == target_id) send_friend_list(i);
    }
    else if (pkt->type == PACKET_PRIVATE_MESSAGE) {
        int target_id = pkt->target_id; int sender_id = players[index].id;
        Packet pm; pm.type = PACKET_PRIVATE_MESSAGE; pm.player_id = sender_id; pm.target_id = target_id; strncpy(pm.msg, pkt->msg, 64);
        int target_found = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) if (players[i].active && players[i].id == target_id) { if (SOCKET_IS_VALID(client_sockets[i])) { send_all(client_sockets[i], &pm, sizeof(Packet), 0); target_found = 1; } break; }
        if (target_found) send_all(client_sockets[index], &pm, sizeof(Packet), 0);
        else { Packet err; err.type = PACKET_CHAT; err.player_id = -1; strcpy(err.msg, "Player not online."); send_all(client_sockets[index], &err, sizeof(Packet), 0); }
    }
    else if (pkt->type == PACKET_PING) { send_all(client_sockets[index], pkt, sizeof(Packet), 0); }
    else if (pkt->type == PACKET_STATUS_CHANGE) { players[index].status = pkt->new_status; broadcast_state(); }
    else if (pkt->type == PACKET_COLOR_CHANGE) {
        players[index].r = pkt->r; players[index].g = pkt->g; players[index].b = pkt->b;
        players[index].r2 = pkt->r2; players[index].g2 = pkt->g2; players[index].b2 = pkt->b2;
        char sql[256]; snprintf(sql, 256, "UPDATE users SET R=%d, G=%d, B=%d WHERE ID=%d;", pkt->r, pkt->g, pkt->b, players[index].id);
        snprintf(sql, 256, "UPDATE users SET R=%d, G=%d, B=%d, R2=%d, G2=%d, B2=%d WHERE ID=%d;", 
            pkt->r, pkt->g, pkt->b, pkt->r2, pkt->g2, pkt->b2, players[index].id);
        sqlite3_exec(db, sql, 0, 0, 0); broadcast_state();
    }
    else if (pkt->type == PACKET_AVATAR_REQUEST) {
        int target = pkt->target_id; char filepath[64]; snprintf(filepath, 64, "avatars/%d.img", target);
        FILE *fp = fopen(filepath, "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END); int size = ftell(fp); fseek(fp, 0, SEEK_SET);
            if (size <= MAX_AVATAR_SIZE) {
                Packet resp; resp.type = PACKET_AVATAR_RESPONSE; resp.player_id = target; resp.image_size = size;
                send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                uint8_t *file_buf = malloc(size); fread(file_buf, 1, size, fp);
                send_all(client_sockets[index], file_buf, size, 0); free(file_buf);
            }
            fclose(fp);
        }
    }
    else if (pkt->type == PACKET_CHANGE_NICK_REQUEST) {
        process_nick_change(index, pkt);
    }
    else if (pkt->type == PACKET_CHANGE_PASSWORD_REQUEST) {
        process_password_change(index, pkt);
    }
    else if (pkt->type == PACKET_ROLE_LIST_REQUEST) {
        Packet resp; 
        resp.type = PACKET_ROLE_LIST_RESPONSE;
        resp.role_count = 0;

        sqlite3_stmt *stmt;
        const char *sql = "SELECT ID, USERNAME, ROLE FROM users WHERE ROLE > 0 ORDER BY ROLE ASC;";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW && resp.role_count < 50) {
                int idx = resp.role_count;
                resp.roles[idx].id = sqlite3_column_int(stmt, 0);
                const unsigned char *name = sqlite3_column_text(stmt, 1);
                strncpy(resp.roles[idx].username, (const char*)name, 31);
                resp.roles[idx].role = sqlite3_column_int(stmt, 2);
                resp.role_count++;
            }
        }
        sqlite3_finalize(stmt);
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
    }
    else if (pkt->type == PACKET_SANCTION_REQUEST) {
        if (players[index].role < ROLE_ADMIN) return; // Security Check

        int target_id = pkt->target_id;
        
        if (pkt->sanction_type == 0) { // WARN
            // Add to DB
            sqlite3_stmt *stmt;
            char sql[256];
            snprintf(sql, 256, "INSERT INTO warnings (USER_ID, REASON, TIMESTAMP) VALUES (%d, ?, %ld);", target_id, time(NULL));
            sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
            sqlite3_bind_text(stmt, 1, pkt->sanction_reason, -1, SQLITE_STATIC);
            sqlite3_step(stmt); sqlite3_finalize(stmt);

            // Increment Count
            snprintf(sql, 256, "UPDATE users SET WARN_COUNT = WARN_COUNT + 1 WHERE ID=%d;", target_id);
            sqlite3_exec(db, sql, 0, 0, 0);

            // Check Auto-Ban (3 Strikes)
            snprintf(sql, 256, "SELECT WARN_COUNT FROM users WHERE ID=%d;", target_id);
            sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int count = sqlite3_column_int(stmt, 0);
                if (count >= 3) {
                    // Auto Ban (10 Years)
                    long expire = time(NULL) + (10 * 31536000);
                    char ban_sql[256];
                    snprintf(ban_sql, 256, "UPDATE users SET BAN_EXPIRE=%ld WHERE ID=%d;", expire, target_id);
                    sqlite3_exec(db, ban_sql, 0, 0, 0);
                    
                    // Kick if online
                    for(int i=0; i<MAX_CLIENTS; i++) if(players[i].active && players[i].id == target_id) {
                        Packet k; k.type = PACKET_KICK; strcpy(k.msg, "Banned: 3 Warnings Reached.");
                        send_all(client_sockets[i], &k, sizeof(Packet), 0);
                        close(client_sockets[i]); players[i].active = 0;
                    }
                }
            }
            sqlite3_finalize(stmt);
            
            // Notify Admin
            Packet msg; msg.type = PACKET_CHAT; msg.player_id = -1; strcpy(msg.msg, "Player Warned.");
            send_all(client_sockets[index], &msg, sizeof(Packet), 0);

        } 
        else { // BAN
            long duration = parse_ban_duration(pkt->ban_duration);
            long expire = time(NULL) + duration;
            
            char sql[256];
            snprintf(sql, 256, "UPDATE users SET BAN_EXPIRE=%ld WHERE ID=%d;", expire, target_id);
            sqlite3_exec(db, sql, 0, 0, 0);

            // Kick if online
            for(int i=0; i<MAX_CLIENTS; i++) if(players[i].active && players[i].id == target_id) {
                Packet k; k.type = PACKET_KICK; 
                snprintf(k.msg, 64, "Banned: %s", pkt->sanction_reason);
                send_all(client_sockets[i], &k, sizeof(Packet), 0);
                close(client_sockets[i]); players[i].active = 0;
            }
             Packet msg; msg.type = PACKET_CHAT; msg.player_id = -1; strcpy(msg.msg, "Player Banned.");
             send_all(client_sockets[index], &msg, sizeof(Packet), 0);
        }
    }

    // 3. FETCH WARNINGS (For User)
    else if (pkt->type == PACKET_WARNINGS_REQUEST) {
        Packet resp; resp.type = PACKET_WARNINGS_RESPONSE; resp.warning_count = 0;
        
        char sql[256];
        snprintf(sql, 256, "SELECT REASON, TIMESTAMP FROM warnings WHERE USER_ID=%d ORDER BY ID DESC LIMIT 20;", players[index].id);
        
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int i = resp.warning_count++;
                const char *r = (const char*)sqlite3_column_text(stmt, 0);
                time_t t = sqlite3_column_int64(stmt, 1);
                
                strncpy(resp.warnings[i].reason, r, 63);
                struct tm *tm_info = localtime(&t);
                strftime(resp.warnings[i].date, 32, "%Y-%m-%d", tm_info);
            }
        }
        sqlite3_finalize(stmt);
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
    }
    else if (pkt->type == PACKET_MAP_CHANGE) {
        strncpy(players[index].map_name, pkt->target_map, 31);
        players[index].x = pkt->dx; // Use dx/dy for spawn coords
        players[index].y = pkt->dy;
        
        // Save to DB
        char sql[256];
        snprintf(sql, 256, "UPDATE users SET MAP='%s', X=%f, Y=%f WHERE ID=%d;", 
            players[index].map_name, players[index].x, players[index].y, players[index].id);
        sqlite3_exec(db, sql, 0, 0, 0);
        
        broadcast_state(); // Tell everyone I moved to a new dimension
    }
    else if (pkt->type == PACKET_TELEMETRY) {
        if (players[index].id != -1) {
            update_telemetry(players[index].id, pkt->gl_renderer, pkt->os_info);
        }
    }
    else if (pkt->type == PACKET_TYPING) {
        // Update typing status and broadcast
        players[index].is_typing = pkt->player_id; // 1 = typing, 0 = not typing
        broadcast_state();
    }
    else if (pkt->type == PACKET_ITEM_PICKUP) {
        // Client requests to pick up nearest ground item
        int pickup_idx = pkt->item_slot; // We'll use this to send which item index to pick up
        
        if (pickup_idx >= 0 && pickup_idx < ground_item_count) {
            GroundItem *item = &ground_items[pickup_idx];
            
            // Check if on same map
            if (strcmp(item->map_name, players[index].map_name) == 0) {
                // Give item to player
                if (give_item_to_player(index, item->item_id, item->quantity)) {
                    LOG("Player %s picked up item %d (x%d) from ground\n", 
                        players[index].username, item->item_id, item->quantity);
                    
                    // Remove from ground items array
                    for (int i = pickup_idx; i < ground_item_count - 1; i++) {
                        ground_items[i] = ground_items[i + 1];
                    }
                    ground_item_count--;
                    
                    send_inventory_update(index);
                    broadcast_ground_items(); // Update all clients
                }
            }
        }
    }
    else if (pkt->type == PACKET_ITEM_DROP) {
        // Client drops an item from slot
        int slot = pkt->item_slot;
        if (slot >= 0 && slot < players[index].inventory_count) {
            int item_id = players[index].inventory[slot].item.item_id;
            int quantity = players[index].inventory[slot].item.quantity;
            
            // Remove from inventory
            if (remove_item_from_player(index, item_id, quantity)) {
                // Add to ground items at player's position
                if (ground_item_count < MAX_GROUND_ITEMS) {
                    ground_items[ground_item_count].item_id = item_id;
                    ground_items[ground_item_count].x = players[index].x + 50; // Offset to prevent instant pickup
                    ground_items[ground_item_count].y = players[index].y + 50;
                    ground_items[ground_item_count].quantity = quantity;
                    strcpy(ground_items[ground_item_count].map_name, players[index].map_name);
                    ground_items[ground_item_count].spawn_time = time(NULL);
                    ground_item_count++;
                    
                    LOG("Player %s dropped item %d (x%d) at %.1f,%.1f\n", 
                        players[index].username, item_id, quantity, 
                        players[index].x, players[index].y);
                    
                    broadcast_ground_items(); // Update all clients
                }
            }
            
            send_inventory_update(index);
            LOG("Player %s dropped item %d (x%d)\n", players[index].username, item_id, quantity);
        }
    }
    else if (pkt->type == PACKET_ITEM_USE) {
        // Client uses an item from slot
        int slot = pkt->item_slot;
        if (slot >= 0 && slot < players[index].inventory_count) {
            int item_id = players[index].inventory[slot].item.item_id;
            
            if (item_id == 1) { // Health Potion
                int heal_amount = 25;
                if (players[index].hp < players[index].max_hp) {
                    players[index].hp += heal_amount;
                    if (players[index].hp > players[index].max_hp) {
                        players[index].hp = players[index].max_hp;
                    }
                    
                    // Consume one quantity
                    remove_item_from_player(index, item_id, 1);
                    send_inventory_update(index);
                    
                    // Sync stats to client
                    send_player_stats(index);
                    
                    // Notify player
                    Packet chat_pkt;
                    memset(&chat_pkt, 0, sizeof(Packet));
                    chat_pkt.type = PACKET_CHAT;
                    chat_pkt.player_id = -1;
                    snprintf(chat_pkt.msg, 64, "You used a Health Potion and restored %d HP!", heal_amount);
                    send_all(client_sockets[index], &chat_pkt, sizeof(Packet), 0);
                    
                    LOG("Player %s used Health Potion (ID 1), restored HP to %d/%d\n", 
                        players[index].username, players[index].hp, players[index].max_hp);
                } else {
                    // Already at max HP
                    Packet chat_pkt;
                    memset(&chat_pkt, 0, sizeof(Packet));
                    chat_pkt.type = PACKET_CHAT;
                    chat_pkt.player_id = -1;
                    strcpy(chat_pkt.msg, "You are already at full health!");
                    send_all(client_sockets[index], &chat_pkt, sizeof(Packet), 0);
                }
            } else {
                // For other items, just log for now
                LOG("Player %s tried to use unhandled item %d\n", players[index].username, item_id);
            }
        }
    }
    else if (pkt->type == PACKET_ITEM_EQUIP) {
        // Client equips/unequips an item
        int slot = pkt->item_slot;
        int equip_slot = pkt->equip_slot;
        
        if (slot >= 0 && slot < players[index].inventory_count && 
            equip_slot >= 0 && equip_slot < EQUIP_SLOT_COUNT) {
            
            Item *item = &players[index].inventory[slot].item;
            
            // Check if there's already an equipped item in that slot
            if (players[index].equipment[equip_slot].item_id > 0) {
                // Unequip current item back to inventory
                Item old_item = players[index].equipment[equip_slot];
                give_item_to_player(index, old_item.item_id, old_item.quantity);
            }
            
            // Equip the new item
            players[index].equipment[equip_slot] = *item;
            
            // Remove from inventory
            remove_item_from_player(index, item->item_id, item->quantity);
            
            save_player_inventory(index);
            send_inventory_update(index);
            
            LOG("Player %s equipped item %d to slot %d\n", 
                players[index].username, item->item_id, equip_slot);
        }
    }
    else if (pkt->type == PACKET_ITEM_UNEQUIP) {
        // Client unequips an item from equipment slot
        int equip_slot = pkt->equip_slot;
        
        if (equip_slot >= 0 && equip_slot < EQUIP_SLOT_COUNT) {
            // Check if there's an item equipped
            if (players[index].equipment[equip_slot].item_id > 0) {
                Item equipped_item = players[index].equipment[equip_slot];
                
                // Add to inventory
                give_item_to_player(index, equipped_item.item_id, equipped_item.quantity);
                
                // Clear equipment slot
                memset(&players[index].equipment[equip_slot], 0, sizeof(Item));
                
                save_player_inventory(index);
                send_inventory_update(index);
                
                LOG("Player %s unequipped item %d from slot %d\n", 
                    players[index].username, equipped_item.item_id, equip_slot);
            }
        }
    }
    else if (pkt->type == PACKET_NPC_INTERACT) {
        // Player clicked on an NPC
        int npc_id = pkt->npc_id;
        
        LOG("Player %s interacted with NPC %d\n", players[index].username, npc_id);
        
        // Check NPC type
        int npc_type = -1;
        for (int i = 0; i < server_npc_count; i++) {
            if (server_npcs[i].npc_id == npc_id) {
                npc_type = server_npcs[i].npc_type;
                break;
            }
        }
        
        // If merchant, send shop data
        if (npc_type == NPC_TYPE_MERCHANT) {
            Packet shop_pkt;
            memset(&shop_pkt, 0, sizeof(Packet));
            shop_pkt.type = PACKET_SHOP_OPEN;
            shop_pkt.shop_data.npc_id = npc_id;
            
            // Load shop items from database
            sqlite3_stmt *shop_stmt;
            const char *shop_sql = "SELECT si.ITEM_ID, i.NAME, i.TYPE, i.ICON, i.MAX_STACK, si.BUY_PRICE, si.SELL_PRICE "
                                   "FROM shop_items si JOIN items i ON si.ITEM_ID = i.ITEM_ID "
                                   "WHERE si.NPC_ID=? LIMIT 20;";
            
            shop_pkt.shop_data.item_count = 0;
            
            if (sqlite3_prepare_v2(db, shop_sql, -1, &shop_stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(shop_stmt, 1, npc_id);
                
                while (sqlite3_step(shop_stmt) == SQLITE_ROW && shop_pkt.shop_data.item_count < 20) {
                    int idx = shop_pkt.shop_data.item_count;
                    shop_pkt.shop_data.items[idx].item_id = sqlite3_column_int(shop_stmt, 0);
                    strncpy(shop_pkt.shop_data.items[idx].name, (const char*)sqlite3_column_text(shop_stmt, 1), 31);
                    shop_pkt.shop_data.items[idx].name[31] = '\0';
                    shop_pkt.shop_data.items[idx].type = sqlite3_column_int(shop_stmt, 2);
                    strncpy(shop_pkt.shop_data.items[idx].icon, (const char*)sqlite3_column_text(shop_stmt, 3), 15);
                    shop_pkt.shop_data.items[idx].icon[15] = '\0';
                    shop_pkt.shop_data.items[idx].quantity = sqlite3_column_int(shop_stmt, 4);
                    shop_pkt.shop_data.buy_prices[idx] = sqlite3_column_int(shop_stmt, 5);
                    shop_pkt.shop_data.sell_prices[idx] = sqlite3_column_int(shop_stmt, 6);
                    shop_pkt.shop_data.item_count++;
                }
                
                sqlite3_finalize(shop_stmt);
            }
            
            send_all(client_sockets[index], &shop_pkt, sizeof(Packet), 0);
            LOG("Sent shop with %d items to player %s\n", shop_pkt.shop_data.item_count, players[index].username);
            
            // Check for quest progress (visit objectives)
            if (player_quest_counts[index] > 0) {
                update_quest_progress(index, npc_id);
            }
            
            return;
        }
        
        // If quest NPC, send available quests
        if (npc_type == NPC_TYPE_QUEST) {
            Packet quest_offer_pkt;
            memset(&quest_offer_pkt, 0, sizeof(Packet));
            quest_offer_pkt.type = PACKET_QUEST_LIST;
            quest_offer_pkt.npc_id = npc_id;
            
            // Load available quests from this NPC that player doesn't have
            sqlite3_stmt *quest_stmt;
            const char *quest_sql = "SELECT QUEST_ID, NAME, DESCRIPTION, REWARD_GOLD, REWARD_XP, REWARD_ITEM_ID, REWARD_ITEM_QTY, LEVEL_REQUIRED "
                                   "FROM quests WHERE NPC_ID=? LIMIT 10;";
            
            quest_offer_pkt.quest_count = 0;
            
            if (sqlite3_prepare_v2(db, quest_sql, -1, &quest_stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(quest_stmt, 1, npc_id);
                
                while (sqlite3_step(quest_stmt) == SQLITE_ROW && quest_offer_pkt.quest_count < 10) {
                    int qid = sqlite3_column_int(quest_stmt, 0);
                    
                    // Check if player already has this quest active
                    int has_quest = 0;
                    for (int i = 0; i < player_quest_counts[index]; i++) {
                        if (player_quests[index][i].quest_id == qid) {
                            has_quest = 1;
                            break;
                        }
                    }
                    
                    // Check if player already completed this quest
                    if (!has_quest) {
                        sqlite3_stmt *check_stmt;
                        const char *check_sql = "SELECT COUNT(*) FROM player_quests WHERE USER_ID=? AND QUEST_ID=? AND STATUS=1;";
                        if (sqlite3_prepare_v2(db, check_sql, -1, &check_stmt, NULL) == SQLITE_OK) {
                            sqlite3_bind_int(check_stmt, 1, players[index].id);
                            sqlite3_bind_int(check_stmt, 2, qid);
                            if (sqlite3_step(check_stmt) == SQLITE_ROW && sqlite3_column_int(check_stmt, 0) > 0) {
                                has_quest = 1; // Already completed
                            }
                            sqlite3_finalize(check_stmt);
                        }
                    }
                    
                    if (!has_quest) {
                        int idx = quest_offer_pkt.quest_count;
                        Quest *q = &quest_offer_pkt.quests[idx];
                        q->quest_id = qid;
                        strncpy(q->name, (const char*)sqlite3_column_text(quest_stmt, 1), 63);
                        strncpy(q->description, (const char*)sqlite3_column_text(quest_stmt, 2), 255);
                        q->npc_id = npc_id;
                        q->reward_gold = sqlite3_column_int(quest_stmt, 3);
                        q->reward_xp = sqlite3_column_int(quest_stmt, 4);
                        q->reward_item_id = sqlite3_column_int(quest_stmt, 5);
                        q->reward_item_qty = sqlite3_column_int(quest_stmt, 6);
                        q->level_required = sqlite3_column_int(quest_stmt, 7);
                        q->status = QUEST_STATUS_ACTIVE; // Mark as available
                        q->objective_count = 0;
                        
                        // Load objectives for display
                        sqlite3_stmt *obj_stmt;
                        const char *obj_sql = "SELECT OBJECTIVE_TYPE, TARGET_NAME, REQUIRED_COUNT FROM quest_objectives WHERE QUEST_ID=?;";
                        if (sqlite3_prepare_v2(db, obj_sql, -1, &obj_stmt, NULL) == SQLITE_OK) {
                            sqlite3_bind_int(obj_stmt, 1, qid);
                            while (sqlite3_step(obj_stmt) == SQLITE_ROW && q->objective_count < 5) {
                                q->objectives[q->objective_count].objective_type = sqlite3_column_int(obj_stmt, 0);
                                strncpy(q->objectives[q->objective_count].target_name, (const char*)sqlite3_column_text(obj_stmt, 1), 31);
                                q->objectives[q->objective_count].required_count = sqlite3_column_int(obj_stmt, 2);
                                q->objectives[q->objective_count].current_count = 0;
                                q->objective_count++;
                            }
                            sqlite3_finalize(obj_stmt);
                        }
                        
                        quest_offer_pkt.quest_count++;
                    }
                }
                sqlite3_finalize(quest_stmt);
            }
            
            send_all(client_sockets[index], &quest_offer_pkt, sizeof(Packet), 0);
            LOG("Sent %d available quests from NPC %d to player %s\n", quest_offer_pkt.quest_count, npc_id, players[index].username);
            
            // Check if player has any "visit" objectives for this NPC (only if they have quests loaded)
            if (player_quest_counts[index] > 0) {
                update_quest_progress(index, npc_id);
            }
            
            return;
        }
        
        // Otherwise, send dialogue
        sqlite3_stmt *stmt;
        const char *sql = "SELECT DIALOGUE_ID, NPC_ID, TEXT FROM dialogues WHERE NPC_ID=? LIMIT 1;";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, npc_id);
            
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                // Send dialogue to client
                Packet response;
                memset(&response, 0, sizeof(Packet));
                response.type = PACKET_DIALOGUE;
                response.dialogue.dialogue_id = sqlite3_column_int(stmt, 0);
                response.dialogue.npc_id = sqlite3_column_int(stmt, 1);
                strncpy(response.dialogue.text, (const char*)sqlite3_column_text(stmt, 2), 255);
                response.dialogue.text[255] = '\0';
                
                send_all(client_sockets[index], &response, sizeof(Packet), 0);
                LOG("Sent dialogue %d to player %s\n", response.dialogue.dialogue_id, players[index].username);
                
                // Check if player has any "visit" objectives for this NPC (only if they have quests loaded)
                if (player_quest_counts[index] > 0) {
                    update_quest_progress(index, npc_id);
                }
            } else {
                // No dialogue found, send generic message
                Packet response;
                memset(&response, 0, sizeof(Packet));
                response.type = PACKET_DIALOGUE;
                response.dialogue.npc_id = npc_id;
                snprintf(response.dialogue.text, 255, "Hello, adventurer!");
                
                send_all(client_sockets[index], &response, sizeof(Packet), 0);
                LOG("Sent generic dialogue to player %s\n", players[index].username);
            }
            
            sqlite3_finalize(stmt);
        }
    }
    else if (pkt->type == PACKET_SHOP_BUY) {
        // Player wants to buy an item
        int item_id = pkt->buy_sell_item_id;
        int quantity = pkt->buy_sell_quantity;
        int npc_id = pkt->npc_id;
        
        // Get item price from shop
        sqlite3_stmt *stmt;
        const char *sql = "SELECT BUY_PRICE FROM shop_items WHERE NPC_ID=? AND ITEM_ID=?;";
        int price = 0;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, npc_id);
            sqlite3_bind_int(stmt, 2, item_id);
            
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                price = sqlite3_column_int(stmt, 0) * quantity;
            }
            
            sqlite3_finalize(stmt);
        }
        
        if (price > 0 && players[index].gold >= price) {
            // Deduct gold
            players[index].gold -= price;
            
            // Update database
            char update_sql[256];
            snprintf(update_sql, 256, "UPDATE users SET GOLD=%d WHERE ID=%d;", 
                players[index].gold, players[index].id);
            sqlite3_exec(db, update_sql, 0, 0, 0);
            
            // Give item to player
            give_item_to_player(index, item_id, quantity);
            
            // Send inventory update to client
            send_inventory_update(index);
            
            // Send currency update
            send_player_stats(index);
            
            LOG("Player %s bought item %d for %d gold (remaining: %d)\n", 
                players[index].username, item_id, price, players[index].gold);
        } else {
            // Not enough gold or item not found
            Packet chat_pkt;
            memset(&chat_pkt, 0, sizeof(Packet));
            chat_pkt.type = PACKET_CHAT;
            chat_pkt.player_id = -1;
            snprintf(chat_pkt.msg, 64, "Not enough gold or item unavailable!");
            send_all(client_sockets[index], &chat_pkt, sizeof(Packet), 0);
            LOG("Player %s failed to buy item %d (price: %d, gold: %d)\n", 
                players[index].username, item_id, price, players[index].gold);
        }
    }
    else if (pkt->type == PACKET_SHOP_SELL) {
        // Player wants to sell an item
        int item_id = pkt->buy_sell_item_id;
        int quantity = pkt->buy_sell_quantity;
        int npc_id = pkt->npc_id;
        int slot = pkt->item_slot;
        
        // Get sell price from shop
        sqlite3_stmt *stmt;
        const char *sql = "SELECT SELL_PRICE FROM shop_items WHERE NPC_ID=? AND ITEM_ID=?;";
        int price = 0;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, npc_id);
            sqlite3_bind_int(stmt, 2, item_id);
            
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                price = sqlite3_column_int(stmt, 0) * quantity;
            } else {
                // Item not in shop, give minimum price
                price = 1 * quantity;
            }
            
            sqlite3_finalize(stmt);
        }
        
        // Check if player has the item
        int has_item = 0;
        for (int i = 0; i < players[index].inventory_count; i++) {
            if (players[index].inventory[i].item.item_id == item_id && 
                !players[index].inventory[i].is_equipped) {
                has_item = 1;
                break;
            }
        }
        
        if (has_item && price > 0) {
            // Remove item from player
            remove_item_from_player(index, item_id, quantity);
            
            // Add gold
            players[index].gold += price;
            
            // Update database
            char update_sql[256];
            snprintf(update_sql, 256, "UPDATE users SET GOLD=%d WHERE ID=%d;", 
                players[index].gold, players[index].id);
            sqlite3_exec(db, update_sql, 0, 0, 0);
            
            // Send inventory update to client
            send_inventory_update(index);
            
            // Send currency update
            send_player_stats(index);
            
            LOG("Player %s sold item %d for %d gold (total: %d)\n", 
                players[index].username, item_id, price, players[index].gold);
        } else {
            // Item not found
            Packet chat_pkt;
            memset(&chat_pkt, 0, sizeof(Packet));
            chat_pkt.type = PACKET_CHAT;
            chat_pkt.player_id = -1;
            snprintf(chat_pkt.msg, 64, "Item not found in inventory!");
            send_all(client_sockets[index], &chat_pkt, sizeof(Packet), 0);
        }
    }
    else if (pkt->type == PACKET_QUEST_ACCEPT) {
        // Player wants to accept a quest from an NPC
        int quest_id = pkt->quest_id;
        int npc_id = pkt->npc_id;
        
        // Check if quest exists and player meets level requirement
        sqlite3_stmt *stmt;
        const char *sql = "SELECT NAME, DESCRIPTION, NPC_ID, REWARD_GOLD, REWARD_XP, REWARD_ITEM_ID, REWARD_ITEM_QTY, LEVEL_REQUIRED "
                         "FROM quests WHERE QUEST_ID=? AND NPC_ID=?;";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, quest_id);
            sqlite3_bind_int(stmt, 2, npc_id);
            
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int level_req = sqlite3_column_int(stmt, 7);
                
                if (players[index].level >= level_req) {
                    // Check if player already has this quest
                    int already_has = 0;
                    for (int i = 0; i < player_quest_counts[index]; i++) {
                        if (player_quests[index][i].quest_id == quest_id) {
                            already_has = 1;
                            break;
                        }
                    }
                    
                    if (!already_has && player_quest_counts[index] < 10) {
                        // Add quest to player_quests table
                        char insert_sql[512];
                        snprintf(insert_sql, 512, 
                                "INSERT INTO player_quests (USER_ID, QUEST_ID, STATUS, PROGRESS, ACCEPTED_TIME) "
                                "VALUES (%d, %d, 0, '', %ld);",
                                players[index].id, quest_id, (long)time(NULL));
                        sqlite3_exec(db, insert_sql, 0, 0, 0);
                        
                        // Reload quests for this player
                        load_player_quests(index);
                        send_quest_list(index);
                        
                        // Send confirmation message
                        Packet chat_pkt;
                        memset(&chat_pkt, 0, sizeof(Packet));
                        chat_pkt.type = PACKET_CHAT;
                        chat_pkt.player_id = -1;
                        snprintf(chat_pkt.msg, 64, "Quest accepted: %s", (const char*)sqlite3_column_text(stmt, 0));
                        send_all(client_sockets[index], &chat_pkt, sizeof(Packet), 0);
                        
                        LOG("Player %s accepted quest %d\n", players[index].username, quest_id);
                    } else {
                        Packet chat_pkt;
                        memset(&chat_pkt, 0, sizeof(Packet));
                        chat_pkt.type = PACKET_CHAT;
                        chat_pkt.player_id = -1;
                        strcpy(chat_pkt.msg, already_has ? "You already have this quest!" : "Quest log is full!");
                        send_all(client_sockets[index], &chat_pkt, sizeof(Packet), 0);
                    }
                } else {
                    Packet chat_pkt;
                    memset(&chat_pkt, 0, sizeof(Packet));
                    chat_pkt.type = PACKET_CHAT;
                    chat_pkt.player_id = -1;
                    snprintf(chat_pkt.msg, 64, "Level %d required!", level_req);
                    send_all(client_sockets[index], &chat_pkt, sizeof(Packet), 0);
                }
            }
            sqlite3_finalize(stmt);
        }
    }
    else if (pkt->type == PACKET_QUEST_COMPLETE) {
        // Player wants to complete a quest
        int quest_id = pkt->quest_id;
        
        // Find quest in player's active quests
        Quest *quest = NULL;
        int quest_idx = -1;
        for (int i = 0; i < player_quest_counts[index]; i++) {
            if (player_quests[index][i].quest_id == quest_id) {
                quest = &player_quests[index][i];
                quest_idx = i;
                break;
            }
        }
        
        if (quest) {
            // Check if all objectives are completed
            int all_complete = 1;
            LOG("Checking quest %d completion for player %s:\n", quest_id, players[index].username);
            for (int i = 0; i < quest->objective_count; i++) {
                LOG("  Objective %d: %d/%d\n", i, quest->objectives[i].current_count, quest->objectives[i].required_count);
                if (quest->objectives[i].current_count < quest->objectives[i].required_count) {
                    all_complete = 0;
                    break;
                }
            }
            
            if (all_complete) {
                // Give rewards
                players[index].gold += quest->reward_gold;
                players[index].xp += quest->reward_xp;
                
                // Check for level up (simple: every 100 XP = 1 level)
                int new_level = 1 + (players[index].xp / 100);
                if (new_level > players[index].level) {
                    players[index].level = new_level;
                }
                
                if (quest->reward_item_id > 0) {
                    give_item_to_player(index, quest->reward_item_id, quest->reward_item_qty);
                    send_inventory_update(index);
                }
                
                // Update database
                char update_sql[512];
                snprintf(update_sql, 512, 
                        "UPDATE player_quests SET STATUS=1, COMPLETED_TIME=%ld WHERE USER_ID=%d AND QUEST_ID=%d;",
                        (long)time(NULL), players[index].id, quest_id);
                sqlite3_exec(db, update_sql, 0, 0, 0);
                
                snprintf(update_sql, 512, "UPDATE users SET GOLD=%d, XP=%d, LEVEL=%d WHERE ID=%d;",
                        players[index].gold, players[index].xp, players[index].level, players[index].id);
                sqlite3_exec(db, update_sql, 0, 0, 0);
                
                // Remove quest from active list
                for (int i = quest_idx; i < player_quest_counts[index] - 1; i++) {
                    player_quests[index][i] = player_quests[index][i + 1];
                }
                player_quest_counts[index]--;
                
                // Send updates
                send_quest_list(index);
                
                send_player_stats(index);
                
                Packet chat_pkt;
                memset(&chat_pkt, 0, sizeof(Packet));
                chat_pkt.type = PACKET_CHAT;
                chat_pkt.player_id = -1;
                snprintf(chat_pkt.msg, 64, "Quest Complete! +%dg +%dxp", quest->reward_gold, quest->reward_xp);
                send_all(client_sockets[index], &chat_pkt, sizeof(Packet), 0);
                
                LOG("Player %s completed quest %d\n", players[index].username, quest_id);
            } else {
                Packet chat_pkt;
                memset(&chat_pkt, 0, sizeof(Packet));
                chat_pkt.type = PACKET_CHAT;
                chat_pkt.player_id = -1;
                strcpy(chat_pkt.msg, "Quest objectives not complete!");
                send_all(client_sockets[index], &chat_pkt, sizeof(Packet), 0);
            }
        }
    }
    else if (pkt->type == PACKET_QUEST_ABANDON) {
        // Player wants to abandon a quest
        int quest_id = pkt->quest_id;
        
        // Remove from database
        char sql[256];
        snprintf(sql, 256, "DELETE FROM player_quests WHERE USER_ID=%d AND QUEST_ID=%d;",
                players[index].id, quest_id);
        sqlite3_exec(db, sql, 0, 0, 0);
        
        // Reload quests
        load_player_quests(index);
        send_quest_list(index);
        
        Packet chat_pkt;
        memset(&chat_pkt, 0, sizeof(Packet));
        chat_pkt.type = PACKET_CHAT;
        chat_pkt.player_id = -1;
        strcpy(chat_pkt.msg, "Quest abandoned.");
        send_all(client_sockets[index], &chat_pkt, sizeof(Packet), 0);
        
        LOG("Player %s abandoned quest %d\n", players[index].username, quest_id);
    }
    else if (pkt->type == PACKET_QUEST_LIST && pkt->quest_id == 0 && pkt->npc_id == 0) {
        // Client requesting their quest list (not from NPC interaction)
        send_quest_list(index);
        LOG("Sent quest list to player %s\n", players[index].username);
    }
    else if (pkt->type == PACKET_ENEMY_ATTACK) {
        // Player attacking an enemy
        int enemy_id = pkt->enemy_id;
        
        // Find the enemy
        Enemy *enemy = NULL;
        int enemy_idx = -1;
        for (int i = 0; i < server_enemy_count; i++) {
            if (server_enemies[i].enemy_id == enemy_id && server_enemies[i].active) {
                enemy = &server_enemies[i];
                enemy_idx = i;
                break;
            }
        }
        
        if (enemy) {
            // Attack speed check (1 second cooldown = 1000ms)
            uint32_t now = (uint32_t)time(NULL);
            if (now - players[index].last_attack_time < 1) {
                // Too fast (using simple 1s cooldown for now)
                return;
            }
            players[index].last_attack_time = now;

            // Damage calculation: (Str / 2) + random(1-5)
            int dmg = (players[index].str / 2) + (rand() % 5) + 1;
            dmg -= enemy->defense;
            if (dmg < 1) dmg = 1;

            enemy->hp -= dmg;
            
            // Log attack
            LOG("Player %s hit %s for %d damage (HP: %d/%d)\n", 
                players[index].username, enemy->name, dmg, enemy->hp, enemy->max_hp);

            // Broadcast damage
            Packet dmg_pkt;
            memset(&dmg_pkt, 0, sizeof(Packet));
            dmg_pkt.type = PACKET_DAMAGE;
            dmg_pkt.damage = dmg;
            dmg_pkt.dx = enemy->x;
            dmg_pkt.dy = enemy->y;
            dmg_pkt.player_id = players[index].id;
            dmg_pkt.enemy_id = enemy->enemy_id;
            
            // Broadcast attack animation trigger
            Packet atk_pkt;
            memset(&atk_pkt, 0, sizeof(Packet));
            atk_pkt.type = PACKET_ENEMY_ATTACK;
            atk_pkt.player_id = players[index].id;
            atk_pkt.enemy_id = enemy->enemy_id;

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (SOCKET_IS_VALID(client_sockets[i]) && players[i].active) {
                    send_all(client_sockets[i], &dmg_pkt, sizeof(Packet), 0);
                    send_all(client_sockets[i], &atk_pkt, sizeof(Packet), 0);
                }
            }

            if (enemy->hp <= 0) {
                // Enemy killed
                enemy->active = 0;
                enemy->hp = 0;
                
                // Update kill quest progress
                update_kill_quest_progress(index, enemy->type);
                
                // Give XP
                players[index].xp += 10;
                int new_level = 1 + (players[index].xp / 100);
                if (new_level > players[index].level) {
                    players[index].level = new_level;
                    
                    // Stat increases on level up
                    players[index].str += 2;
                    players[index].agi += 2;
                    players[index].intel += 2;
                    
                    // Update HP/Mana based on new level
                    int base_hp = 100;
                    int base_mana = 50;
                    players[index].max_hp = base_hp + (new_level - 1) * 10;
                    players[index].max_mana = base_mana + (new_level - 1) * 5;
                    players[index].hp = players[index].max_hp;  // Heal on level up
                    players[index].skill_points += 5;
                    
                    Packet chat_pkt;
                    memset(&chat_pkt, 0, sizeof(Packet));
                    chat_pkt.type = PACKET_CHAT;
                    chat_pkt.player_id = -1;
                    snprintf(chat_pkt.msg, 64, "Level UP! Level %d. +5 Skill Points!", players[index].level);
                    send_all(client_sockets[index], &chat_pkt, sizeof(Packet), 0);
                    send_player_stats(index);
                }
                
                // Update database
                char sql[512];
                snprintf(sql, 512, "UPDATE users SET XP=%d, LEVEL=%d, STR=%d, AGI=%d, INT=%d, SKILL_POINTS=%d WHERE ID=%d;",
                        players[index].xp, players[index].level, players[index].str, players[index].agi, players[index].intel, players[index].skill_points, players[index].id);
                sqlite3_exec(db, sql, 0, 0, 0);
                
                send_player_stats(index);
                
                LOG("Player %s killed %s (enemy %d)\n", players[index].username, enemy->name, enemy_id);
                
                // Drop loot (random chance for item drop)
                if ((rand() % 100) < 30) {  // 30% chance to drop loot
                    if (ground_item_count < MAX_GROUND_ITEMS) {
                        int loot_item_id = 7 + (rand() % 3);  // Random material item (7-9)
                        ground_items[ground_item_count].item_id = loot_item_id;
                        ground_items[ground_item_count].x = enemy->x;
                        ground_items[ground_item_count].y = enemy->y;
                        ground_items[ground_item_count].quantity = 1 + (rand() % 2);  // 1-2 items
                        strcpy(ground_items[ground_item_count].map_name, enemy->map_name);
                        ground_items[ground_item_count].spawn_time = time(NULL);
                        
                        // Get item name from database
                        char item_sql[256];
                        snprintf(item_sql, 256, "SELECT NAME FROM items WHERE ITEM_ID=%d;", loot_item_id);
                        sqlite3_stmt *item_stmt;
                        if (sqlite3_prepare_v2(db, item_sql, -1, &item_stmt, NULL) == SQLITE_OK) {
                            if (sqlite3_step(item_stmt) == SQLITE_ROW) {
                                strncpy(ground_items[ground_item_count].name, (const char*)sqlite3_column_text(item_stmt, 0), 31);
                                ground_items[ground_item_count].name[31] = '\0';
                            } else {
                                strcpy(ground_items[ground_item_count].name, "Loot");
                            }
                            sqlite3_finalize(item_stmt);
                        } else {
                            strcpy(ground_items[ground_item_count].name, "Loot");
                        }
                        
                        ground_item_count++;
                        broadcast_ground_items();
                        
                        LOG("Enemy %s dropped item %d at (%.1f, %.1f)\n", 
                            enemy->name, loot_item_id, enemy->x, enemy->y);
                    }
                }
            }
            
            // Broadcast updated enemy list to all players
            broadcast_enemy_list();
        }
    }
    else if (pkt->type == PACKET_PVP_TOGGLE) {
        players[index].pvp_enabled = pkt->pvp_enabled;
        char sql[256];
        snprintf(sql, 256, "UPDATE users SET PVP_ENABLED=%d WHERE ID=%d;", players[index].pvp_enabled, players[index].id);
        sqlite3_exec(db, sql, 0, 0, 0);
        
        Packet resp; resp.type = PACKET_CHAT; resp.player_id = -1;
        snprintf(resp.msg, 127, "PvP %s", players[index].pvp_enabled ? "ENABLED" : "DISABLED");
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
        
        LOG("Player %s (Packet) toggled PvP to %d\n", players[index].username, players[index].pvp_enabled);
    }
    else if (pkt->type == PACKET_ALLOCATE_STATS) {
        if (players[index].skill_points > 0) {
            int stat = pkt->stat_type;
            if (stat == 0) players[index].str++;
            else if (stat == 1) players[index].agi++;
            else if (stat == 2) players[index].intel++;
            
            players[index].skill_points--;
            
            // Re-calculate HP/Mana caps based on new level if level increases (handled in combat)
            // Primary stat updates sync here.
            
            // Update database
            char sql[256];
            snprintf(sql, 256, "UPDATE users SET STR=%d, AGI=%d, INT=%d, SKILL_POINTS=%d WHERE ID=%d;",
                players[index].str, players[index].agi, players[index].intel, players[index].skill_points, players[index].id);
            sqlite3_exec(db, sql, 0, 0, 0);
            
            // Sync to client
            send_player_stats(index);
            
            LOG("Player %s allocated skill point to %s (Points left: %d)\n",
                players[index].username, (stat==0?"STR":(stat==1?"AGI":"INT")), players[index].skill_points);
        }
    }
    else if (pkt->type == PACKET_ATTACK) {
        // Player attacking another player
        int target_id = pkt->target_id;
        int target_idx = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (players[i].active && players[i].id == target_id) {
                target_idx = i;
                break;
            }
        }

        if (target_idx != -1 && target_idx != index) {
            // PVP CHECK
            if (!players[index].pvp_enabled) {
                Packet chat_pkt; memset(&chat_pkt, 0, sizeof(Packet));
                chat_pkt.type = PACKET_CHAT; chat_pkt.player_id = -1;
                strcpy(chat_pkt.msg, "You must enable PvP to attack other players (/pvp).");
                send_all(client_sockets[index], &chat_pkt, sizeof(Packet), 0);
            } else if (!players[target_idx].pvp_enabled) {
                Packet chat_pkt; memset(&chat_pkt, 0, sizeof(Packet));
                chat_pkt.type = PACKET_CHAT; chat_pkt.player_id = -1;
                snprintf(chat_pkt.msg, 127, "Target %s has PvP disabled.", players[target_idx].username);
                send_all(client_sockets[index], &chat_pkt, sizeof(Packet), 0);
            } else {
                // Both have PvP enabled
                uint32_t now = time(NULL);
                if (now - players[index].last_attack_time >= 1) { // 1s cooldown
                    players[index].last_attack_time = now;
                    
                    int dmg = 5 + (players[index].str / 3) + (rand() % 5);
                    players[target_idx].hp -= dmg;
                    if (players[target_idx].hp < 0) players[target_idx].hp = 0;
                    
                    LOG("PvP: %s hit %s for %d damage (HP: %d/%d)\n",
                        players[index].username, players[target_idx].username, dmg, players[target_idx].hp, players[target_idx].max_hp);
                    
                    // Broadcast damage
                    Packet dmg_pkt; memset(&dmg_pkt, 0, sizeof(Packet));
                    dmg_pkt.type = PACKET_DAMAGE;
                    dmg_pkt.damage = dmg;
                    dmg_pkt.dx = players[target_idx].x;
                    dmg_pkt.dy = players[target_idx].y;
                    dmg_pkt.player_id = players[target_idx].id; // Target
                    
                    // Broadcast attack animation trigger
                    Packet atk_pkt; memset(&atk_pkt, 0, sizeof(Packet));
                    atk_pkt.type = PACKET_ATTACK;
                    atk_pkt.player_id = players[index].id; // Attacker
                    atk_pkt.target_id = target_id; // Victim
                    
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (players[i].active) {
                            send_all(client_sockets[i], &dmg_pkt, sizeof(Packet), 0);
                            send_all(client_sockets[i], &atk_pkt, sizeof(Packet), 0);
                        }
                    }
                    
                    send_player_stats(target_idx); // Sync HP to target and others
                    
                    if (players[target_idx].hp == 0) {
                        Packet chat_pkt; memset(&chat_pkt, 0, sizeof(Packet));
                        chat_pkt.type = PACKET_CHAT; chat_pkt.player_id = -1;
                        snprintf(chat_pkt.msg, 127, "Player %s was killed by %s!", players[target_idx].username, players[index].username);
                        for (int i = 0; i < MAX_CLIENTS; i++) {
                            if (players[i].active) send_all(client_sockets[i], &chat_pkt, sizeof(Packet), 0);
                        }
                        
                        // Respawn logic (simplified)
                        players[target_idx].hp = players[target_idx].max_hp / 2;
                        players[target_idx].x = 400; players[target_idx].y = 300; // Generic spawn
                        send_player_stats(target_idx);
                    }
                }
            }
        }
    }
    // === PLAYER TRADING ===
    else if (pkt->type == PACKET_TRADE_REQUEST) {
        // Player wants to trade with another player
        int target_id = pkt->trade_partner_id;
        
        // Find target player index
        int target_idx = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (players[i].active && players[i].id == target_id) {
                target_idx = i;
                break;
            }
        }
        
        if (target_idx >= 0 && player_trades[target_idx].partner_id == -1 && 
            player_trades[index].partner_id == -1) {
            // Send trade request to target
            Packet req_pkt;
            memset(&req_pkt, 0, sizeof(Packet));
            req_pkt.type = PACKET_TRADE_REQUEST;
            req_pkt.player_id = players[index].id;
            strncpy(req_pkt.username, players[index].username, 63);
            send_all(client_sockets[target_idx], &req_pkt, sizeof(Packet), 0);
            
            LOG("Player %s sent trade request to %s\n", players[index].username, players[target_idx].username);
        } else {
            // Target busy or invalid
            Packet chat_pkt;
            memset(&chat_pkt, 0, sizeof(Packet));
            chat_pkt.type = PACKET_CHAT;
            chat_pkt.player_id = -1;
            strcpy(chat_pkt.msg, "Player is busy or unavailable for trading.");
            send_all(client_sockets[index], &chat_pkt, sizeof(Packet), 0);
        }
    }
    else if (pkt->type == PACKET_TRADE_RESPONSE) {
        // Player accepts or rejects trade request
        int requester_id = pkt->trade_partner_id;
        int accepted = pkt->response_accepted;
        
        // Find requester
        int requester_idx = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (players[i].active && players[i].id == requester_id) {
                requester_idx = i;
                break;
            }
        }
        
        if (requester_idx >= 0) {
            if (accepted) {
                // Start trade session
                player_trades[index].partner_id = requester_id;
                player_trades[index].offered_count = 0;
                player_trades[index].offered_gold = 0;
                player_trades[index].confirmed = 0;
                memset(player_trades[index].offered_items, 0, sizeof(player_trades[index].offered_items));
                
                player_trades[requester_idx].partner_id = players[index].id;
                player_trades[requester_idx].offered_count = 0;
                player_trades[requester_idx].offered_gold = 0;
                player_trades[requester_idx].confirmed = 0;
                memset(player_trades[requester_idx].offered_items, 0, sizeof(player_trades[requester_idx].offered_items));
                
                // Notify both players
                Packet start_pkt;
                memset(&start_pkt, 0, sizeof(Packet));
                start_pkt.type = PACKET_TRADE_RESPONSE;
                start_pkt.response_accepted = 1;
                start_pkt.trade_partner_id = players[index].id;
                strncpy(start_pkt.username, players[index].username, 63);
                send_all(client_sockets[requester_idx], &start_pkt, sizeof(Packet), 0);
                
                start_pkt.trade_partner_id = requester_id;
                strncpy(start_pkt.username, players[requester_idx].username, 63);
                send_all(client_sockets[index], &start_pkt, sizeof(Packet), 0);
                
                LOG("Trade started between %s and %s\n", players[index].username, players[requester_idx].username);
            } else {
                // Reject trade
                Packet reject_pkt;
                memset(&reject_pkt, 0, sizeof(Packet));
                reject_pkt.type = PACKET_TRADE_RESPONSE;
                reject_pkt.response_accepted = 0;
                send_all(client_sockets[requester_idx], &reject_pkt, sizeof(Packet), 0);
                
                LOG("Player %s rejected trade from %s\n", players[index].username, players[requester_idx].username);
            }
        }
    }
    else if (pkt->type == PACKET_TRADE_OFFER) {
        // Player updates their trade offer
        if (player_trades[index].partner_id != -1) {
            // Find partner
            int partner_idx = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (players[i].active && players[i].id == player_trades[index].partner_id) {
                    partner_idx = i;
                    break;
                }
            }
            
            // Update offer
            int offered_gold = pkt->trade_offer_gold;
            if (offered_gold > players[index].gold) offered_gold = players[index].gold;
            if (offered_gold < 0) offered_gold = 0;
            
            player_trades[index].offered_count = pkt->trade_offer_count;
            player_trades[index].offered_gold = offered_gold;
            for (int i = 0; i < pkt->trade_offer_count && i < 10; i++) {
                player_trades[index].offered_items[i] = pkt->trade_offer_items[i];
            }
            
            // Reset confirmations when offer changes
            player_trades[index].confirmed = 0;
            if (partner_idx >= 0) player_trades[partner_idx].confirmed = 0;
            
            // Send update to partner
            if (partner_idx >= 0) {
                Packet offer_pkt;
                memset(&offer_pkt, 0, sizeof(Packet));
                offer_pkt.type = PACKET_TRADE_OFFER;
                offer_pkt.trade_offer_count = player_trades[index].offered_count;
                offer_pkt.trade_offer_gold = player_trades[index].offered_gold;
                for (int i = 0; i < player_trades[index].offered_count && i < 10; i++) {
                    offer_pkt.trade_offer_items[i] = player_trades[index].offered_items[i];
                }
                send_all(client_sockets[partner_idx], &offer_pkt, sizeof(Packet), 0);
            }
        }
    }
    else if (pkt->type == PACKET_TRADE_CONFIRM) {
        // Player confirms trade
        if (player_trades[index].partner_id != -1) {
            player_trades[index].confirmed = 1;
            
            // Find partner
            int partner_idx = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (players[i].active && players[i].id == player_trades[index].partner_id) {
                    partner_idx = i;
                    break;
                }
            }
            
            // Notify partner of confirmation
            if (partner_idx >= 0) {
                Packet confirm_pkt;
                memset(&confirm_pkt, 0, sizeof(Packet));
                confirm_pkt.type = PACKET_TRADE_CONFIRM;
                confirm_pkt.trade_confirmed = 1;
                send_all(client_sockets[partner_idx], &confirm_pkt, sizeof(Packet), 0);
                
                // Check if both confirmed - execute trade!
                if (player_trades[partner_idx].confirmed) {
                    // Final gold verification
                    if (players[index].gold < player_trades[index].offered_gold || 
                        players[partner_idx].gold < player_trades[partner_idx].offered_gold) {
                        
                        LOG("Trade failed: Insufficient gold at execution time for %s or %s\n", 
                            players[index].username, players[partner_idx].username);
                        
                        // Cancel trade due to invalid state
                        player_trades[index].partner_id = -1;
                        player_trades[partner_idx].partner_id = -1;
                        
                        Packet cancel_pkt;
                        memset(&cancel_pkt, 0, sizeof(Packet));
                        cancel_pkt.type = PACKET_TRADE_CANCEL;
                        cancel_pkt.response_accepted = 0;
                        send_all(client_sockets[index], &cancel_pkt, sizeof(Packet), 0);
                        send_all(client_sockets[partner_idx], &cancel_pkt, sizeof(Packet), 0);
                        return;
                    }
                    
                    // Transfer items
                    for (int i = 0; i < player_trades[index].offered_count; i++) {
                        Item *item = &player_trades[index].offered_items[i];
                        if (item->item_id > 0) {
                            remove_item_from_player(index, item->item_id, item->quantity);
                            give_item_to_player(partner_idx, item->item_id, item->quantity);
                        }
                    }
                    for (int i = 0; i < player_trades[partner_idx].offered_count; i++) {
                        Item *item = &player_trades[partner_idx].offered_items[i];
                        if (item->item_id > 0) {
                            remove_item_from_player(partner_idx, item->item_id, item->quantity);
                            give_item_to_player(index, item->item_id, item->quantity);
                        }
                    }
                    
                    // Transfer gold
                    players[index].gold -= player_trades[index].offered_gold;
                    players[partner_idx].gold += player_trades[index].offered_gold;
                    players[partner_idx].gold -= player_trades[partner_idx].offered_gold;
                    players[index].gold += player_trades[partner_idx].offered_gold;
                    
                    // Reset trade state
                    player_trades[index].partner_id = -1;
                    player_trades[partner_idx].partner_id = -1;
                    
                    // Send updates
                    send_inventory_update(index);
                    send_inventory_update(partner_idx);
                    
                    send_player_stats(index);
                    send_player_stats(partner_idx);
                    
                    
                    // Notify trade complete
                    Packet complete_pkt;
                    memset(&complete_pkt, 0, sizeof(Packet));
                    complete_pkt.type = PACKET_TRADE_CANCEL;  // Reuse to close window
                    complete_pkt.response_accepted = 1;  // 1 = success, 0 = cancelled
                    send_all(client_sockets[index], &complete_pkt, sizeof(Packet), 0);
                    send_all(client_sockets[partner_idx], &complete_pkt, sizeof(Packet), 0);
                    
                    LOG("Trade completed between %s and %s\n", players[index].username, players[partner_idx].username);
                }
            }
        }
    }
    else if (pkt->type == PACKET_TRADE_CANCEL) {
        // Cancel trade
        if (player_trades[index].partner_id != -1) {
            int partner_idx = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (players[i].active && players[i].id == player_trades[index].partner_id) {
                    partner_idx = i;
                    break;
                }
            }
            
            player_trades[index].partner_id = -1;
            if (partner_idx >= 0) {
                player_trades[partner_idx].partner_id = -1;
                
                Packet cancel_pkt;
                memset(&cancel_pkt, 0, sizeof(Packet));
                cancel_pkt.type = PACKET_TRADE_CANCEL;
                cancel_pkt.response_accepted = 0;  // 0 = cancelled
                send_all(client_sockets[partner_idx], &cancel_pkt, sizeof(Packet), 0);
            }
            
            LOG("Trade cancelled by %s\n", players[index].username);
        }
    }
    // === PARTY SYSTEM ===
    else if (pkt->type == PACKET_PARTY_INVITE) {
        int target_id = pkt->invitee_id;
        int target_idx = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (players[i].active && players[i].id == target_id) {
                target_idx = i;
                break;
            }
        }

        if (target_idx != -1 && target_idx != index) {
            if (players[target_idx].party_id != -1) {
                Packet err; memset(&err, 0, sizeof(Packet));
                err.type = PACKET_CHAT; err.player_id = -1;
                strcpy(err.msg, "Player is already in a party.");
                send_all(client_sockets[index], &err, sizeof(Packet), 0);
            } else {
                Packet inv; memset(&inv, 0, sizeof(Packet));
                inv.type = PACKET_PARTY_INVITE;
                inv.player_id = players[index].id;
                strncpy(inv.username, players[index].username, 63);
                send_all(client_sockets[target_idx], &inv, sizeof(Packet), 0);
                LOG("Player %s invited %s to party\n", players[index].username, players[target_idx].username);
            }
        }
    }
    else if (pkt->type == PACKET_PARTY_ACCEPT) {
        int leader_id = pkt->player_id;
        int leader_idx = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (players[i].active && players[i].id == leader_id) {
                leader_idx = i;
                break;
            }
        }

        if (leader_idx != -1) {
            // If leader is not in a party, create one with themselves as leader
            if (players[leader_idx].party_id == -1) {
                players[leader_idx].party_id = leader_id;
            }
            
            // Join the party
            players[index].party_id = players[leader_idx].party_id;
            LOG("Player %s joined %s's party\n", players[index].username, players[leader_idx].username);
            
            // Save party status to DB for both
            char p_sql[128];
            snprintf(p_sql, 128, "UPDATE users SET PARTY_ID=%d WHERE ID=%d;", players[leader_idx].party_id, players[leader_idx].id);
            sqlite3_exec(db, p_sql, 0, 0, 0);
            snprintf(p_sql, 128, "UPDATE users SET PARTY_ID=%d WHERE ID=%d;", players[index].party_id, players[index].id);
            sqlite3_exec(db, p_sql, 0, 0, 0);

            // Sync party state to all members
            broadcast_party_update(players[index].party_id);
        }
    }
    else if (pkt->type == PACKET_PARTY_LEAVE) {
        int pid = players[index].party_id;
        if (pid != -1) {
            players[index].party_id = -1;
            LOG("Player %s left party %d\n", players[index].username, pid);
            
            // Save to DB
            char l_sql[128];
            snprintf(l_sql, 128, "UPDATE users SET PARTY_ID=-1 WHERE ID=%d;", players[index].id);
            sqlite3_exec(db, l_sql, 0, 0, 0);

            broadcast_party_update(pid);
            
            // Notify the client themselves they left
            Packet leave_pkt; memset(&leave_pkt, 0, sizeof(Packet));
            leave_pkt.type = PACKET_PARTY_LEAVE;
            send_all(client_sockets[index], &leave_pkt, sizeof(Packet), 0);
        }
    }
    // === CLAN SYSTEM ===
    else if (pkt->type == PACKET_CLAN_CREATE_REQUEST) {
        if (players[index].clan_id != -1) {
            Packet err; memset(&err, 0, sizeof(Packet));
            err.type = PACKET_CHAT; err.player_id = -1;
            strcpy(err.msg, "You are already in a clan.");
            send_all(client_sockets[index], &err, sizeof(Packet), 0);
        } else if (players[index].gold < 5000) {
            Packet err; memset(&err, 0, sizeof(Packet));
            err.type = PACKET_CHAT; err.player_id = -1;
            strcpy(err.msg, "You need 5000 gold to create a clan.");
            send_all(client_sockets[index], &err, sizeof(Packet), 0);
        } else {
            // Check for unique name
            sqlite3_stmt *stmt;
            const char *sql_check = "SELECT COUNT(*) FROM clans WHERE NAME = ?;";
            int exists = 0;
            if (sqlite3_prepare_v2(db, sql_check, -1, &stmt, 0) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, pkt->clan_name, -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) == SQLITE_ROW) exists = sqlite3_column_int(stmt, 0);
                sqlite3_finalize(stmt);
            }

            if (exists) {
                Packet err; memset(&err, 0, sizeof(Packet));
                err.type = PACKET_CHAT; err.player_id = -1;
                strcpy(err.msg, "Clan name already exists.");
                send_all(client_sockets[index], &err, sizeof(Packet), 0);
            } else {
                // Deduct gold
                players[index].gold -= 5000;
                send_player_stats(index);

                // Create clan - Deposit the 5000 gold creation cost into storage!
                char sql_create[512];
                snprintf(sql_create, 512, "INSERT INTO clans (NAME, OWNER_ID, GOLD_STORAGE, CREATED_AT) VALUES ('%s', %d, 5000, %lld);",
                         pkt->clan_name, players[index].id, (long long)time(NULL));
                sqlite3_exec(db, sql_create, 0, 0, 0);
                
                int new_clan_id = (int)sqlite3_last_insert_rowid(db);
                players[index].clan_id = new_clan_id;
                players[index].clan_role = 1; // Owner

                // Update player in DB
                char u_sql[128];
                snprintf(u_sql, 128, "UPDATE users SET CLAN_ID=%d, CLAN_ROLE=1, GOLD=%d WHERE ID=%d;", 
                         new_clan_id, players[index].gold, players[index].id);
                sqlite3_exec(db, u_sql, 0, 0, 0);

                LOG("Clan '%s' created by %s (ID: %d)\n", pkt->clan_name, players[index].username, new_clan_id);
                broadcast_clan_update(new_clan_id);
            }
        }
    }
    else if (pkt->type == PACKET_CLAN_INVITE) {
        if (players[index].clan_role != 1) {
            Packet err; memset(&err, 0, sizeof(Packet));
            err.type = PACKET_CHAT; err.player_id = -1;
            strcpy(err.msg, "Only clan owners can invite members.");
            send_all(client_sockets[index], &err, sizeof(Packet), 0);
            return;
        }

        int target_id = pkt->target_id;
        int target_idx = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (players[i].active && players[i].id == target_id) {
                target_idx = i;
                break;
            }
        }

        if (target_idx != -1) {
            if (players[target_idx].clan_id != -1) {
                Packet err; memset(&err, 0, sizeof(Packet));
                err.type = PACKET_CHAT; err.player_id = -1;
                strcpy(err.msg, "Player is already in a clan.");
                send_all(client_sockets[index], &err, sizeof(Packet), 0);
            } else {
                Packet inv; memset(&inv, 0, sizeof(Packet));
                inv.type = PACKET_CLAN_INVITE;
                inv.clan_id = players[index].clan_id;
                // Fetch clan name
                sqlite3_stmt *stmt;
                const char *sql_n = "SELECT NAME FROM clans WHERE ID = ?;";
                if (sqlite3_prepare_v2(db, sql_n, -1, &stmt, 0) == SQLITE_OK) {
                    sqlite3_bind_int(stmt, 1, players[index].clan_id);
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        strncpy(inv.clan_name, (const char*)sqlite3_column_text(stmt, 0), 31);
                    }
                    sqlite3_finalize(stmt);
                }
                inv.player_id = players[index].id;
                strncpy(inv.username, players[index].username, 63);
                send_all(client_sockets[target_idx], &inv, sizeof(Packet), 0);
                LOG("Player %s invited %s to clan %s\n", players[index].username, players[target_idx].username, inv.clan_name);
            }
        }
    }
    else if (pkt->type == PACKET_CLAN_ACCEPT) {
        if (players[index].gold < 500) {
            Packet err; memset(&err, 0, sizeof(Packet));
            err.type = PACKET_CHAT; err.player_id = -1;
            strcpy(err.msg, "You need 500 gold to join a clan.");
            send_all(client_sockets[index], &err, sizeof(Packet), 0);
            return;
        }

        int clan_id = pkt->clan_id;
        // Verify clan exists
        sqlite3_stmt *stmt;
        const char *sql_c = "SELECT ID FROM clans WHERE ID = ?;";
        int exists = 0;
        if (sqlite3_prepare_v2(db, sql_c, -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, clan_id);
            if (sqlite3_step(stmt) == SQLITE_ROW) exists = 1;
            sqlite3_finalize(stmt);
        }

        if (exists) {
            players[index].gold -= 500;
            players[index].clan_id = clan_id;
            players[index].clan_role = 0; // Member

            // Update user
            char u_sql[128];
            snprintf(u_sql, 128, "UPDATE users SET CLAN_ID=%d, CLAN_ROLE=0, GOLD=%d WHERE ID=%d;", clan_id, players[index].gold, players[index].id);
            sqlite3_exec(db, u_sql, 0, 0, 0);

            // Add join fee to clan storage
            char s_sql[128];
            snprintf(s_sql, 128, "UPDATE clans SET GOLD_STORAGE = GOLD_STORAGE + 500 WHERE ID=%d;", clan_id);
            sqlite3_exec(db, s_sql, 0, 0, 0);

            LOG("Player %s joined clan %d\n", players[index].username, clan_id);
            send_player_stats(index);
            broadcast_clan_update(clan_id);
        }
    }
    else if (pkt->type == PACKET_CLAN_LEAVE) {
        int cid = players[index].clan_id;
        if (cid != -1) {
            players[index].clan_id = -1;
            players[index].clan_role = 0;
            
            char l_sql[128];
            snprintf(l_sql, 128, "UPDATE users SET CLAN_ID=-1, CLAN_ROLE=0 WHERE ID=%d;", players[index].id);
            sqlite3_exec(db, l_sql, 0, 0, 0);

            LOG("Player %s left clan %d\n", players[index].username, cid);
            
            // Notify the client
            Packet leave_pkt; memset(&leave_pkt, 0, sizeof(Packet));
            leave_pkt.type = PACKET_CLAN_LEAVE;
            send_all(client_sockets[index], &leave_pkt, sizeof(Packet), 0);

            broadcast_clan_update(cid);
        }
    }
    else if (pkt->type == PACKET_CLAN_STORAGE_UPDATE) {
        if (players[index].clan_id == -1) return;

        // 1. Gold Update
        if (pkt->clan_gold != 0) {
            int amount = pkt->clan_gold;
            int clan_id = players[index].clan_id;
            
            if (amount > 0) {
                // DEPOSIT: Anyone can deposit
                if (players[index].gold >= amount) {
                    // Update DB
                    char s_sql[128];
                    snprintf(s_sql, 128, "UPDATE clans SET GOLD_STORAGE = GOLD_STORAGE + %d WHERE ID=%d;", amount, clan_id);
                    sqlite3_exec(db, s_sql, 0, 0, 0);

                    players[index].gold -= amount;
                    snprintf(s_sql, 128, "UPDATE users SET GOLD = %d WHERE ID=%d;", players[index].gold, players[index].id);
                    sqlite3_exec(db, s_sql, 0, 0, 0);

                    LOG("Player %s deposited %d gold to clan %d\n", players[index].username, amount, clan_id);
                    send_player_stats(index);
                    broadcast_clan_update(clan_id);
                    
                    Packet p; memset(&p, 0, sizeof(Packet));
                    p.type = PACKET_CHAT; p.player_id = -1;
                    snprintf(p.msg, 127, "Deposited %d gold.", amount);
                    send_all(client_sockets[index], &p, sizeof(Packet), 0);
                } else {
                    Packet err; memset(&err, 0, sizeof(Packet));
                    err.type = PACKET_CHAT; err.player_id = -1;
                    strcpy(err.msg, "Insufficient gold.");
                    send_all(client_sockets[index], &err, sizeof(Packet), 0);
                }
            } 
            else {
                // WITHDRAW: Limit to Owner (role 1)
                if (players[index].clan_role != 1) {
                    Packet err; memset(&err, 0, sizeof(Packet));
                    err.type = PACKET_CHAT; err.player_id = -1;
                    strcpy(err.msg, "Only owner can withdraw gold.");
                    send_all(client_sockets[index], &err, sizeof(Packet), 0);
                    return;
                }

                int withdraw_amount = -amount;
                sqlite3_stmt *stmt;
                int current_gold = 0;
                const char *sql_g = "SELECT GOLD_STORAGE FROM clans WHERE ID = ?;";
                if (sqlite3_prepare_v2(db, sql_g, -1, &stmt, 0) == SQLITE_OK) {
                    sqlite3_bind_int(stmt, 1, clan_id);
                    if (sqlite3_step(stmt) == SQLITE_ROW) current_gold = sqlite3_column_int(stmt, 0);
                    sqlite3_finalize(stmt);
                }

                if (withdraw_amount > current_gold) {
                    Packet err; memset(&err, 0, sizeof(Packet));
                    err.type = PACKET_CHAT; err.player_id = -1;
                    strcpy(err.msg, "Insufficient clan gold.");
                    send_all(client_sockets[index], &err, sizeof(Packet), 0);
                } else {
                    char s_sql[128];
                    snprintf(s_sql, 128, "UPDATE clans SET GOLD_STORAGE = GOLD_STORAGE - %d WHERE ID=%d;", withdraw_amount, clan_id);
                    sqlite3_exec(db, s_sql, 0, 0, 0);

                    players[index].gold += withdraw_amount;
                    snprintf(s_sql, 128, "UPDATE users SET GOLD = %d WHERE ID=%d;", players[index].gold, players[index].id);
                    sqlite3_exec(db, s_sql, 0, 0, 0);

                    LOG("Player %s withdrew %d gold from clan %d\n", players[index].username, withdraw_amount, clan_id);
                    send_player_stats(index);
                    broadcast_clan_update(clan_id);
                    
                    Packet p; memset(&p, 0, sizeof(Packet));
                    p.type = PACKET_CHAT; p.player_id = -1;
                    snprintf(p.msg, 127, "Withdrew %d gold.", withdraw_amount);
                    send_all(client_sockets[index], &p, sizeof(Packet), 0);
                }
            }
        }
        
        // 2. Item Withdrawal
        else if (pkt->item_data.quantity == -1) {
             int item_id = pkt->item_data.item_id;
             int clan_id = players[index].clan_id;
             
             // Find slot and quantity of this item in clan storage
             sqlite3_stmt *stmt;
             int slot_index = -1;
             int stored_qty = 0;
             const char *sql_find = "SELECT SLOT_INDEX, QUANTITY FROM clan_storage_items WHERE CLAN_ID=? AND ITEM_ID=?;";
             if (sqlite3_prepare_v2(db, sql_find, -1, &stmt, 0) == SQLITE_OK) {
                 sqlite3_bind_int(stmt, 1, clan_id);
                 sqlite3_bind_int(stmt, 2, item_id);
                 if (sqlite3_step(stmt) == SQLITE_ROW) {
                     slot_index = sqlite3_column_int(stmt, 0);
                     stored_qty = sqlite3_column_int(stmt, 1);
                 }
                 sqlite3_finalize(stmt);
             }
             
             if (slot_index != -1 && stored_qty > 0) {
                 if (give_item_to_player(index, item_id, stored_qty)) {
                     char sql_del[128];
                     snprintf(sql_del, 128, "DELETE FROM clan_storage_items WHERE CLAN_ID=%d AND SLOT_INDEX=%d;", clan_id, slot_index);
                     sqlite3_exec(db, sql_del, 0, 0, 0);
                     
                     send_inventory_update(index);
                     broadcast_clan_update(clan_id);
                     LOG("Player %s withdrew item %d x%d from clan %d\n", players[index].username, item_id, stored_qty, clan_id);
                 } else {
                     Packet err; memset(&err, 0, sizeof(Packet));
                     err.type = PACKET_CHAT; err.player_id = -1;
                     strcpy(err.msg, "Inventory full.");
                     send_all(client_sockets[index], &err, sizeof(Packet), 0);
                 }
             }
        }
    }
    else if (pkt->type == PACKET_CLAN_ITEM_DEPOSIT) {
        if (players[index].clan_id == -1) return;
        int inv_slot = pkt->item_slot;
        if (inv_slot < 0 || inv_slot >= MAX_INVENTORY_SLOTS) return; // Note: player might have less items than slots, but we check presence.
        
        Item *item = &players[index].inventory[inv_slot].item;
        if (item->item_id <= 0) return;

        int clan_id = players[index].clan_id;
        int qty_to_deposit = item->quantity;
        int item_id = item->item_id;

        // 1. Find if item already exists in clan storage (to stack)
        sqlite3_stmt *stmt;
        int target_slot = -1;
        int existing_qty = 0;
        int max_stack = 999; // Default max stack for shared storage
        
        const char *sql_find = "SELECT SLOT_INDEX, QUANTITY FROM clan_storage_items WHERE CLAN_ID=? AND ITEM_ID=?;";
        if (sqlite3_prepare_v2(db, sql_find, -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, clan_id);
            sqlite3_bind_int(stmt, 2, item_id);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                target_slot = sqlite3_column_int(stmt, 0);
                existing_qty = sqlite3_column_int(stmt, 1);
            }
            sqlite3_finalize(stmt);
        }

        if (target_slot != -1) {
            // Update existing slot
            char sql_upd[256];
            snprintf(sql_upd, 256, "UPDATE clan_storage_items SET QUANTITY = QUANTITY + %d WHERE CLAN_ID=%d AND SLOT_INDEX=%d;",
                     qty_to_deposit, clan_id, target_slot);
            sqlite3_exec(db, sql_upd, 0, 0, 0);
        } else {
            // Find empty slot (0-19)
            // Cheap way: get all slots, then find first missing
            int taken[20] = {0};
            const char *sql_slots = "SELECT SLOT_INDEX FROM clan_storage_items WHERE CLAN_ID=?;";
            if (sqlite3_prepare_v2(db, sql_slots, -1, &stmt, 0) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, clan_id);
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    int s = sqlite3_column_int(stmt, 0);
                    if (s >= 0 && s < 20) taken[s] = 1;
                }
                sqlite3_finalize(stmt);
            }
            
            for (int i = 0; i < 20; i++) {
                if (!taken[i]) {
                    target_slot = i;
                    break;
                }
            }
            
            if (target_slot == -1) {
                Packet err; memset(&err, 0, sizeof(Packet));
                err.type = PACKET_CHAT; err.player_id = -1;
                strcpy(err.msg, "Clan storage is full.");
                send_all(client_sockets[index], &err, sizeof(Packet), 0);
                return;
            }
            
            char sql_ins[256];
            snprintf(sql_ins, 256, "INSERT INTO clan_storage_items (CLAN_ID, SLOT_INDEX, ITEM_ID, QUANTITY) VALUES (%d, %d, %d, %d);",
                     clan_id, target_slot, item_id, qty_to_deposit);
            sqlite3_exec(db, sql_ins, 0, 0, 0);
        }

        // Remove from player
        remove_item_from_player(index, item_id, qty_to_deposit);
        send_inventory_update(index);
        broadcast_clan_update(clan_id);
        LOG("Player %s deposited item %d x%d to clan %d\n", players[index].username, item_id, qty_to_deposit, clan_id);
    }
    else if (pkt->type == PACKET_CLAN_ITEM_WITHDRAW) {
        if (players[index].clan_id == -1) return;
        int storage_slot = pkt->item_slot;
        if (storage_slot < 0 || storage_slot >= 20) return;

        int clan_id = players[index].clan_id;
        sqlite3_stmt *stmt;
        int item_id = 0;
        int quantity = 0;
        
        const char *sql_get = "SELECT ITEM_ID, QUANTITY FROM clan_storage_items WHERE CLAN_ID=? AND SLOT_INDEX=?;";
        if (sqlite3_prepare_v2(db, sql_get, -1, &stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, clan_id);
            sqlite3_bind_int(stmt, 2, storage_slot);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                item_id = sqlite3_column_int(stmt, 0);
                quantity = sqlite3_column_int(stmt, 1);
            }
            sqlite3_finalize(stmt);
        }

        if (item_id <= 0) return;

        // Give to player
        if (give_item_to_player(index, item_id, quantity)) {
            // Remove from clan storage
            char sql_del[128];
            snprintf(sql_del, 128, "DELETE FROM clan_storage_items WHERE CLAN_ID=%d AND SLOT_INDEX=%d;", clan_id, storage_slot);
            sqlite3_exec(db, sql_del, 0, 0, 0);
            
            send_inventory_update(index);
            broadcast_clan_update(clan_id);
            LOG("Player %s withdrew item %d x%d from clan %d slot %d\n", players[index].username, item_id, quantity, clan_id, storage_slot);
        } else {
            Packet err; memset(&err, 0, sizeof(Packet));
            err.type = PACKET_CHAT; err.player_id = -1;
            strcpy(err.msg, "Inventory full.");
            send_all(client_sockets[index], &err, sizeof(Packet), 0);
        }
    }
}

int recv_full(socket_t sockfd, void *buf, size_t len) {
    size_t total = 0;
    size_t bytes_left = len;
    int n;
    while(total < len) {
        n = recv(sockfd, (char*)buf + total, bytes_left, 0); 
        if(n <= 0) return n; // Error or disconnect
        total += n; 
        bytes_left -= n;
    }
    return total;
}

void cleanup_server() {
    static int cleanup_called = 0;
    if (cleanup_called) return;  // Prevent double cleanup
    cleanup_called = 1;
    
    printf("\nShutting down server...\n");
    if (log_file) LOG("Shutting down server...\n");
    
    // Close all client connections
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (SOCKET_IS_VALID(client_sockets[i])) {
            close(client_sockets[i]);
            client_sockets[i] = SOCKET_INVALID;
        }
    }
    
    // Close database properly with flush
    if (db) {
        // Ensure all pending transactions are completed
        sqlite3_exec(db, "PRAGMA optimize;", 0, 0, 0);
        int rc = sqlite3_close(db);
        if (rc == SQLITE_BUSY) {
            // Force close if busy
            sqlite3_close_v2(db);
        }
        db = NULL;
        printf("Database closed\n");
        if (log_file) LOG("Database closed\n");
    }
    
    // Close log file last
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
    
#ifdef _WIN32
    WSACleanup();
#endif
    printf("Server shutdown complete\n");
}

void signal_handler(int signum) {
    printf("\nReceived signal %d, shutting down...\n", signum);
    cleanup_server();
    exit(0);
}

// --- NEW TICK THREAD ---
void *tick_thread(void *arg) {
    int respawn_counter = 0;
    while (1) {
        usleep(50000); // 50ms (20 updates/sec)
        pthread_mutex_lock(&state_mutex);
        broadcast_state();
        
        // Enemy respawn check every 10 seconds (200 ticks * 50ms = 10s)
        respawn_counter++;
        if (respawn_counter >= 200) {
            respawn_counter = 0;
            
            // Check for dead enemies and respawn them after some time
            for (int i = 0; i < server_enemy_count; i++) {
                if (!server_enemies[i].active) {
                    // Respawn this enemy
                    server_enemies[i].hp = server_enemies[i].max_hp;
                    server_enemies[i].active = 1;
                    // Randomize position slightly
                    server_enemies[i].x = 300.0f + (rand() % 800);
                    server_enemies[i].y = 300.0f + (rand() % 600);
                    
                    LOG("Enemy %s respawned at (%.1f, %.1f)\\n", 
                        server_enemies[i].name, server_enemies[i].x, server_enemies[i].y);
                }
            }
            
            // Broadcast updated enemy list if any respawned
            broadcast_enemy_list();
        }

        // Enemy retaliation & Player logic (Every 20 ticks = 1 second)
        static int retaliate_timer = 0;
        retaliate_timer++;
        if (retaliate_timer >= 20) {
            retaliate_timer = 0;
            uint32_t now = (uint32_t)time(NULL);

            for (int e = 0; e < server_enemy_count; e++) {
                if (!server_enemies[e].active) continue;

                for (int p = 0; p < MAX_CLIENTS; p++) {
                    if (players[p].active && players[p].hp > 0 && strcmp(players[p].map_name, server_enemies[e].map_name) == 0) {
                        float dx = players[p].x - server_enemies[e].x;
                        float dy = players[p].y - server_enemies[e].y;
                        float dist = sqrt(dx*dx + dy*dy);

                        if (dist < 64.0f) { // Attack range
                            int dmg = server_enemies[e].attack_power + (rand() % 3);
                            players[p].hp -= dmg;
                            if (players[p].hp < 0) players[p].hp = 0;

                            LOG("Enemy %s hit player %s for %d damage (HP: %d/%d)\n",
                                server_enemies[e].name, players[p].username, dmg, players[p].hp, players[p].max_hp);

                            // Send damage notification
                            Packet dmg_pkt;
                            memset(&dmg_pkt, 0, sizeof(Packet));
                            dmg_pkt.type = PACKET_DAMAGE;
                            dmg_pkt.damage = dmg;
                            dmg_pkt.dx = players[p].x;
                            dmg_pkt.dy = players[p].y;
                            // Broadcast
                            for (int i = 0; i < MAX_CLIENTS; i++) {
                                if (SOCKET_IS_VALID(client_sockets[i]) && players[i].active) {
                                    send_all(client_sockets[i], &dmg_pkt, sizeof(Packet), 0);
                                }
                            }
                            send_player_stats(p);

                            if (players[p].hp == 0) {
                                LOG("Player %s died!\n", players[p].username);
                                // Death logic: Reset to spawn, lose 10% XP
                                players[p].hp = players[p].max_hp;
                                players[p].x = 100.0f;
                                players[p].y = 100.0f;
                                players[p].xp -= players[p].xp / 10;
                                
                                Packet chat_pkt;
                                memset(&chat_pkt, 0, sizeof(Packet));
                                chat_pkt.type = PACKET_CHAT;
                                chat_pkt.player_id = -1;
                                strcpy(chat_pkt.msg, "You died! Respawned at home.");
                                send_all(client_sockets[p], &chat_pkt, sizeof(Packet), 0);
                                send_player_stats(p);
                            }
                        }
                    }
                }
            }
        }
        
        pthread_mutex_unlock(&state_mutex);
    }
    return NULL;
}

void *client_handler(void *arg) {
    int index = *(int*)arg; free(arg); socket_t sd = client_sockets[index]; Packet pkt;

    // Send Handshake
    Packet handshake;
    memset(&handshake, 0, sizeof(Packet));
    handshake.type = PACKET_HANDSHAKE;
    handshake.protocol_version = 1001;
    strcpy(handshake.msg, "MMORPG_VALID");
    send_all(sd, &handshake, sizeof(Packet), 0);

    while (1) {
        int valread = recv_full(sd, &pkt, sizeof(Packet));
        if (valread <= 0) {
            pthread_mutex_lock(&state_mutex);
            close(sd); client_sockets[index] = SOCKET_INVALID;
            if(players[index].active) {
                save_player_location(players[index].username, players[index].x, players[index].y);
                char sql[256]; snprintf(sql, 256, "UPDATE users SET LAST_LOGIN=datetime('now', 'localtime') WHERE ID=%d;", players[index].id);
                sqlite3_exec(db, sql, 0, 0, 0);
            }
            
            // Cancel any active trade
            if (player_trades[index].partner_id != -1) {
                int partner_idx = -1;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (players[i].active && players[i].id == player_trades[index].partner_id) {
                        partner_idx = i;
                        break;
                    }
                }
                if (partner_idx >= 0) {
                    player_trades[partner_idx].partner_id = -1;
                    Packet cancel_pkt;
                    memset(&cancel_pkt, 0, sizeof(Packet));
                    cancel_pkt.type = PACKET_TRADE_CANCEL;
                    cancel_pkt.response_accepted = 0;
                    send_all(client_sockets[partner_idx], &cancel_pkt, sizeof(Packet), 0);
                }
                player_trades[index].partner_id = -1;
            }
            
            players[index].active = 0; broadcast_state(); broadcast_friend_update();
            pthread_mutex_unlock(&state_mutex);
            break;
        }
        if (pkt.type == PACKET_AVATAR_UPLOAD) {
            if (pkt.image_size > 0 && pkt.image_size <= MAX_AVATAR_SIZE) {
                uint8_t *img_buf = malloc(pkt.image_size);
                if (recv_full(sd, img_buf, pkt.image_size) == pkt.image_size) {
                    pthread_mutex_lock(&state_mutex); 
                    char filepath[64]; snprintf(filepath, 64, "avatars/%d.img", players[index].id);
                    FILE *fp = fopen(filepath, "wb");
                    if (fp) { fwrite(img_buf, 1, pkt.image_size, fp); fclose(fp); LOG("Saved avatar User %d\\n", players[index].id); }
                    pthread_mutex_unlock(&state_mutex);
                }
                free(img_buf);
            }
        } else {
            pthread_mutex_lock(&state_mutex);
            handle_client_message(index, &pkt);
            pthread_mutex_unlock(&state_mutex);
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    //printf("DEBUG: Packet Size is %d bytes\n", (int)sizeof(Packet));
    
    // Initialize logging
    #ifdef _WIN32
    _mkdir("logs");
    #else
    mkdir("logs", 0755);
    #endif
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char log_filename[256];
    if (tm_info) {
        strftime(log_filename, sizeof(log_filename), "logs/server_%Y%m%d_%H%M%S.log", tm_info);
    } else {
        snprintf(log_filename, sizeof(log_filename), "logs/server_default.log");
    }
    
    log_file = fopen(log_filename, "w");
    if (log_file) {
        LOG("Server log started\n");
    } else {
        printf("Warning: Could not create log file %s\n", log_filename);
    }
    
    socket_t server_fd, new_socket; 
    struct sockaddr_in address;
    
    // 1. Set Default Port
    int current_port = PORT; // Default 8888 from common.h

    // 2. Parse Command Line Arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            current_port = atoi(argv[i+1]);
            if (current_port <= 0 || current_port > 65535) {
                LOG("Invalid port number. Using default %d.\\n", PORT);
                current_port = PORT;
            }
        }
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#else
    signal(SIGPIPE, SIG_IGN);
#endif

    // Register cleanup function to run on exit (more reliable than signals on Windows)
    atexit(cleanup_server);

    // Install signal handlers for clean shutdown
#ifdef _WIN32
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGBREAK, signal_handler);
#else
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
#endif
    
    init_game();
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_INVALID) exit(1);
    
    int opt = 1; 
#ifdef _WIN32
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    
    // 3. Use the variable port
    address.sin_port = htons(current_port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { 
        perror("Bind failed"); 
        exit(1); 
    }
    
    if (listen(server_fd, MAX_CLIENTS) < 0) { 
        perror("Listen failed"); 
        exit(1); 
    }
    
    // Get and display server IP address
    char hostname[256];
    char ip_address[INET_ADDRSTRLEN] = "0.0.0.0";
    int found_good_ip = 0;
    
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct hostent *host_entry = gethostbyname(hostname);
        if (host_entry != NULL && host_entry->h_addr_list[0] != NULL) {
            // Iterate through all IP addresses and prefer non-virtual interfaces
            for (int i = 0; host_entry->h_addr_list[i] != NULL; i++) {
                struct in_addr addr;
                memcpy(&addr, host_entry->h_addr_list[i], sizeof(struct in_addr));
#ifdef _WIN32
                char *current_ip = inet_ntoa(addr);
#else
                char current_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &addr, current_ip, INET_ADDRSTRLEN);
#endif
                
                // Skip localhost
                if (strncmp(current_ip, "127.", 4) == 0) continue;
                
                // Prefer private network IPs (192.168.x.x, 10.x.x.x) over virtual adapters
                // WSL typically uses 172.x.x.x but so do some LANs, so we prefer 192.168 first
                if (!found_good_ip || 
                    (strncmp(current_ip, "192.168.", 8) == 0) ||
                    (strncmp(current_ip, "10.", 3) == 0 && strncmp(ip_address, "192.168.", 8) != 0)) {
                    strcpy(ip_address, current_ip);
                    found_good_ip = 1;
                    // If we found a 192.168.x.x address, use it immediately (most likely WiFi/LAN)
                    if (strncmp(current_ip, "192.168.", 8) == 0) break;
                }
            }
            
            if (found_good_ip) {
                LOG("Server running on %s:%d\n", ip_address, current_port);
            } else {
                LOG("Server running on port %d (could not determine IP)\\n", current_port);
            }
        } else {
            LOG("Server running on port %d (could not determine IP)\\n", current_port);
        }
    } else {
        LOG("Server running on port %d (could not determine IP)\\n", current_port);
    }

    // Start Tick Thread
    pthread_t ticker;
    if (pthread_create(&ticker, NULL, tick_thread, NULL) != 0) { 
        perror("Failed to start tick thread"); 
        return 1; 
    }

    // Start GUI Thread
    server_start_time = time(NULL);
    if (pthread_create(&gui_thread_handle, NULL, gui_thread, NULL) != 0) {
        LOG("Failed to start GUI thread\n");
    }

    while (1) {
        socklen_t addrlen = sizeof(address);
        new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (!SOCKET_IS_VALID(new_socket)) { 
            perror("Accept failed"); 
            continue; 
        }
    
    pthread_mutex_lock(&state_mutex);
    int slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) { 
        if (!SOCKET_IS_VALID(client_sockets[i])) { 
            client_sockets[i] = new_socket; 
            players[i].id = -1; 
            players[i].active = 0;
            player_trades[i].partner_id = -1; // Reset trade state for new connection
            slot = i; 
                break; 
            } 
        }
        pthread_mutex_unlock(&state_mutex);
        
        if (slot != -1) {
            int *arg = malloc(sizeof(int)); 
            *arg = slot;
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, client_handler, arg) != 0) { 
                perror("Thread create failed"); 
                close(new_socket); 
            } else {
                pthread_detach(thread_id);
            }
        } else { 
            LOG("Server full.\\n"); 
            close(new_socket); 
        }
    }
    return 0;
}