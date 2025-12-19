#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #include <ws2tcpip.h>
    #define close closesocket
    #define ioctl ioctlsocket
    #define sleep(x) Sleep(x * 1000)
    #define usleep(x) Sleep(x / 1000)
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <sys/utsname.h>
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_syswm.h>
#include <sys/types.h>

// Handle OpenGL headers
#if defined(__IPHONEOS__) || defined(__ANDROID__)
#include <SDL2/SDL_opengles2.h>
#elif defined(__APPLE__)
#include <SDL2/SDL_opengl.h>
#ifndef __IPHONEOS__
// Objective-C runtime for macOS window manipulation
#include <objc/runtime.h>
#include <objc/message.h>
#include <CoreFoundation/CoreFoundation.h>
#endif
#elif defined(_WIN32)
#include <SDL2/SDL_opengl.h>
#else
#include <GL/gl.h>
#endif

#include "common.h"
#include <dirent.h> // MinGW usually provides this, but visual studio does not. MinGW is fine.

// Define NSInteger for macOS Objective-C runtime calls
#if defined(__APPLE__) && !defined(__IPHONEOS__)
#if defined(__LP64__) && __LP64__
typedef long NSInteger;
#else
typedef int NSInteger;
#endif
// NSWindow constants
#define NSWindowTitleVisible 0
#define NSWindowTabbingModeDisallowed 2
#endif

// --- Config ---
#define PLAYER_WIDTH 32
#define PLAYER_HEIGHT 32
char current_map_file[32] = "map.jpg";
#define PLAYER_FILE "player.png"
#define FONT_PATH "DejaVuSans.ttf"
#define FONT_SIZE 14
#define SCROLL_SPEED 30  // Pixels per mouse wheel notch



// --- Globals ---
#ifdef _WIN32
    typedef SOCKET socket_t;
#else
    typedef int socket_t;
#endif

socket_t sock = 0;
char server_ip[16] = "127.0.0.1";
TTF_Font *font = NULL;
SDL_Texture *tex_map = NULL;
SDL_Texture *tex_player = NULL;
SDL_Renderer *global_renderer = NULL;
int map_w = MAP_WIDTH, map_h = MAP_HEIGHT;

int local_player_id = -1;
Player local_players[MAX_CLIENTS];
FriendInfo my_friends[20]; // Changed from int array to Struct array
int friend_count = 0;

// BLOCKED SYSTEM
int blocked_ids[50];
int blocked_count = 0;
int show_blocked_list = 0;
int blocked_scroll = 0; // Scroll offset for blocked list 

// UI Rects
SDL_Rect btn_hide_player;      
SDL_Rect btn_view_blocked;     
SDL_Rect blocked_win;          
SDL_Rect btn_close_blocked;    
SDL_Rect btn_hide_player_dyn; // Dynamic rect for click handling

// UI STATE
int selected_player_id = -1;
int pending_friend_req_id = -1;
char pending_friend_name[32] = "";

SDL_Rect profile_win = {10, 10, 200, 130}; 
SDL_Rect btn_add_friend = {20, 50, 180, 30};
SDL_Rect btn_send_pm    = {20, 90, 180, 30}; 
SDL_Rect popup_win;

// Settings & Debug State
int is_settings_open = 0;
int show_debug_info = 0;
int show_fps = 0;
int show_coords = 0;
int current_ping = 0;
Uint32 last_ping_sent = 0;

int current_fps = 0;
int frame_count = 0;
Uint32 last_fps_check = 0;

SDL_Rect btn_settings_toggle;
SDL_Rect settings_win;
SDL_Rect btn_toggle_debug;
SDL_Rect btn_toggle_fps;
SDL_Rect btn_toggle_coords;

// Chat & Auth
typedef struct { char msg[64]; Uint32 timestamp; } FloatingText;
FloatingText floating_texts[MAX_CLIENTS];
#define CHAT_HISTORY 10
char chat_log[CHAT_HISTORY][64];
int chat_log_count = 0;
int is_chat_open = 0;
int chat_target_id = -1; 
int chat_input_active = 0;
char input_buffer[64] = "";

typedef enum { STATE_AUTH, STATE_GAME } ClientState;
ClientState client_state = STATE_AUTH;

#define MAX_INPUT_LEN 31
char auth_username[MAX_INPUT_LEN+1] = "";
char auth_password[MAX_INPUT_LEN+1] = "";
int active_field = -1;
int show_password = 0;
char auth_message[128] = "Enter Credentials";

SDL_Rect auth_box, btn_login, btn_register, btn_chat_toggle, btn_show_pass;
SDL_Color col_white = {255,255,255,255};
SDL_Color col_red = {255,50,50,255};
SDL_Color col_yellow = {255,255,0,255};
SDL_Color col_cyan = {0,255,255,255};
SDL_Color col_green = {0,255,0,255}; 
SDL_Color col_btn = {100,100,100,255};
SDL_Color col_black = {0,0,0,255};
SDL_Color col_magenta = {255,0,255,255}; 

// Status Colors
SDL_Color col_status_online = {0, 255, 0, 255};
SDL_Color col_status_afk = {255, 255, 0, 255};
SDL_Color col_status_dnd = {255, 50, 50, 255};
SDL_Color col_status_rp = {200, 0, 200, 255};
SDL_Color col_status_talk = {50, 150, 255, 255};

const char* status_names[] = { "Online", "AFK", "Do Not Disturb", "Roleplay", "Open to Talk" };

SDL_Rect btn_cycle_status; 
SDL_Rect btn_view_friends; 
SDL_Rect friend_list_win;
int show_friend_list = 0;
int friend_list_scroll = 0; // Scroll offset for friend list

SDL_Rect slider_r, slider_g, slider_b;

SDL_Texture* avatar_cache[MAX_CLIENTS]; 
int avatar_status[MAX_CLIENTS]; 

// Audio
char music_playlist[20][64]; 
int music_count = 0;
int current_track = -1;
Mix_Music *bgm = NULL;
int music_volume = 64; 
SDL_Rect slider_volume; 

uint8_t temp_avatar_buf[MAX_AVATAR_SIZE];

// --- Connection State ---
char input_ip[64] = "127.0.0.1";
char input_port[16] = "8888";
int is_connected = 0;

// --- Server List State ---
typedef struct { char name[32]; char ip[64]; int port; } ServerEntry;
ServerEntry server_list[10];
int server_count = 0;
int show_server_list = 0;
SDL_Rect server_list_win;
SDL_Rect btn_open_servers; // On login screen
SDL_Rect btn_add_server;   // Inside list window
SDL_Rect btn_close_servers; // Close button for server list

// --- Profile List State ---
typedef struct { char username[32]; char password[32]; } ProfileEntry;
ProfileEntry profile_list[10];
int profile_count = 0;
int show_profile_list = 0;
SDL_Rect profile_list_win;
SDL_Rect btn_open_profiles; // On login screen
SDL_Rect btn_add_profile;   // Inside list window
SDL_Rect btn_close_profiles; // Close button for profile list

int show_nick_popup = 0;
char nick_new[32] = "";
char nick_confirm[32] = "";
char nick_pass[32] = "";

// --- Inbox State ---
typedef struct { int id; char name[32]; } IncomingReq;
IncomingReq inbox[10];
int inbox_count = 0;
int is_inbox_open = 0;
SDL_Rect btn_inbox;
int inbox_scroll = 0; // Scroll offset for inbox 

// --- Add Friend Popup State ---
int show_add_friend_popup = 0;
char input_friend_id[10] = "";
char friend_popup_msg[128] = "";

SDL_Rect btn_friend_add_id_rect; // Global to store position for click handler
SDL_Rect friend_win_rect;
SDL_Rect btn_friend_close_rect;

SDL_Rect blocked_win_rect;
SDL_Rect btn_blocked_close_rect;
SDL_Rect btn_cycle_status;
SDL_Rect slider_r, slider_g, slider_b, slider_volume, slider_afk;
SDL_Rect btn_disconnect_rect; // Renamed to avoid conflicts
SDL_Rect btn_inbox_rect;
SDL_Rect btn_friend_add_id_rect;

// Blocked UI
SDL_Rect blocked_win_rect;
SDL_Rect btn_blocked_close_rect;

int cursor_pos = 0; // Tracks current text cursor index

int unread_chat_count = 0;
int show_unread_counter = 1; // Default ON
SDL_Rect btn_toggle_unread;

// --- GL Probe Cache ---
char gl_renderer_cache[128] = "";
char gl_vendor_cache[128] = "";
int gl_probe_done = 0;

Uint32 last_input_tick = 0;
int afk_timeout_minutes = 2; // Default 2 minutes
Uint32 last_color_packet_ms = 0;
int is_auto_afk = 0;         // Flag to know if we triggered it automatically
SDL_Rect slider_afk;

// --- Scrolling State ---
int settings_scroll_y = 0;
int settings_content_h = 0; // Total height of content
SDL_Rect settings_view_port; // Visible area

// --- Secondary Color Sliders ---
SDL_Rect slider_r2, slider_g2, slider_b2;
int my_r2 = 255, my_g2 = 255, my_b2 = 255; // Local state for now

int saved_r = 255, saved_g = 255, saved_b = 255;

int selection_start = 0; // Index where selection begins
int selection_len = 0;   // Length of selection (can be negative for left-selection)
int is_dragging = 0;     // For mouse drag selection
SDL_Rect active_input_rect;

int show_nick_pass = 0;      // Toggle for Nickname Menu
SDL_Rect btn_show_nick_pass; // Button rect for Nickname Menu

int show_contributors = 0;

// --- UI Scaling ---
float ui_scale = 1.0f; // Default scale (range 0.5 to 2.0)
float pending_ui_scale = 1.0f; // Scale value being dragged (applied on release)
SDL_Rect slider_ui_scale;

// --- Game World Zoom ---
float game_zoom = 1.0f; // Default zoom (range 0.5 to 2.0)
float pending_game_zoom = 1.0f; // Zoom value being dragged (applied on release)
SDL_Rect slider_game_zoom;

// --- Mobile Keyboard Handling ---
#if defined(__ANDROID__) || defined(__IPHONEOS__)
int keyboard_height = 0; // Estimated keyboard height (updated when keyboard shows)
int chat_window_shift = 0; // Amount to shift chat window up
#endif
int show_documentation = 0;
SDL_Rect btn_contributors_rect;
SDL_Rect btn_documentation_rect;

// Cached textures for performance on iOS
SDL_Texture *cached_contributors_tex = NULL;
SDL_Texture *cached_documentation_tex = NULL;
int contributors_tex_w = 0, contributors_tex_h = 0;
int documentation_tex_w = 0, documentation_tex_h = 0;
int contributors_scroll = 0; // Scroll offset for contributors window
int documentation_scroll = 0; // Scroll offset for documentation window

int show_role_list = 0;
struct { int id; char name[32]; int role; } staff_list[50];
int staff_count = 0;
SDL_Rect btn_staff_list_rect;

// --- Sanction System ---
int show_sanction_popup = 0;
int sanction_target_id = -1;
int sanction_mode = 0; // 0=Warn, 1=Ban
char input_sanction_reason[64] = "";
char input_ban_time[16] = ""; // e.g. "1d"

int show_my_warnings = 0;
struct { char reason[64]; char date[32]; } my_warning_list[20];
int my_warning_count = 0;
int warnings_scroll = 0; // Scroll offset for warnings window

SDL_Rect btn_sanction_open; // In Profile
SDL_Rect btn_my_warnings;   // In Settings

enum { SLIDER_NONE, SLIDER_R, SLIDER_G, SLIDER_B, SLIDER_R2, SLIDER_G2, SLIDER_B2, SLIDER_VOL, SLIDER_AFK, SLIDER_UI_SCALE, SLIDER_GAME_ZOOM };
int active_slider = SLIDER_NONE;

// --- Color Sliders ---
SDL_Rect slider_r, slider_g, slider_b;
SDL_Rect slider_r2, slider_g2, slider_b2;
// Add these:
int my_r = 255, my_g = 255, my_b = 255; 

typedef struct {
    char src_map[32];
    SDL_Rect rect;
    char target_map[32];
    int spawn_x, spawn_y;
} MapTrigger;

MapTrigger triggers[20];
int trigger_count = 0;
Uint32 last_map_switch_time = 0;
Uint32 last_move_time = 0; // <--- MOVE HERE

// Mobile Controls
SDL_Rect dpad_rect = {20, 0, 150, 150}; // Y calculated dynamically
float vjoy_dx = 0, vjoy_dy = 0;
SDL_FingerID touch_id_dpad = -1; // Track which finger is on the D-Pad
SDL_FingerID scroll_touch_id = -1;
int scroll_last_y = 0;
int joystick_active = 0;


void get_path(char *out, const char *filename, int is_save_file) {
    #if defined(__IPHONEOS__) || defined(__ANDROID__) || (defined(__APPLE__) && !defined(__IPHONEOS__))
        if (is_save_file) {
            // Writeable folder (Documents/Library)
            char *pref = SDL_GetPrefPath("MyOrg", "C_MMO_Client");
            if (pref) { snprintf(out, 256, "%s%s", pref, filename); SDL_free(pref); }
        } else {
            // Read-only Asset folder (Bundle/Resources)
            char *base = SDL_GetBasePath();
            if (base) { snprintf(out, 256, "%s%s", base, filename); SDL_free(base); }
        }
    #else
        // Desktop: Just use the filename
        strcpy(out, filename);
    #endif
}

void ensure_save_file(const char *filename, const char *asset_name) {
    char path[256]; get_path(path, filename, 1);
    FILE *fp = fopen(path, "r");
    if (fp) { fclose(fp); return; }
    fp = fopen(path, "w");
    if (!fp) return;
    if (asset_name) {
        char asset_path[256]; get_path(asset_path, asset_name, 0);
        FILE *af = fopen(asset_path, "r");
        if (af) {
            char buf[256];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), af)) > 0) fwrite(buf, 1, n, fp);
            fclose(af);
        }
    }
    fclose(fp);
}

// --- Helpers ---
int send_all(socket_t sockfd, const void *buf, size_t len) {
    size_t total = 0;
    size_t bytes_left = len;
    int n;

    while(total < len) {
        n = send(sockfd, (const char*)buf + total, bytes_left, 0);
        if (n == -1) { break; }
        total += n;
        bytes_left -= n;
    }
    return (n == -1) ? -1 : 0; // return -1 on failure, 0 on success
}

void send_packet(Packet *pkt) { 
    send_all(sock, pkt, sizeof(Packet)); 
}

// --- Helper to ensure full packet receipt on Windows ---
int recv_total(socket_t sockfd, void *buf, size_t len) {
    size_t total = 0;
    size_t bytes_left = len;
    int n;
    while(total < len) {
        // Note: Using flag 0 instead of MSG_WAITALL
        n = recv(sockfd, (char*)buf + total, bytes_left, 0); 
        if(n <= 0) return n; // Error or disconnect
        total += n; 
        bytes_left -= n;
    }
    return total;
}

void load_servers() {
    char path[256]; get_path(path, "servers.txt", 1); // 1 = Save File
    FILE *fp = fopen(path, "r");
    if (!fp) return;
    server_count = 0;
    while (fscanf(fp, "%s %s %d", server_list[server_count].name, server_list[server_count].ip, &server_list[server_count].port) == 3) {
        server_count++;
        if (server_count >= 10) break;
    }
    fclose(fp);
}

void save_config() {
    char path[256]; get_path(path, "config.txt", 1); // 1 = Save File
    FILE *fp = fopen(path, "w");
    if (fp) {
        int r=255, g=255, b=255;
        for(int i=0; i<MAX_CLIENTS; i++) {
            if(local_players[i].active && local_players[i].id == local_player_id) {
                r = local_players[i].r; g = local_players[i].g; b = local_players[i].b;
            }
        }
        // Format: R G B R2 G2 B2 AFK_MIN DEBUG FPS COORDS VOL UNREAD UI_SCALE GAME_ZOOM
        fprintf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %.2f %.2f\n", 
            r, g, b, 
            my_r2, my_g2, my_b2, 
            afk_timeout_minutes,
            show_debug_info, show_fps, show_coords, music_volume,
            show_unread_counter,
            ui_scale,
            game_zoom
        );
        fclose(fp);
    }
}

void load_config() {
    char path[256]; get_path(path, "config.txt", 1); // 1 = Save File
    FILE *fp = fopen(path, "r");
    if (!fp) {
        get_path(path, "configs.txt", 1);
        fp = fopen(path, "r");
    }
    if (!fp) {
        get_path(path, "config.txt", 0);
        fp = fopen(path, "r");
    }
    if (!fp) {
        get_path(path, "configs.txt", 0);
        fp = fopen(path, "r");
    }
    if (fp) {
        // Temp variables for flags
        int dbg=0, fps=0, crd=0, vol=64, unread=1;
        float scale=1.0f, zoom=1.0f;
        
        int count = fscanf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d %f %f", 
            &saved_r, &saved_g, &saved_b, 
            &my_r2, &my_g2, &my_b2, 
            &afk_timeout_minutes,
            &dbg, &fps, &crd, &vol, &unread, &scale, &zoom
        );
        
        // Only apply if we successfully read at least the old version (11) or new (12)
        if (count >= 11) {
            show_debug_info = dbg;
            show_fps = fps;
            show_coords = crd;
            music_volume = vol;
            Mix_VolumeMusic(music_volume);
        }
        
        // Apply new field if present
        if (count >= 12) {
            show_unread_counter = unread;
        }
        
        // Apply UI scale if present
        if (count >= 13) {
            ui_scale = scale;
            // Clamp to valid range
            if (ui_scale < 0.5f) ui_scale = 0.5f;
            if (ui_scale > 2.0f) ui_scale = 2.0f;
            pending_ui_scale = ui_scale; // Initialize pending to match current
        }
        
        // Apply game zoom if present
        if (count >= 14) {
            game_zoom = zoom;
            // Clamp to valid range
            if (game_zoom < 0.5f) game_zoom = 0.5f;
            if (game_zoom > 2.0f) game_zoom = 2.0f;
            pending_game_zoom = game_zoom; // Initialize pending to match current
        }
        
        fclose(fp);
    }
}

void load_triggers() {
    // Triggers are now loaded from the server, not from file
    // This function kept for compatibility but does nothing
    trigger_count = 0;
    printf("Client: Waiting to receive triggers from server...\n");
}

void receive_triggers_from_server(Packet *pkt) {
    trigger_count = pkt->trigger_count;
    printf("Client: Received %d triggers from server\n", trigger_count);
    
    for (int i = 0; i < trigger_count && i < 20; i++) {
        strncpy(triggers[i].src_map, pkt->triggers[i].src_map, sizeof(triggers[i].src_map) - 1);
        triggers[i].src_map[sizeof(triggers[i].src_map) - 1] = '\0';
        triggers[i].rect.x = pkt->triggers[i].rect_x;
        triggers[i].rect.y = pkt->triggers[i].rect_y;
        triggers[i].rect.w = pkt->triggers[i].rect_w;
        triggers[i].rect.h = pkt->triggers[i].rect_h;
        strncpy(triggers[i].target_map, pkt->triggers[i].target_map, sizeof(triggers[i].target_map) - 1);
        triggers[i].target_map[sizeof(triggers[i].target_map) - 1] = '\0';
        triggers[i].spawn_x = pkt->triggers[i].spawn_x;
        triggers[i].spawn_y = pkt->triggers[i].spawn_y;
        
        printf("Client: Loaded Trigger %d: %s [%d,%d %dx%d] -> %s\n",
               i, triggers[i].src_map,
               triggers[i].rect.x, triggers[i].rect.y,
               triggers[i].rect.w, triggers[i].rect.h,
               triggers[i].target_map);
    }
}

void switch_map(const char* new_map, int x, int y) {
    // 1. Update State
    if (SDL_GetTicks() - last_map_switch_time < 2000) return;
    last_map_switch_time = SDL_GetTicks();
    strncpy(current_map_file, new_map, 31);
    
    // 2. Load New Texture
    if (tex_map) SDL_DestroyTexture(tex_map);
    char path[256]; get_path(path, current_map_file, 0); // 0 = Asset
    SDL_Surface *temp = IMG_Load(path);
    if (temp) {
        tex_map = SDL_CreateTextureFromSurface(global_renderer, temp); // Need global renderer or pass it
        map_w = temp->w; map_h = temp->h;
        SDL_FreeSurface(temp);
    } else {
        printf("Failed to load map: %s\n", current_map_file);
    }

    // 3. Teleport Local Player
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(local_players[i].active && local_players[i].id == local_player_id) {
            local_players[i].x = x;
            local_players[i].y = y;
            strncpy(local_players[i].map_name, new_map, 31); // Update local struct immediately
        }
    }

    // 4. Notify Server
    Packet pkt; 
    pkt.type = PACKET_MAP_CHANGE;
    strncpy(pkt.target_map, new_map, 31);
    pkt.dx = x; 
    pkt.dy = y;
    send_packet(&pkt);
    
    printf("Switched to %s at %d,%d\n", new_map, x, y);
}

int get_cursor_pos_from_click(const char *text, int mouse_x, int rect_x) {
    if (!text || strlen(text) == 0) return 0;
    
    int len = strlen(text);
    int best_index = 0;
    int min_dist = 10000; // Arbitrary large number

    // Iterate through valid UTF-8 indices
    for (int i = 0; i <= len; ) {
        char temp[256];
        strncpy(temp, text, i);
        temp[i] = 0;
        
        int w, h;
        TTF_SizeText(font, temp, &w, &h);
        
        // +5 is the padding offset used in render_input_with_cursor
        int text_screen_x = rect_x + 5 + w;
        int dist = abs(mouse_x - text_screen_x);
        
        if (dist < min_dist) {
            min_dist = dist;
            best_index = i;
        }

        // Advance to next UTF-8 character start
        if (i == len) break;
        do { i++; } while (i < len && (text[i] & 0xC0) == 0x80);
    }
    return best_index;
}

// Helper: Delete currently selected text
void delete_selection(char *buffer) {
    if (selection_len == 0) return;

    int start = selection_start;
    int len = selection_len;
    
    // Normalize if selection goes backwards
    if (len < 0) {
        start += len; // move start back
        len = -len;   // make len positive
    }

    int total_len = strlen(buffer);
    if (start < 0) start = 0;
    if (start + len > total_len) len = total_len - start;

    // Shift text left
    memmove(buffer + start, buffer + start + len, total_len - start - len + 1);
    
    // Reset cursor and selection
    cursor_pos = start;
    selection_len = 0;
}



void handle_text_edit(char *buffer, int max_len, SDL_Event *ev) {
    int len = strlen(buffer);
    const Uint8 *state = SDL_GetKeyboardState(NULL);
    int shift_pressed = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
    int ctrl_pressed = state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];

    if (ev->type == SDL_TEXTINPUT) {
        // If text is selected, delete it first
        if (selection_len != 0) delete_selection(buffer);
        
        int add_len = strlen(ev->text.text);
        if (strlen(buffer) + add_len <= max_len) {
            memmove(buffer + cursor_pos + add_len, buffer + cursor_pos, strlen(buffer) - cursor_pos + 1);
            memcpy(buffer + cursor_pos, ev->text.text, add_len);
            cursor_pos += add_len;
        }
    } 
    else if (ev->type == SDL_KEYDOWN) {
        // --- COPY (Ctrl+C) ---
        if (ctrl_pressed && ev->key.keysym.sym == SDLK_c) {
            if (selection_len != 0) {
                int start = selection_start;
                int slen = selection_len;
                if (slen < 0) { start += slen; slen = -slen; }
                
                char *clip_buf = malloc(slen + 1);
                if (clip_buf) {
                    strncpy(clip_buf, buffer + start, slen);
                    clip_buf[slen] = 0;
                    SDL_SetClipboardText(clip_buf);
                    free(clip_buf);
                }
            }
        }
        // --- PASTE (Ctrl+V) ---
        else if (ctrl_pressed && ev->key.keysym.sym == SDLK_v) {
            if (SDL_HasClipboardText()) {
                char *text = SDL_GetClipboardText();
                if (text) {
                    if (selection_len != 0) delete_selection(buffer);
                    
                    int add_len = strlen(text);
                    if (strlen(buffer) + add_len <= max_len) {
                        memmove(buffer + cursor_pos + add_len, buffer + cursor_pos, strlen(buffer) - cursor_pos + 1);
                        memcpy(buffer + cursor_pos, text, add_len);
                        cursor_pos += add_len;
                    }
                    SDL_free(text);
                }
            }
        }
        // --- CUT (Ctrl+X) ---
        else if (ctrl_pressed && ev->key.keysym.sym == SDLK_x) {
            if (selection_len != 0) {
                // Copy logic
                int start = selection_start;
                int slen = selection_len;
                if (slen < 0) { start += slen; slen = -slen; }
                char *clip_buf = malloc(slen + 1);
                if (clip_buf) {
                    strncpy(clip_buf, buffer + start, slen);
                    clip_buf[slen] = 0;
                    SDL_SetClipboardText(clip_buf);
                    free(clip_buf);
                }
                // Delete logic
                delete_selection(buffer);
            }
        }
        // --- SELECT ALL (Ctrl+A) ---
        else if (ctrl_pressed && ev->key.keysym.sym == SDLK_a) {
            cursor_pos = len;
            selection_start = 0;
            selection_len = len;
        }
        // --- NAVIGATION & SELECTION ---
        else if (ev->key.keysym.sym == SDLK_LEFT) {
            if (shift_pressed && selection_len == 0) selection_start = cursor_pos;
            
            if (cursor_pos > 0) {
                do { cursor_pos--; if(shift_pressed) selection_len--; } 
                while (cursor_pos > 0 && (buffer[cursor_pos] & 0xC0) == 0x80);
            }
            if (!shift_pressed) selection_len = 0;
        }
        else if (ev->key.keysym.sym == SDLK_RIGHT) {
            if (shift_pressed && selection_len == 0) selection_start = cursor_pos;

            if (cursor_pos < len) {
                do { cursor_pos++; if(shift_pressed) selection_len++; } 
                while (cursor_pos < len && (buffer[cursor_pos] & 0xC0) == 0x80);
            }
            if (!shift_pressed) selection_len = 0;
        }
        else if (ev->key.keysym.sym == SDLK_BACKSPACE) {
            if (selection_len != 0) {
                delete_selection(buffer);
            } else if (cursor_pos > 0) {
                int end = cursor_pos;
                int start = end;
                do { start--; } while (start > 0 && (buffer[start] & 0xC0) == 0x80);
                memmove(buffer + start, buffer + end, len - end + 1);
                cursor_pos = start;
            }
        }
    }
}

void save_servers() {
    char path[256]; get_path(path, "servers.txt", 1); // 1 = Save File
    FILE *fp = fopen(path, "w");
    if (!fp) return;
    for(int i=0; i<server_count; i++) {
        fprintf(fp, "%s %s %d\n", server_list[i].name, server_list[i].ip, server_list[i].port);
    }
    fclose(fp);
}

void add_server_to_list(const char* name, const char* ip, int port) {
    if (server_count >= 10) return;
    strcpy(server_list[server_count].name, name);
    strcpy(server_list[server_count].ip, ip);
    server_list[server_count].port = port;
    server_count++;
    save_servers();
}

void load_profiles() {
    char path[256]; get_path(path, "profiles.txt", 1); // 1 = Save File
    FILE *fp = fopen(path, "r");
    if (!fp) return;
    profile_count = 0;
    while (fscanf(fp, "%s %s", profile_list[profile_count].username, profile_list[profile_count].password) == 2) {
        profile_count++;
        if (profile_count >= 10) break;
    }
    fclose(fp);
}

void save_profiles() {
    char path[256]; get_path(path, "profiles.txt", 1); // 1 = Save File
    FILE *fp = fopen(path, "w");
    if (!fp) return;
    for(int i=0; i<profile_count; i++) {
        fprintf(fp, "%s %s\n", profile_list[i].username, profile_list[i].password);
    }
    fclose(fp);
}

void add_profile_to_list(const char* username, const char* password) {
    if (profile_count >= 10) return;
    strcpy(profile_list[profile_count].username, username);
    strcpy(profile_list[profile_count].password, password);
    profile_count++;
    save_profiles();
}

int try_connect() {
    if (is_connected) close(sock);
    
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == (socket_t)-1) {
        strcpy(auth_message, "Socket Error");
        return 0;
    }
    
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(atoi(input_port));
    
    // Try inet_pton first (for IP addresses)
    if (inet_pton(AF_INET, input_ip, &serv_addr.sin_addr) <= 0) {
        // If inet_pton fails, try getaddrinfo (for domain names)
        struct addrinfo hints, *result = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        if (getaddrinfo(input_ip, NULL, &hints, &result) != 0 || result == NULL) {
            if (result) freeaddrinfo(result);
            strcpy(auth_message, "Invalid Address");
            close(sock);
            return 0;
        }
        
        // Copy the resolved address
        memcpy(&serv_addr.sin_addr, &((struct sockaddr_in*)result->ai_addr)->sin_addr, sizeof(struct in_addr));
        freeaddrinfo(result);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        strcpy(auth_message, "Connection Failed");
        return 0;
    }

    // --- FIX: Update the global variable for Debug Menu ---
    strcpy(server_ip, input_ip); 
    // -----------------------------------------------------

    is_connected = 1;
    strcpy(auth_message, "Connected! Logging in...");
    return 1;
}

int is_blocked(int id) {
    for(int i=0; i<blocked_count; i++) if(blocked_ids[i] == id) return 1;
    return 0;
}

void reset_avatar_cache() {
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(avatar_cache[i]) SDL_DestroyTexture(avatar_cache[i]);
        avatar_cache[i] = NULL;
        avatar_status[i] = 0;
    }
}

// New helper to handle array shifting
void push_chat_line(const char *text) {
    for(int i=0; i<CHAT_HISTORY-1; i++) strcpy(chat_log[i], chat_log[i+1]);
    strncpy(chat_log[CHAT_HISTORY-1], text, 63);
}

void play_next_track() {
    if (music_count == 0) return;
    if (bgm) Mix_FreeMusic(bgm);
    current_track++;
    if (current_track >= music_count) current_track = 0;
    char path[256];
    char music_file[256];
    snprintf(music_file, 256, "music/%s", music_playlist[current_track]);
    get_path(path, music_file, 0);
    bgm = Mix_LoadMUS(path);
    if (bgm) Mix_PlayMusic(bgm, 1);
}

void init_audio() {
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) return;
    
    #ifdef __ANDROID__
    // On Android, we can't use opendir on assets, so we manually list known music files
    // or try to load files with known names
    const char* android_music_files[] = {"1.mp3", NULL}; // Add more files as needed
    for(int i = 0; android_music_files[i] != NULL && music_count < 20; i++) {
        // Try to load to verify file exists
        char path[256];
        char music_file[256];
        snprintf(music_file, 256, "music/%s", android_music_files[i]);
        get_path(path, music_file, 0);
        SDL_RWops *rw = SDL_RWFromFile(path, "rb");
        if (rw) {
            SDL_RWclose(rw);
            strncpy(music_playlist[music_count++], android_music_files[i], 63);
        }
    }
    #else
    DIR *d; struct dirent *dir;
    char music_dir_path[256];
    get_path(music_dir_path, "music", 0);
    d = opendir(music_dir_path);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strstr(dir->d_name, ".mp3") || strstr(dir->d_name, ".ogg") || strstr(dir->d_name, ".wav")) {
                if (music_count < 20) strncpy(music_playlist[music_count++], dir->d_name, 63);
            }
        }
        closedir(d);
    }
    #endif
    Mix_VolumeMusic(music_volume);
}

void toggle_block(int id) {
    if (is_blocked(id)) {
        for(int i=0; i<blocked_count; i++) {
            if(blocked_ids[i] == id) {
                for(int j=i; j<blocked_count-1; j++) blocked_ids[j] = blocked_ids[j+1];
                blocked_count--; return;
            }
        }
    } else if(blocked_count < 50) blocked_ids[blocked_count++] = id;
}

SDL_Color get_status_color(int status) {
    switch(status) {
        case STATUS_AFK: return col_status_afk;
        case STATUS_DND: return col_status_dnd;
        case STATUS_ROLEPLAY: return col_status_rp;
        case STATUS_TALK: return col_status_talk;
        default: return col_status_online;
    }
}

// --- Rich Text Helpers ---

// Helper function to scale an integer value based on UI scale
int scale_ui(int value) {
    return (int)(value * ui_scale);
}

// Helper function to scale a rectangle
SDL_Rect scale_rect(int x, int y, int w, int h) {
    return (SDL_Rect){scale_ui(x), scale_ui(y), scale_ui(w), scale_ui(h)};
}

// Returns color for codes ^1 through ^9
SDL_Color get_color_from_code(char code) {
    switch(code) {
        case '1': return (SDL_Color){255, 50, 50, 255};   // Red
        case '2': return (SDL_Color){50, 255, 50, 255};   // Green
        case '3': return (SDL_Color){80, 150, 255, 255};  // Blue
        case '4': return (SDL_Color){255, 255, 0, 255};   // Yellow
        case '5': return (SDL_Color){0, 255, 255, 255};   // Cyan
        case '6': return (SDL_Color){255, 0, 255, 255};   // Magenta
        case '7': return (SDL_Color){255, 255, 255, 255}; // White
        case '8': return (SDL_Color){150, 150, 150, 255}; // Gray
        case '9': return (SDL_Color){0, 0, 0, 255};       // Black
        default: return (SDL_Color){255, 255, 255, 255};
    }
}

// Old Simple Renderer (Renamed) - Used for Input Boxes so you see raw codes while typing
void render_raw_text(SDL_Renderer *renderer, const char *text, int x, int y, SDL_Color color, int center) {
    if (!text || strlen(text) == 0) return;
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    int rx = center ? x - (surface->w/2) : x;
    SDL_Rect dst = {rx, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

// Helper: Safely remove the last UTF-8 character
void remove_last_utf8_char(char *str) {
    int len = strlen(str);
    if (len == 0) return;

    // Move back while the byte is a "continuation byte" (starts with 10xxxxxx)
    // 0xC0 is 11000000. If (byte & 0xC0) == 0x80, it's a continuation byte.
    while (len > 0) {
        len--;
        // If it's a start byte (0xxxxxxx or 11xxxxxx), we stop deleting here
        if ((str[len] & 0xC0) != 0x80) break; 
    }
    str[len] = '\0';
}

void render_text_gradient(SDL_Renderer *renderer, const char *text, int x, int y, SDL_Color c1, SDL_Color c2, int center) {
    if (!text || !*text) return;

    // 1. Render base white text
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, (SDL_Color){255, 255, 255, 255});
    if (!surface) return;

    SDL_LockSurface(surface);
    
    int w = surface->w;
    int h = surface->h;
    int pitch = surface->pitch; // Actual bytes per row in memory
    Uint8 *pixels = (Uint8 *)surface->pixels;
    SDL_PixelFormat *fmt = surface->format;

    // 2. Iterate pixels safely
    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            // Calculate exact memory address for this pixel
            // Row offset (py * pitch) + Column offset (px * 4 bytes)
            Uint32 *pixel_addr = (Uint32 *)(pixels + py * pitch + px * 4);
            
            Uint32 pixel_val = *pixel_addr;
            Uint8 r_old, g_old, b_old, a;
            SDL_GetRGBA(pixel_val, fmt, &r_old, &g_old, &b_old, &a);

            // Apply gradient only to visible pixels
            if (a > 0) {
                float t = (float)px / (float)w; // 0.0 to 1.0 based on width

                Uint8 r = c1.r + (int)((c2.r - c1.r) * t);
                Uint8 g = c1.g + (int)((c2.g - c1.g) * t);
                Uint8 b = c1.b + (int)((c2.b - c1.b) * t);

                *pixel_addr = SDL_MapRGBA(fmt, r, g, b, a);
            }
        }
    }
    
    SDL_UnlockSurface(surface);

    // 3. Create Texture & Render
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    int rx = center ? x - (w / 2) : x;
    SDL_Rect dst = {rx, y, w, h};
    SDL_RenderCopy(renderer, texture, NULL, &dst);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void render_text(SDL_Renderer *renderer, const char *text, int x, int y, SDL_Color default_color, int center) {
    if (!text || !*text) return;

    int original_style = TTF_GetFontStyle(font);
    int current_style = original_style;
    SDL_Color current_color = default_color;
    
    typedef struct { SDL_Texture *tex; int w, h; } Segment;
    Segment segments[64]; int seg_count = 0;
    int total_width = 0; int max_height = 0;

    char buffer[256]; int buf_idx = 0; int i = 0;

    while (text[i] && seg_count < 64) {
        int flush = 0; int skip = 0;
        
        // Color Code (^1 - ^9)
        if (text[i] == '^' && text[i+1] >= '0' && text[i+1] <= '9') {
            flush = 1; skip = 2;
        } 
        // Style Tags (* = Italic, # = Bold, ~ = Strikethrough)
        // CHANGED: '-' -> '~' to avoid conflicts with dates/minuses
        else if (text[i] == '*' || text[i] == '#' || text[i] == '~') {
            flush = 1; skip = 1;
        }

        if (flush) {
            if (buf_idx > 0) {
                buffer[buf_idx] = 0;
                TTF_SetFontStyle(font, current_style);
                SDL_Surface *s = TTF_RenderUTF8_Blended(font, buffer, current_color);
                if (s) {
                    segments[seg_count].tex = SDL_CreateTextureFromSurface(renderer, s);
                    segments[seg_count].w = s->w; segments[seg_count].h = s->h;
                    total_width += s->w; if (s->h > max_height) max_height = s->h;
                    SDL_FreeSurface(s); seg_count++;
                }
                buf_idx = 0;
            }

            if (skip == 2) {
                current_color = get_color_from_code(text[i+1]);
            } else if (skip == 1) {
                if (text[i] == '*') current_style ^= TTF_STYLE_ITALIC;
                if (text[i] == '#') current_style ^= TTF_STYLE_BOLD;
                if (text[i] == '~') current_style ^= TTF_STYLE_STRIKETHROUGH; // CHANGED
            }
            i += skip;
        } else {
            buffer[buf_idx++] = text[i++];
        }
    }

    // Flush remaining
    if (buf_idx > 0 && seg_count < 64) {
        buffer[buf_idx] = 0;
        TTF_SetFontStyle(font, current_style);
        SDL_Surface *s = TTF_RenderUTF8_Blended(font, buffer, current_color);
        if (s) {
            segments[seg_count].tex = SDL_CreateTextureFromSurface(renderer, s);
            segments[seg_count].w = s->w; segments[seg_count].h = s->h;
            total_width += s->w; if (s->h > max_height) max_height = s->h;
            SDL_FreeSurface(s); seg_count++;
        }
    }

    // Render
    int current_x = center ? (x - total_width / 2) : x;
    for (int k = 0; k < seg_count; k++) {
        if (segments[k].tex) {
            SDL_Rect dst = {current_x, y, segments[k].w, segments[k].h};
            SDL_RenderCopy(renderer, segments[k].tex, NULL, &dst);
            current_x += segments[k].w;
            SDL_DestroyTexture(segments[k].tex);
        }
    }
    TTF_SetFontStyle(font, original_style);
}

// --- Role Helpers ---
const char* get_role_name(int role) {
    switch(role) {
        case 1: return "ADMIN";        // ROLE_ADMIN
        case 2: return "DEV";          // ROLE_DEVELOPER
        case 3: return "CONTRIB";      // ROLE_CONTRIBUTOR
        case 4: return "VIP";          // ROLE_VIP
        default: return "Player";
    }
}

SDL_Color get_role_color(int role) {
    switch(role) {
        case 1: return (SDL_Color){255, 50, 50, 255};   // Red (Admin)
        case 2: return (SDL_Color){50, 150, 255, 255};  // Blue (Dev)
        case 3: return (SDL_Color){0, 200, 100, 255};   // Teal (Contrib)
        case 4: return (SDL_Color){255, 215, 0, 255};   // Gold (VIP)
        default: return (SDL_Color){100, 100, 100, 255}; // Grey
    }
}

void render_input_with_cursor(SDL_Renderer *renderer, SDL_Rect rect, char *buffer, int is_active, int is_password) {
    // 1. Draw Box
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, is_active ? 0 : 100, is_active ? 255 : 100, 0, 255); SDL_RenderDrawRect(renderer, &rect);

    // 2. Prepare Display Text
    char display[256];
    if (is_password) {
        memset(display, '*', strlen(buffer)); display[strlen(buffer)] = 0;
    } else {
        strcpy(display, buffer);
    }

    // Calculate text dimensions
    int text_width = 0, text_height = 0;
    if (strlen(display) > 0) {
        TTF_SizeText(font, display, &text_width, &text_height);
    }
    
    // Calculate cursor position width
    char temp_to_cursor[256];
    strncpy(temp_to_cursor, display, cursor_pos);
    temp_to_cursor[cursor_pos] = 0;
    int cursor_x = 0;
    if (cursor_pos > 0) {
        TTF_SizeText(font, temp_to_cursor, &cursor_x, &text_height);
    }
    
    // Calculate scroll offset to keep cursor visible
    int available_width = rect.w - 10; // 5px padding on each side
    // Note: static variable is shared across all input fields, but this is acceptable
    // because only one field can be active at a time and it resets when inactive
    static int scroll_offset = 0;
    
    if (is_active) {
        // Ensure cursor is visible
        if (cursor_x - scroll_offset > available_width - 10) {
            // Cursor is too far right, scroll right
            scroll_offset = cursor_x - available_width + 10;
        } else if (cursor_x < scroll_offset) {
            // Cursor is too far left, scroll left
            scroll_offset = cursor_x;
        }
    } else {
        scroll_offset = 0; // Reset when not active
    }
    
    // Clamp scroll offset
    if (scroll_offset < 0) scroll_offset = 0;
    if (text_width - scroll_offset < available_width) {
        scroll_offset = text_width - available_width;
        if (scroll_offset < 0) scroll_offset = 0;
    }

    // Set clipping rectangle to prevent text overflow
    SDL_Rect clip_rect = {rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4};
    SDL_RenderSetClipRect(renderer, &clip_rect);

    // --- Render Selection Highlight ---
    if (is_active && selection_len != 0) {
        int start = selection_start;
        int len = selection_len;
        
        // Handle reverse selection
        if (len < 0) { start += len; len = -len; }

        // Measure width up to Start
        char temp_start[256]; strncpy(temp_start, display, start); temp_start[start] = 0;
        int w_start = 0, h; if (start > 0) TTF_SizeText(font, temp_start, &w_start, &h);

        // Measure width of Selection
        char temp_sel[256]; strncpy(temp_sel, display + start, len); temp_sel[len] = 0;
        int w_sel = 0; if (len > 0) TTF_SizeText(font, temp_sel, &w_sel, &h);

        // Draw Blue Box behind text (with scroll offset applied)
        SDL_Rect sel_rect = {rect.x + 5 + w_start - scroll_offset, rect.y + 4, w_sel, 20};
        SDL_SetRenderDrawColor(renderer, 0, 100, 255, 128); // Semi-transparent blue
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_RenderFillRect(renderer, &sel_rect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }
    // ----------------------------------------

    // 3. Render Text (Raw) with scroll offset
    render_raw_text(renderer, display, rect.x + 5 - scroll_offset, rect.y + 2, col_white, 0);

    // 4. Render Cursor
    if (is_active) {
        if ((SDL_GetTicks() / 500) % 2 == 0) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            int cursor_render_x = rect.x + 5 + cursor_x - scroll_offset;
            SDL_RenderDrawLine(renderer, cursor_render_x, rect.y + 4, cursor_render_x, rect.y + 20);
        }
    }
    
    // Remove clipping
    SDL_RenderSetClipRect(renderer, NULL);
}

void add_chat_message(Packet *pkt) {
    // 1. Block Check
    if (pkt->player_id != -1 && pkt->player_id != local_player_id) {
        if (is_blocked(pkt->player_id)) return;
    }

    // 2. Format the Message into a temporary buffer
    char entry[128]; 
    
    if (pkt->type == PACKET_PRIVATE_MESSAGE) {
        char *sender_name = "Unknown"; char *target_name = "Unknown";
        for(int i=0; i<MAX_CLIENTS; i++) {
            if (local_players[i].active) {
                if (local_players[i].id == pkt->player_id) sender_name = local_players[i].username;
                if (local_players[i].id == pkt->target_id) target_name = local_players[i].username;
            }
        }
        if (pkt->player_id == local_player_id) {
            snprintf(entry, 128, "To [%s]: %s", target_name, pkt->msg);
        } else {
            snprintf(entry, 128, "From [%s]: %s", sender_name, pkt->msg);
        }
    } else {
        char *name = "System";
        if (pkt->player_id != -1) {
            name = "Unknown"; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == pkt->player_id) name = local_players[i].username;
        }
        snprintf(entry, 128, "[%s]: %s", name, pkt->msg);
    }

    // 3. Floating Text Logic (Always show full message above head)
    if (pkt->player_id != -1) {
        for(int i=0; i<MAX_CLIENTS; i++) {
            if(local_players[i].id == pkt->player_id) {
                strncpy(floating_texts[i].msg, pkt->msg, 63);
                floating_texts[i].timestamp = SDL_GetTicks();
            }
        }
    }

    // 4. Width Check & Splitting
    int w, h;
    TTF_SizeText(font, entry, &w, &h);
    
    // Chat window is 300px wide. Text starts at x+15. Safe width ~270.
    if (w > 270) {
        // Find a safe split point (35 chars is roughly 270px at size 14)
        int split_idx = 35;
        if (strlen(entry) < 35) split_idx = strlen(entry);

        char line1[64], line2[64];
        
        // Copy first part
        strncpy(line1, entry, split_idx);
        line1[split_idx] = '\0'; 
        
        // Copy second part (with indent)
        snprintf(line2, 64, "  %s", entry + split_idx);

        push_chat_line(line1);
        push_chat_line(line2);
    } else {
        // Fits on one line
        push_chat_line(entry);
    }
}

void render_hud(SDL_Renderer *renderer, int screen_w, int screen_h) {
    int y = 10; char buf[64];
    if (show_fps) {
        snprintf(buf, 64, "FPS: %d", current_fps);
        int w, h; TTF_SizeText(font, buf, &w, &h); SDL_Rect bg = {10, y, w+4, h};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150); SDL_RenderFillRect(renderer, &bg);
        render_text(renderer, buf, 12, y, col_green, 0); y += 20;
    }
    if (show_coords) {
        float px=0, py=0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].active && local_players[i].id == local_player_id) { px=local_players[i].x; py=local_players[i].y; }
        snprintf(buf, 64, "XY: %.0f, %.0f", px, py);
        int w, h; TTF_SizeText(font, buf, &w, &h); SDL_Rect bg = {10, y, w+4, h};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150); SDL_RenderFillRect(renderer, &bg);
        render_text(renderer, buf, 12, y, col_white, 0);
    }

    // INBOX BUTTON (Top Right) - use passed scaled width, not SDL_GetRendererOutputSize
    btn_inbox = (SDL_Rect){screen_w - 50, 10, 40, 40};
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); SDL_RenderFillRect(renderer, &btn_inbox);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &btn_inbox);
    render_text(renderer, "[M]", btn_inbox.x+8, btn_inbox.y+10, col_white, 0);

    if (inbox_count > 0) {
        SDL_Rect badge = {btn_inbox.x+25, btn_inbox.y-5, 20, 20};
        SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255); SDL_RenderFillRect(renderer, &badge);
        char num[4]; sprintf(num, "%d", inbox_count);
        render_text(renderer, num, badge.x+6, badge.y+2, col_white, 0);
    }
}

void render_inbox(SDL_Renderer *renderer, int w, int h) {
    if (!is_inbox_open) return;
    
    SDL_Rect win = {w - 320, 60, 300, 300};
    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255); SDL_RenderFillRect(renderer, &win);
    SDL_SetRenderDrawColor(renderer, 100, 100, 255, 255); SDL_RenderDrawRect(renderer, &win);
    render_text(renderer, "Pending Requests", win.x + 80, win.y + 10, col_yellow, 0);

    if (inbox_count == 0) {
        render_text(renderer, "No new requests.", win.x + 80, win.y + 40, col_white, 0);
        return;
    }
    
    // Set up clipping for scrollable area
    SDL_Rect clip_rect = {win.x, win.y + 35, win.w, win.h - 35};
    SDL_RenderSetClipRect(renderer, &clip_rect);

    int y = win.y + 40 - inbox_scroll; // Apply scroll offset
    for(int i=0; i<inbox_count; i++) {
        // Only render if within visible area
        if (y + 55 > win.y + 35 && y < win.y + win.h) {
            SDL_Rect row = {win.x+10, y, 280, 50};
            SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255); SDL_RenderFillRect(renderer, &row);
            
            char label[64]; snprintf(label, 64, "%s (ID: %d)", inbox[i].name, inbox[i].id);
            render_text(renderer, label, row.x+5, row.y+5, col_white, 0);

            // Accept
            SDL_Rect btn_acc = {row.x+160, row.y+25, 50, 20};
            SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_acc);
            render_text(renderer, "Yes", btn_acc.x+10, btn_acc.y+2, col_white, 0);

            // Deny
            SDL_Rect btn_deny = {row.x+220, row.y+25, 50, 20};
            SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_deny);
            render_text(renderer, "No", btn_deny.x+15, btn_deny.y+2, col_white, 0);
        }
        y += 55;
    }
    
    // Reset clipping
    SDL_RenderSetClipRect(renderer, NULL);
}

void render_add_friend_popup(SDL_Renderer *renderer, int w, int h) {
    if (!show_add_friend_popup) return;
    
    SDL_Rect pop = {w/2 - 150, h/2 - 100, 300, 200};
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); SDL_RenderFillRect(renderer, &pop);
    SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255); SDL_RenderDrawRect(renderer, &pop);
    
    render_text(renderer, "Add Friend by ID", pop.x+80, pop.y+10, col_green, 0);
    
    // --- NEW: Status Message ---
    // If message starts with "Error", render in Red, otherwise Yellow
    SDL_Color status_col = (strncmp(friend_popup_msg, "Error", 5) == 0) ? col_red : col_yellow;
    render_text(renderer, friend_popup_msg, pop.x+150, pop.y+35, status_col, 1); // 1 = Centered
    // ---------------------------
    
    SDL_Rect input_rect = {pop.x+50, pop.y+60, 200, 30};
    render_input_with_cursor(renderer, input_rect, input_friend_id, active_field == 20, 0);

    SDL_Rect btn_ok = {pop.x+50, pop.y+130, 80, 30};
    SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_ok);
    render_text(renderer, "Add", btn_ok.x+25, btn_ok.y+5, col_white, 0);

    SDL_Rect btn_cancel = {pop.x+170, pop.y+130, 80, 30};
    SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_cancel);
    render_text(renderer, "Cancel", btn_cancel.x+15, btn_cancel.y+5, col_white, 0);
}


void render_sanction_popup(SDL_Renderer *renderer, int w, int h) {
    if (!show_sanction_popup) return;

    SDL_Rect win = {w/2 - 150, h/2 - 150, 300, 300};
    SDL_SetRenderDrawColor(renderer, 40, 0, 0, 255); SDL_RenderFillRect(renderer, &win);
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); SDL_RenderDrawRect(renderer, &win);

    render_text(renderer, "Sanction Player", win.x + 150, win.y + 10, col_red, 1);

    // Mode Toggle (Warn / Ban)
    SDL_Rect btn_warn = {win.x + 20, win.y + 40, 120, 30};
    SDL_Rect btn_ban = {win.x + 160, win.y + 40, 120, 30};
    
    SDL_SetRenderDrawColor(renderer, sanction_mode==0?200:50, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_warn);
    render_text(renderer, "WARN", btn_warn.x + 60, btn_warn.y + 5, col_white, 1);
    
    SDL_SetRenderDrawColor(renderer, sanction_mode==1?200:50, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_ban);
    render_text(renderer, "BAN", btn_ban.x + 60, btn_ban.y + 5, col_white, 1);

    int y = win.y + 90;
    
    // Reason Input
    render_text(renderer, "Reason:", win.x + 20, y, col_white, 0);
    render_input_with_cursor(renderer, (SDL_Rect){win.x + 20, y+25, 260, 30}, input_sanction_reason, active_field == 30, 0);
    y += 70;

    // Time Input (Ban Only)
    if (sanction_mode == 1) {
        render_text(renderer, "Time (1h, 1d, 1w):", win.x + 20, y, col_white, 0);
        render_input_with_cursor(renderer, (SDL_Rect){win.x + 20, y+25, 100, 30}, input_ban_time, active_field == 31, 0);
    } else {
        render_text(renderer, "(3 Warns = Auto Ban)", win.x + 150, y+20, col_yellow, 1);
    }

    // Submit / Cancel
    SDL_Rect btn_submit = {win.x + 20, win.y + 240, 120, 30};
    SDL_Rect btn_cancel = {win.x + 160, win.y + 240, 120, 30};
    
    SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_submit);
    render_text(renderer, "EXECUTE", btn_submit.x + 60, btn_submit.y + 5, col_white, 1);
    
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_cancel);
    render_text(renderer, "Cancel", btn_cancel.x + 60, btn_cancel.y + 5, col_white, 1);
}

void render_my_warnings(SDL_Renderer *renderer, int w, int h) {
    if (!show_my_warnings) return;

    SDL_Rect win = {w/2 - 200, h/2 - 200, 400, 400};
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); SDL_RenderFillRect(renderer, &win);
    SDL_SetRenderDrawColor(renderer, 255, 165, 0, 255); SDL_RenderDrawRect(renderer, &win);

    render_text(renderer, "My Warnings", win.x + 200, win.y + 10, col_yellow, 1);
    
    SDL_Rect btn_close = {win.x + 360, win.y + 5, 30, 30};
    SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_close);
    render_text(renderer, "X", btn_close.x + 10, btn_close.y + 5, col_white, 0);

    if (my_warning_count == 0) {
        render_text(renderer, "No warnings on record.", win.x + 200, win.y + 70, col_green, 1);
        return;
    }
    
    // Set up clipping for scrollable area
    SDL_Rect clip_rect = {win.x, win.y + 45, win.w, win.h - 45};
    SDL_RenderSetClipRect(renderer, &clip_rect);

    int y = win.y + 50 - warnings_scroll; // Apply scroll offset
    for (int i=0; i<my_warning_count; i++) {
        // Only render if within visible area
        if (y + 45 > win.y + 45 && y < win.y + win.h) {
            SDL_SetRenderDrawColor(renderer, 50, 0, 0, 255);
            SDL_Rect row = {win.x + 10, y, 380, 40};
            SDL_RenderFillRect(renderer, &row);
            
            char buf[128];
            snprintf(buf, 128, "[%s] %s", my_warning_list[i].date, my_warning_list[i].reason);
            render_text(renderer, buf, row.x + 10, row.y + 10, col_white, 0);
        }
        y += 45;
    }
    
    // Reset clipping
    SDL_RenderSetClipRect(renderer, NULL);
}

void render_debug_overlay(SDL_Renderer *renderer, int screen_w) {
    if (!show_debug_info) return;
    char lines[12][128]; int line_count = 0;
    snprintf(lines[line_count++], 128, "Ping: %d ms", current_ping);
    snprintf(lines[line_count++], 128, "Server IP: %s", server_ip);
    float px=0, py=0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].active && local_players[i].id == local_player_id) { px=local_players[i].x; py=local_players[i].y; }
    snprintf(lines[line_count++], 128, "Pos: %.1f, %.1f", px, py);
    SDL_RendererInfo info;
    SDL_GetRendererInfo(renderer, &info);
    const char *renderer_str = info.name;
    const char *video_drv = SDL_GetCurrentVideoDriver();
    snprintf(lines[line_count++], 128, "VideoDrv: %s", video_drv ? video_drv : "Unknown");
    if (renderer_str) snprintf(lines[line_count++], 128, "GPU: %s", renderer_str); else snprintf(lines[line_count++], 128, "GPU: Unknown");
    void *glctx = SDL_GL_GetCurrentContext();
    if (glctx) {
        const char *gl_renderer = (const char*)glGetString(GL_RENDERER);
        const char *gl_vendor   = (const char*)glGetString(GL_VENDOR);
        if (gl_renderer && strlen(gl_renderer) > 0) snprintf(lines[line_count++], 128, "GL Renderer: %s", gl_renderer);
        if (gl_vendor   && strlen(gl_vendor)   > 0) snprintf(lines[line_count++], 128, "GL Vendor: %s", gl_vendor);
    } else {
        if (gl_renderer_cache[0]) snprintf(lines[line_count++], 128, "GL Renderer: %s", gl_renderer_cache);
        if (gl_vendor_cache[0])   snprintf(lines[line_count++], 128, "GL Vendor: %s", gl_vendor_cache);
        else snprintf(lines[line_count++], 128, "GL Renderer: N/A (non-GL backend)");
    }
    SDL_version compiled; SDL_VERSION(&compiled); snprintf(lines[line_count++], 128, "SDL: %d.%d.%d", compiled.major, compiled.minor, compiled.patch);
    #ifndef _WIN32
    struct utsname buffer; 
    if (uname(&buffer) == 0) snprintf(lines[line_count++], 128, "OS: %s %s", buffer.sysname, buffer.release); 
    else snprintf(lines[line_count++], 128, "OS: Unknown");
    #else
    snprintf(lines[line_count++], 128, "OS: Windows");
    #endif
    char compiler_name[10] = "Unknown";
    #if defined(__clang__) 
        strcpy(compiler_name, "Clang");
    #elif defined(__GNUC__)
        strcpy(compiler_name, "GCC");
    #endif
    snprintf(lines[line_count++], 128, "Compiler: %s %s", compiler_name, __VERSION__);
    int max_w = 200; for(int i=0; i<line_count; i++) { int w, h; TTF_SizeText(font, lines[i], &w, &h); if (w > max_w) max_w = w; }
    int box_w = max_w + 20; int box_h = (line_count * 20) + 10;
    int start_x = screen_w - box_w - 10; if (start_x < 10) start_x = 10;
    SDL_Rect dbg_box = {start_x, 10, box_w, box_h};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150); SDL_RenderFillRect(renderer, &dbg_box);
    int y = 15;
    for(int i=0; i<line_count; i++) {
        SDL_Color color = col_white; if (strncmp(lines[i], "Ping:", 5) == 0) color = col_green;
        render_raw_text(renderer, lines[i], dbg_box.x + 10, y, color, 0); y += 20;
    }
}

void render_role_list(SDL_Renderer *renderer, int w, int h) {
    if (!show_role_list) return;

    SDL_Rect win = {w/2 - 200, h/2 - 225, 400, 450};
    
    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255); SDL_RenderFillRect(renderer, &win);
    SDL_SetRenderDrawColor(renderer, 200, 200, 255, 255); SDL_RenderDrawRect(renderer, &win);

    render_text(renderer, "Server Staff List", win.x + 200, win.y + 15, col_cyan, 1);

    SDL_Rect btn_close = {win.x + 360, win.y + 5, 30, 30};
    SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_close);
    render_text(renderer, "X", btn_close.x + 10, btn_close.y + 5, col_white, 0);

    int y = win.y + 50;
    
    if (staff_count == 0) {
        render_text(renderer, "Loading...", win.x + 200, y + 20, col_white, 1);
    }

    for (int i = 0; i < staff_count; i++) {
        char buffer[128];
        const char* role_name = get_role_name(staff_list[i].role);
        SDL_Color c = get_role_color(staff_list[i].role);

        // Format: [ROLE] Name (ID: X)
        snprintf(buffer, 128, "[%s] %s (ID: %d)", role_name, staff_list[i].name, staff_list[i].id);
        
        render_text(renderer, buffer, win.x + 20, y, c, 0);
        y += 25;
    }
}

void process_slider_drag(int mx) {
    if (active_slider == SLIDER_NONE) return;

    SDL_Rect *r = NULL;
    int *val_ptr = NULL;
    int max_val = 255;
    int update_color = 0; // Flag to send packet

    switch (active_slider) {
        // FIX: Point to Global Variables
        case SLIDER_R:   r = &slider_r;   val_ptr = &my_r; update_color=1; break;
        case SLIDER_G:   r = &slider_g;   val_ptr = &my_g; update_color=1; break;
        case SLIDER_B:   r = &slider_b;   val_ptr = &my_b; update_color=1; break;
        
        case SLIDER_R2:  r = &slider_r2;  val_ptr = &my_r2; update_color=1; break;
        case SLIDER_G2:  r = &slider_g2;  val_ptr = &my_g2; update_color=1; break;
        case SLIDER_B2:  r = &slider_b2;  val_ptr = &my_b2; update_color=1; break;
        
        case SLIDER_VOL: r = &slider_volume; val_ptr = &music_volume; max_val = 128; break;
        case SLIDER_AFK: r = &slider_afk;    val_ptr = &afk_timeout_minutes; max_val = 10; break;
        case SLIDER_UI_SCALE: r = &slider_ui_scale; break; // Special handling below
        case SLIDER_GAME_ZOOM: r = &slider_game_zoom; break; // Special handling below
    }

    if (!r) return;
    
    // Calculate Value
    float pct = (float)(mx - r->x) / (float)r->w;
    if (pct < 0.0f) pct = 0.0f; else if (pct > 1.0f) pct = 1.0f;

    // Handle UI Scale specially (float, different range)
    if (active_slider == SLIDER_UI_SCALE) {
        pending_ui_scale = 0.5f + (pct * 1.5f); // Map 0.0-1.0 to 0.5-2.0
        // Don't apply immediately - wait for mouse release to avoid jarring changes
        return;
    }
    
    // Handle Game Zoom specially (float, different range)
    if (active_slider == SLIDER_GAME_ZOOM) {
        pending_game_zoom = 0.5f + (pct * 1.5f); // Map 0.0-1.0 to 0.5-2.0
        // Don't apply immediately - wait for mouse release to avoid jarring changes
        return;
    }
    
    if (!val_ptr) return;

    if (active_slider == SLIDER_AFK) *val_ptr = 2 + (int)(pct * 8.0f + 0.5f);
    else *val_ptr = (int)(pct * max_val);

    // Apply Immediate Effects
    if (active_slider == SLIDER_VOL) Mix_VolumeMusic(music_volume);
    
    if (update_color) {
        Uint32 nowc = SDL_GetTicks();
        if (nowc - last_color_packet_ms > 100) { // throttle to ~10 packets/sec during drag
            Packet pkt; pkt.type = PACKET_COLOR_CHANGE;
            pkt.r = my_r; pkt.g = my_g; pkt.b = my_b;
            pkt.r2 = my_r2; pkt.g2 = my_g2; pkt.b2 = my_b2;
            send_packet(&pkt);
            last_color_packet_ms = nowc;
        }
        
        // Force Local Update (For Game View Preview)
        for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].active && local_players[i].id == local_player_id) { 
            local_players[i].r = my_r; local_players[i].g = my_g; local_players[i].b = my_b;
            local_players[i].r2 = my_r2; local_players[i].g2 = my_g2; local_players[i].b2 = my_b2;
        }
    }
    save_config(); 
}

void render_fancy_slider(SDL_Renderer *renderer, SDL_Rect *rect, float pct, SDL_Color fill_col) {
    // 1. Draw Hit Area Debug (Optional, disabled)
    // SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); SDL_RenderDrawRect(renderer, rect);

    // 2. Draw Track (Thin line centered vertically)
    int track_h = 4;
    int track_y = rect->y + (rect->h / 2) - (track_h / 2);
    SDL_Rect track = {rect->x, track_y, rect->w, track_h};

    // Track Background (Dark)
    SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
    SDL_RenderFillRect(renderer, &track);

    // Track Fill (Colored)
    SDL_Rect filled = {track.x, track.y, (int)(track.w * pct), track.h};
    SDL_SetRenderDrawColor(renderer, fill_col.r, fill_col.g, fill_col.b, 255);
    SDL_RenderFillRect(renderer, &filled);

    // 3. Draw Knob (Centered)
    int knob_w = 12;
    int knob_h = 20;
    int knob_x = rect->x + (int)(rect->w * pct) - (knob_w / 2);
    int knob_y = rect->y + (rect->h / 2) - (knob_h / 2);

    SDL_Rect knob = {knob_x, knob_y, knob_w, knob_h};

    // Knob Shadow
    SDL_Rect shadow = knob; shadow.x += 1; shadow.y += 1;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 100);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderFillRect(renderer, &shadow);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // Knob Body
    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
    SDL_RenderFillRect(renderer, &knob);
    
    // Knob Border
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderDrawRect(renderer, &knob);
}

void render_settings_menu(SDL_Renderer *renderer, int screen_w, int screen_h) {
    if (!is_settings_open) return;
    
    // 1. Setup Main Window (Fixed Size)
    int win_h = 600;
    settings_win = (SDL_Rect){screen_w/2 - 175, screen_h/2 - 300, 350, win_h}; 
    
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255); SDL_RenderFillRect(renderer, &settings_win);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &settings_win);
    render_text(renderer, "Settings", settings_win.x + 175, settings_win.y + 10, col_white, 1);

    // 2. Setup Clipping Region (Content Area)
    settings_view_port = (SDL_Rect){settings_win.x + 10, settings_win.y + 40, settings_win.w - 20, settings_win.h - 50};
    SDL_RenderSetClipRect(renderer, &settings_view_port);

    // 3. Render Content (Offset by settings_scroll_y)
    int start_x = settings_win.x + 20;
    int y = settings_win.y + 50 - settings_scroll_y; // Apply Scroll
    
    // -- Toggles --
    btn_toggle_debug = (SDL_Rect){start_x, y, 20, 20};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &btn_toggle_debug);
    if (show_debug_info) { SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_Rect c={btn_toggle_debug.x+4,btn_toggle_debug.y+4,12,12}; SDL_RenderFillRect(renderer,&c); }
    render_text(renderer, "Show Debug Info", start_x + 30, y, col_white, 0); y += 40;

    btn_toggle_fps = (SDL_Rect){start_x, y, 20, 20};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &btn_toggle_fps);
    if (show_fps) { SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_Rect c={btn_toggle_fps.x+4,btn_toggle_fps.y+4,12,12}; SDL_RenderFillRect(renderer,&c); }
    render_text(renderer, "Show FPS", start_x + 30, y, col_white, 0); y += 40;

    btn_toggle_coords = (SDL_Rect){start_x, y, 20, 20};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &btn_toggle_coords);
    if (show_coords) { SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_Rect c={btn_toggle_coords.x+4,btn_toggle_coords.y+4,12,12}; SDL_RenderFillRect(renderer,&c); }
    render_text(renderer, "Show Coordinates", start_x + 30, y, col_white, 0); y += 40;

    btn_toggle_unread = (SDL_Rect){start_x, y, 20, 20};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &btn_toggle_unread);
    if (show_unread_counter) { SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_Rect c={btn_toggle_unread.x+4,btn_toggle_unread.y+4,12,12}; SDL_RenderFillRect(renderer,&c); }
    render_text(renderer, "Show Unread Counter", start_x + 30, y, col_white, 0); y += 40;

    // -- Buttons --
    btn_view_blocked = (SDL_Rect){start_x, y, 300, 30};
    SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255); SDL_RenderFillRect(renderer, &btn_view_blocked);
    render_text(renderer, "Manage Blocked Players", btn_view_blocked.x + 150, btn_view_blocked.y + 5, col_white, 1);
    y += 40;

    char id_str[32]; snprintf(id_str, 32, "My ID: %d", local_player_id);
    render_text(renderer, id_str, settings_win.x + 175, y, (SDL_Color){150, 150, 255, 255}, 1);
    y += 25;

    int my_status = 0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == local_player_id) my_status = local_players[i].status;
    btn_cycle_status = (SDL_Rect){start_x, y, 300, 30};
    SDL_SetRenderDrawColor(renderer, 50, 50, 100, 255); SDL_RenderFillRect(renderer, &btn_cycle_status);
    char status_str[64]; snprintf(status_str, 64, "Status: %s", status_names[my_status]);
    render_text(renderer, status_str, btn_cycle_status.x + 150, btn_cycle_status.y + 5, get_status_color(my_status), 1);
    y += 40;

    SDL_Rect btn_nick = {start_x, y, 300, 30};
    SDL_SetRenderDrawColor(renderer, 100, 50, 150, 255); SDL_RenderFillRect(renderer, &btn_nick);
    render_text(renderer, "Change Nickname", btn_nick.x + 150, btn_nick.y + 5, col_white, 1);
    y += 40;
 
    

    render_text(renderer, "Name Color (Start)", settings_win.x + 175, y, (SDL_Color){my_r, my_g, my_b, 255}, 1); 
    y += 25; // Header Gap
    
    slider_r = (SDL_Rect){start_x + 30, y, 240, 15};
    render_fancy_slider(renderer, &slider_r, my_r/255.0f, (SDL_Color){255, 50, 50, 255}); 
    y += 40; // Increased spacing

    slider_g = (SDL_Rect){start_x + 30, y, 240, 15};
    render_fancy_slider(renderer, &slider_g, my_g/255.0f, (SDL_Color){50, 255, 50, 255}); 
    y += 40;

    slider_b = (SDL_Rect){start_x + 30, y, 240, 15};
    render_fancy_slider(renderer, &slider_b, my_b/255.0f, (SDL_Color){50, 50, 255, 255}); 
    y += 50; // Extra gap before next section title

    // -- Color Sliders 2 (End) --
    render_text(renderer, "Name Color (End)", settings_win.x + 175, y, (SDL_Color){my_r2, my_g2, my_b2, 255}, 1); 
    y += 25;

    slider_r2 = (SDL_Rect){start_x + 30, y, 240, 15};
    render_fancy_slider(renderer, &slider_r2, my_r2/255.0f, (SDL_Color){255, 50, 50, 255}); 
    y += 40;

    slider_g2 = (SDL_Rect){start_x + 30, y, 240, 15};
    render_fancy_slider(renderer, &slider_g2, my_g2/255.0f, (SDL_Color){50, 255, 50, 255}); 
    y += 40;

    slider_b2 = (SDL_Rect){start_x + 30, y, 240, 15};
    render_fancy_slider(renderer, &slider_b2, my_b2/255.0f, (SDL_Color){50, 50, 255, 255}); 
    y += 50; // Extra gap

    // -- Volume --
    render_text(renderer, "Music Volume", settings_win.x + 175, y, col_white, 1); 
    y += 25;
    slider_volume = (SDL_Rect){start_x + 30, y, 240, 15};
    render_fancy_slider(renderer, &slider_volume, music_volume/128.0f, (SDL_Color){0, 200, 255, 255}); 
    y += 50;

    // -- AFK --
    char afk_str[64]; snprintf(afk_str, 64, "Auto-AFK: %d min", afk_timeout_minutes);
    render_text(renderer, afk_str, settings_win.x + 175, y, col_white, 1); 
    y += 25;
    
    slider_afk = (SDL_Rect){start_x + 30, y, 240, 15};
    float afk_pct = (afk_timeout_minutes - 2) / 8.0f;
    render_fancy_slider(renderer, &slider_afk, afk_pct, (SDL_Color){255, 165, 0, 255});
    
    render_text(renderer, "2m", slider_afk.x - 20, slider_afk.y + 2, col_white, 0);  
    render_text(renderer, "10m", slider_afk.x + 245, slider_afk.y + 2, col_white, 0); 
    y += 50;

    // -- UI Scale --
    // Show pending scale if dragging, otherwise show current scale
    float display_scale = (active_slider == SLIDER_UI_SCALE) ? pending_ui_scale : ui_scale;
    char scale_str[64]; snprintf(scale_str, 64, "UI Scale: %.1fx", display_scale);
    render_text(renderer, scale_str, settings_win.x + 175, y, col_white, 1); 
    y += 25;
    
    slider_ui_scale = (SDL_Rect){start_x + 30, y, 240, 15};
    float scale_pct = (display_scale - 0.5f) / 1.5f; // Map 0.5-2.0 to 0.0-1.0
    render_fancy_slider(renderer, &slider_ui_scale, scale_pct, (SDL_Color){150, 150, 255, 255});
    
    render_text(renderer, "0.5x", slider_ui_scale.x - 25, slider_ui_scale.y + 2, col_white, 0);  
    render_text(renderer, "2.0x", slider_ui_scale.x + 245, slider_ui_scale.y + 2, col_white, 0); 
    y += 50;
    
    // -- Game World Zoom --
    // Show pending zoom if dragging, otherwise show current zoom
    float display_zoom = (active_slider == SLIDER_GAME_ZOOM) ? pending_game_zoom : game_zoom;
    char zoom_str[64]; snprintf(zoom_str, 64, "Game Zoom: %.1fx", display_zoom);
    render_text(renderer, zoom_str, settings_win.x + 175, y, col_white, 1); 
    y += 25;
    
    slider_game_zoom = (SDL_Rect){start_x + 30, y, 240, 15};
    float zoom_pct = (display_zoom - 0.5f) / 1.5f; // Map 0.5-2.0 to 0.0-1.0
    render_fancy_slider(renderer, &slider_game_zoom, zoom_pct, (SDL_Color){100, 200, 100, 255});
    
    render_text(renderer, "0.5x", slider_game_zoom.x - 25, slider_game_zoom.y + 2, col_white, 0);  
    render_text(renderer, "2.0x", slider_game_zoom.x + 245, slider_game_zoom.y + 2, col_white, 0); 
    y += 50;

   // -- Disconnect --
    btn_disconnect_rect = (SDL_Rect){start_x, y, 300, 30};
    SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_disconnect_rect);
    render_text(renderer, "Disconnect / Logout", btn_disconnect_rect.x + 150, btn_disconnect_rect.y + 5, col_white, 1); 
    y += 40;

    // --- Docs / Staff / Credits Row ---
    int btn_w = 95; 
    
    // 1. Docs
    btn_documentation_rect = (SDL_Rect){start_x, y, btn_w, 30};
    SDL_SetRenderDrawColor(renderer, 0, 100, 150, 255); SDL_RenderFillRect(renderer, &btn_documentation_rect);
    render_text(renderer, "Docs", btn_documentation_rect.x + 47, btn_documentation_rect.y + 5, col_white, 1);

    // 2. Staff
    btn_staff_list_rect = (SDL_Rect){start_x + 100, y, btn_w, 30};
    SDL_SetRenderDrawColor(renderer, 150, 100, 0, 255); SDL_RenderFillRect(renderer, &btn_staff_list_rect);
    render_text(renderer, "Staff", btn_staff_list_rect.x + 47, btn_staff_list_rect.y + 5, col_white, 1);

    // 3. Credits
    btn_contributors_rect = (SDL_Rect){start_x + 200, y, btn_w, 30};
    SDL_SetRenderDrawColor(renderer, 0, 150, 100, 255); SDL_RenderFillRect(renderer, &btn_contributors_rect);
    render_text(renderer, "Credits", btn_contributors_rect.x + 47, btn_contributors_rect.y + 5, col_white, 1);
    
    y += 40; // Move down for next row

    // --- My Warnings Button ---
    btn_my_warnings = (SDL_Rect){start_x, y, 300, 30};
    SDL_SetRenderDrawColor(renderer, 200, 100, 0, 255); SDL_RenderFillRect(renderer, &btn_my_warnings);
    render_text(renderer, "View My Warnings", btn_my_warnings.x + 150, btn_my_warnings.y + 5, col_white, 1);

    y += 45; // <--- FIX: Added gap so text doesn't overlap button

    // --- Footer Text ---
    render_text(renderer, "Drag & Drop Image here", settings_win.x + 175, y, col_yellow, 1); y += 20;
    render_text(renderer, "to upload Avatar (<16KB)", settings_win.x + 175, y, col_yellow, 1); y += 30;

    // CALCULATE CONTENT HEIGHT
    settings_content_h = y - settings_win.y + settings_scroll_y;
    // Reset Clip
    SDL_RenderSetClipRect(renderer, NULL);

    // --- POPUPS (Draw OUTSIDE Clip so they float above) ---
    // (Nick popup code logic remains the same, drawn here)
    if (show_nick_popup) {
        SDL_Rect pop = {screen_w/2 - 150, screen_h/2 - 150, 300, 300};
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); SDL_RenderFillRect(renderer, &pop);
        SDL_SetRenderDrawColor(renderer, 200, 200, 0, 255); SDL_RenderDrawRect(renderer, &pop);
        
        render_text(renderer, "Change Nickname", pop.x+150, pop.y+10, col_yellow, 1);
        render_text(renderer, auth_message, pop.x+150, pop.y+35, col_red, 1);

        int py = pop.y + 60;
        render_text(renderer, "New Name:", pop.x+20, py, col_white, 0);
        render_input_with_cursor(renderer, (SDL_Rect){pop.x+20, py+20, 260, 25}, nick_new, active_field==10, 0);

        py += 60;
        render_text(renderer, "Type 'CONFIRM':", pop.x+20, py, col_white, 0);
        render_input_with_cursor(renderer, (SDL_Rect){pop.x+20, py+20, 260, 25}, nick_confirm, active_field==11, 0);

        py += 60;
        render_text(renderer, "Current Password:", pop.x+20, py, col_white, 0);
        // --- FIX: Use 'py' instead of 'y' here ---
        render_input_with_cursor(renderer, (SDL_Rect){pop.x+20, py+20, 200, 25}, nick_pass, active_field==12, !show_nick_pass); 

        // Checkbox (Use 'py' here too)
        btn_show_nick_pass = (SDL_Rect){pop.x + 230, py + 25, 15, 15};
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); 
        SDL_RenderDrawRect(renderer, &btn_show_nick_pass);
        
        if (show_nick_pass) {
            SDL_Rect inner = {btn_show_nick_pass.x+3, btn_show_nick_pass.y+3, 9, 9};
            SDL_RenderFillRect(renderer, &inner);
        }
        render_text(renderer, "Show", btn_show_nick_pass.x + 20, btn_show_nick_pass.y - 2, col_white, 0);
        // -----------------------------------------

        SDL_Rect btn_submit = {pop.x+20, pop.y+240, 120, 30};
        SDL_Rect btn_cancel = {pop.x+160, pop.y+240, 120, 30};
        SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_submit);
        render_text(renderer, "Submit", btn_submit.x+60, btn_submit.y+5, col_white, 1);
        SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_cancel);
        render_text(renderer, "Cancel", btn_cancel.x+60, btn_cancel.y+5, col_white, 1);
    }
}

void render_friend_list(SDL_Renderer *renderer, int w, int h) {
    if (!show_friend_list) return;

    int max_text_w = 200; 
    for(int i=0; i<friend_count; i++) {
        char temp_str[128];
        if (my_friends[i].is_online) snprintf(temp_str, 128, "%s (Online)", my_friends[i].username);
        else snprintf(temp_str, 128, "%s (Last: %s)", my_friends[i].username, my_friends[i].last_login);
        int fw, fh; TTF_SizeText(font, temp_str, &fw, &fh);
        if (fw > max_text_w) max_text_w = fw;
    }

    int win_w = max_text_w + 110; if (win_w < 300) win_w = 300; 
    friend_list_win = (SDL_Rect){w/2 - (win_w/2), h/2 - 200, win_w, 400};

    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); SDL_RenderFillRect(renderer, &friend_list_win);
    SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255); SDL_RenderDrawRect(renderer, &friend_list_win);
    
    int title_w, title_h; TTF_SizeText(font, "Friends List", &title_w, &title_h);
    render_text(renderer, "Friends List", friend_list_win.x + (win_w/2) - (title_w/2), friend_list_win.y + 10, col_green, 0);

    // --- CLOSE BUTTON GLOBAL UPDATE ---
    btn_friend_close_rect = (SDL_Rect){friend_list_win.x + win_w - 40, friend_list_win.y + 5, 30, 30};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_friend_close_rect);
    render_text(renderer, "X", btn_friend_close_rect.x + 10, btn_friend_close_rect.y + 5, col_white, 1);
    // ----------------------------------

    btn_friend_add_id_rect = (SDL_Rect){friend_list_win.x + 20, friend_list_win.y + 45, 100, 30};
    SDL_SetRenderDrawColor(renderer, 50, 50, 150, 255); SDL_RenderFillRect(renderer, &btn_friend_add_id_rect);
    render_text(renderer, "+ ID", btn_friend_add_id_rect.x+30, btn_friend_add_id_rect.y+5, col_white, 0);

    // Set up clipping for scrollable area
    SDL_Rect clip_rect = {friend_list_win.x, friend_list_win.y + 80, friend_list_win.w, friend_list_win.h - 80};
    SDL_RenderSetClipRect(renderer, &clip_rect);
    
    int y_off = 85 - friend_list_scroll; // Apply scroll offset
    for(int i=0; i<friend_count; i++) {
        // Only render if within visible area
        if (y_off + 30 > 80 && y_off < friend_list_win.h) {
            char display[128];
            SDL_Color text_col = col_white;
            if (my_friends[i].is_online) { snprintf(display, 128, "%s (Online)", my_friends[i].username); text_col = col_green; } 
            else { snprintf(display, 128, "%s (Last: %s)", my_friends[i].username, my_friends[i].last_login); text_col = (SDL_Color){150, 150, 150, 255}; }
            render_text(renderer, display, friend_list_win.x + 20, friend_list_win.y + y_off, text_col, 0);

            SDL_Rect btn_del = {friend_list_win.x + win_w - 50, friend_list_win.y + y_off, 40, 20};
            SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_del);
            render_text(renderer, "Del", btn_del.x+8, btn_del.y+2, col_white, 0);
        }
        y_off += 30;
    }
    
    // Reset clipping
    SDL_RenderSetClipRect(renderer, NULL);
}

void render_popup(SDL_Renderer *renderer, int w, int h) {
    if (pending_friend_req_id == -1) return;
    popup_win = (SDL_Rect){w/2-150, h/2-60, 300, 120};
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255); SDL_RenderFillRect(renderer, &popup_win);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &popup_win);
    char msg[64]; snprintf(msg, 64, "Friend Request from %s", pending_friend_name);
    render_text(renderer, msg, popup_win.x + 150, popup_win.y + 20, col_yellow, 1);
    SDL_Rect btn_accept = {popup_win.x + 20, popup_win.y + 70, 120, 30}; SDL_Rect btn_deny = {popup_win.x + 160, popup_win.y + 70, 120, 30};
    SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_accept);
    render_text(renderer, "Accept", btn_accept.x + 60, btn_accept.y + 5, col_white, 1);
    SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_deny);
    render_text(renderer, "Deny", btn_deny.x + 60, btn_deny.y + 5, col_white, 1);
}

void render_profile(SDL_Renderer *renderer) {
    if (selected_player_id == -1 || selected_player_id == local_player_id) return;
    
    // 1. Get Data
    int exists = 0; char *name = "Unknown"; int status = 0; int idx = -1; int role = 0;
    for(int i=0; i<MAX_CLIENTS; i++) 
        if(local_players[i].id == selected_player_id) { 
            exists=1; name=local_players[i].username; status=local_players[i].status; role=local_players[i].role; idx = i;
        }
    if (!exists) { selected_player_id = -1; return; }

    int text_w = 0, text_h = 0; TTF_SizeText(font, name, &text_w, &text_h);
    int required_w = 100 + text_w + 20; if (required_w < 200) required_w = 200;

    profile_win.w = required_w; 
    profile_win.h = 280; // Base height

    // Check My Role to adjust height for Admin Button
    int my_role = 0; 
    for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == local_player_id) my_role = local_players[i].role;
    if (my_role >= ROLE_ADMIN) profile_win.h = 330; // Taller for admin

    SDL_SetRenderDrawColor(renderer, 30, 30, 50, 230); SDL_RenderFillRect(renderer, &profile_win);
    SDL_SetRenderDrawColor(renderer, 200, 200, 255, 255); SDL_RenderDrawRect(renderer, &profile_win);
    
    // Avatar
    SDL_Rect avatar_rect = {profile_win.x + 20, profile_win.y + 20, 64, 64};
    if (avatar_status[idx] == 0) { Packet req; req.type = PACKET_AVATAR_REQUEST; req.target_id = selected_player_id; send_packet(&req); avatar_status[idx] = 1; }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderFillRect(renderer, &avatar_rect);
    if (avatar_status[idx] == 2 && avatar_cache[idx]) SDL_RenderCopy(renderer, avatar_cache[idx], NULL, &avatar_rect);
    else render_text(renderer, "?", avatar_rect.x + 25, avatar_rect.y + 20, col_white, 0);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDrawRect(renderer, &avatar_rect);

    int text_x = profile_win.x + 100;

    // Name & Role
    render_text(renderer, name, text_x, profile_win.y + 15, col_white, 0);
    if (role != 0) { 
        const char* r_name = get_role_name(role);
        int rw, rh; TTF_SizeText(font, r_name, &rw, &rh);
        SDL_Rect role_box = {text_x, profile_win.y + 40, rw + 10, rh};
        SDL_Color rc = get_role_color(role);
        SDL_SetRenderDrawColor(renderer, rc.r, rc.g, rc.b, 255); SDL_RenderFillRect(renderer, &role_box);
        render_text(renderer, r_name, text_x + 5, profile_win.y + 40, col_black, 0);
    }

    char id_str[32]; snprintf(id_str, 32, "ID: %d", selected_player_id);
    render_text(renderer, id_str, text_x, profile_win.y + 65, (SDL_Color){150,150,150,255}, 0);
    render_text(renderer, status_names[status], text_x, profile_win.y + 90, get_status_color(status), 0);

    // Buttons
    int btn_w = profile_win.w - 40; int start_y = profile_win.y + 120;
    
    int is_friend = 0; 
    for(int i=0; i<friend_count; i++) if(my_friends[i].id == selected_player_id) is_friend = 1;

    SDL_Rect btn = {profile_win.x + 20, start_y, btn_w, 30}; 
    if (!is_friend) { SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255); SDL_RenderFillRect(renderer, &btn); render_text(renderer, "+ Add Friend", btn.x + (btn_w/2) - 40, btn.y + 5, col_white, 0); } 
    else { SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn); render_text(renderer, "- Remove", btn.x + (btn_w/2) - 30, btn.y + 5, col_white, 0); }
    btn_add_friend = btn;

    btn_send_pm = (SDL_Rect){profile_win.x + 20, start_y + 40, btn_w, 30};
    SDL_SetRenderDrawColor(renderer, 100, 0, 100, 255); SDL_RenderFillRect(renderer, &btn_send_pm);
    render_text(renderer, "Message", btn_send_pm.x + (btn_w/2) - 30, btn_send_pm.y + 5, col_white, 0);

    SDL_Rect btn_hide = {profile_win.x + 20, start_y + 80, btn_w, 30};
    int hidden = is_blocked(selected_player_id);
    if (hidden) { SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_hide); render_text(renderer, "Unhide", btn_hide.x + (btn_w/2) - 20, btn_hide.y + 5, col_white, 0); } 
    else { SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255); SDL_RenderFillRect(renderer, &btn_hide); render_text(renderer, "Hide", btn_hide.x + (btn_w/2) - 20, btn_hide.y + 5, col_white, 0); }
    btn_hide_player_dyn = btn_hide;

    // --- NEW: Sanction Button (Admin Only) ---
    if (my_role >= ROLE_ADMIN) {
        btn_sanction_open = (SDL_Rect){profile_win.x + 20, start_y + 120, btn_w, 30};
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_sanction_open);
        render_text(renderer, "SANCTION", btn_sanction_open.x + (btn_w/2) - 35, btn_sanction_open.y + 5, col_white, 0);
    }
}

void render_auth_screen(SDL_Renderer *renderer) {
    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
    auth_box = (SDL_Rect){w/2-200, h/2-200, 400, 400}; 
    
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); SDL_RenderFillRect(renderer, &auth_box);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &auth_box);
    
    render_text(renderer, "C-MMO Login", auth_box.x + 200, auth_box.y + 20, col_white, 1);
    render_text(renderer, auth_message, auth_box.x + 200, auth_box.y + 50, col_red, 1);

    int y_start = auth_box.y + 80;

    // 1. IP Field
    render_text(renderer, "Server IP:", auth_box.x + 20, y_start, col_white, 0);
    render_input_with_cursor(renderer, (SDL_Rect){auth_box.x + 130, y_start - 5, 200, 25}, input_ip, active_field == 2, 0);

    // 2. Port Field
    render_text(renderer, "Port:", auth_box.x + 20, y_start + 40, col_white, 0);
    render_input_with_cursor(renderer, (SDL_Rect){auth_box.x + 130, y_start + 35, 80, 25}, input_port, active_field == 3, 0);

    // 3. Username
    y_start += 90;
    render_text(renderer, "Username:", auth_box.x + 20, y_start, col_white, 0);
    render_input_with_cursor(renderer, (SDL_Rect){auth_box.x + 130, y_start - 5, 200, 25}, auth_username, active_field == 0, 0);

    // 4. Password
    render_text(renderer, "Password:", auth_box.x + 20, y_start + 50, col_white, 0);
    render_input_with_cursor(renderer, (SDL_Rect){auth_box.x + 130, y_start + 45, 200, 25}, auth_password, active_field == 1, !show_password);
    
    btn_show_pass = (SDL_Rect){auth_box.x + 340, y_start + 50, 15, 15};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDrawRect(renderer, &btn_show_pass);
    if (show_password) { SDL_Rect inner = {btn_show_pass.x + 3, btn_show_pass.y + 3, btn_show_pass.w - 6, btn_show_pass.h - 6}; SDL_RenderFillRect(renderer, &inner); }
    render_text(renderer, "Show", btn_show_pass.x + 20, btn_show_pass.y, col_white, 0);

    // Buttons & Server/Profile Lists
    btn_login = (SDL_Rect){auth_box.x+20, auth_box.y+280, 160, 40};
    btn_register = (SDL_Rect){auth_box.x+220, auth_box.y+280, 160, 40};
    btn_open_servers = (SDL_Rect){auth_box.x+20, auth_box.y+340, 180, 30};
    btn_open_profiles = (SDL_Rect){auth_box.x+210, auth_box.y+340, 170, 30};

    SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_login);
    render_text(renderer, "Login", btn_login.x + 80, btn_login.y + 10, col_white, 1);
    SDL_SetRenderDrawColor(renderer, 0, 0, 150, 255); SDL_RenderFillRect(renderer, &btn_register);
    render_text(renderer, "Register", btn_register.x + 80, btn_register.y + 10, col_white, 1);
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_open_servers);
    render_text(renderer, "Saved Servers", btn_open_servers.x + 90, btn_open_servers.y + 5, col_white, 1);
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_open_profiles);
    render_text(renderer, "Saved Profiles", btn_open_profiles.x + 85, btn_open_profiles.y + 5, col_white, 1);

    if (show_server_list) {
        server_list_win = (SDL_Rect){w/2 + 220, h/2 - 200, 250, 400}; 
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255); SDL_RenderFillRect(renderer, &server_list_win);
        SDL_SetRenderDrawColor(renderer, 200, 200, 0, 255); SDL_RenderDrawRect(renderer, &server_list_win);
        render_text(renderer, "Select Server", server_list_win.x + 125, server_list_win.y + 10, col_yellow, 1);
        
        // Close button
        btn_close_servers = (SDL_Rect){server_list_win.x + 210, server_list_win.y + 5, 30, 30};
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_close_servers);
        render_text(renderer, "X", btn_close_servers.x + 15, btn_close_servers.y + 5, col_white, 1);
        
        int y_s = 50;
        for(int i=0; i<server_count; i++) {
            SDL_Rect row = {server_list_win.x + 10, server_list_win.y + y_s, 230, 30};
            SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255); SDL_RenderFillRect(renderer, &row);
            char label[64]; snprintf(label, 64, "%s (%s)", server_list[i].name, server_list[i].ip);
            render_text(renderer, label, row.x + 5, row.y + 5, col_white, 0);
            y_s += 35;
        }
        btn_add_server = (SDL_Rect){server_list_win.x + 20, server_list_win.y + 350, 210, 30};
        SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255); SDL_RenderFillRect(renderer, &btn_add_server);
        render_text(renderer, "Save Current IP", btn_add_server.x + 105, btn_add_server.y + 5, col_white, 1);
    }
    
    if (show_profile_list) {
        profile_list_win = (SDL_Rect){w/2 - 470, h/2 - 200, 250, 400}; 
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255); SDL_RenderFillRect(renderer, &profile_list_win);
        SDL_SetRenderDrawColor(renderer, 0, 200, 200, 255); SDL_RenderDrawRect(renderer, &profile_list_win);
        render_text(renderer, "Select Profile", profile_list_win.x + 125, profile_list_win.y + 10, col_cyan, 1);
        
        // Close button
        btn_close_profiles = (SDL_Rect){profile_list_win.x + 210, profile_list_win.y + 5, 30, 30};
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_close_profiles);
        render_text(renderer, "X", btn_close_profiles.x + 15, btn_close_profiles.y + 5, col_white, 1);
        
        int y_p = 50;
        for(int i=0; i<profile_count; i++) {
            SDL_Rect row = {profile_list_win.x + 10, profile_list_win.y + y_p, 230, 30};
            SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255); SDL_RenderFillRect(renderer, &row);
            render_text(renderer, profile_list[i].username, row.x + 5, row.y + 5, col_white, 0);
            y_p += 35;
        }
        btn_add_profile = (SDL_Rect){profile_list_win.x + 20, profile_list_win.y + 350, 210, 30};
        SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255); SDL_RenderFillRect(renderer, &btn_add_profile);
        render_text(renderer, "Save Current", btn_add_profile.x + 105, btn_add_profile.y + 5, col_white, 1);
    }
    SDL_RenderPresent(renderer);
}

void render_blocked_list(SDL_Renderer *renderer, int w, int h) {
    if (!show_blocked_list) return;

    blocked_win_rect = (SDL_Rect){w/2 - 150, h/2 - 200, 300, 400}; // Use global
    
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); SDL_RenderFillRect(renderer, &blocked_win_rect);
    SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255); SDL_RenderDrawRect(renderer, &blocked_win_rect);
    
    render_text(renderer, "Blocked Players", blocked_win_rect.x + 150, blocked_win_rect.y + 10, col_red, 1);

    // --- CLOSE BUTTON GLOBAL UPDATE ---
    btn_blocked_close_rect = (SDL_Rect){blocked_win_rect.x + 260, blocked_win_rect.y + 5, 30, 30};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_blocked_close_rect);
    render_text(renderer, "X", btn_blocked_close_rect.x + 15, btn_blocked_close_rect.y + 5, col_white, 1);
    // ----------------------------------

    if (blocked_count == 0) {
        render_text(renderer, "(No hidden players)", blocked_win_rect.x + 150, blocked_win_rect.y + 100, col_white, 1);
        return;
    }
    
    // Set up clipping for scrollable area
    SDL_Rect clip_rect = {blocked_win_rect.x, blocked_win_rect.y + 45, blocked_win_rect.w, blocked_win_rect.h - 45};
    SDL_RenderSetClipRect(renderer, &clip_rect);

    int y_off = 50 - blocked_scroll; // Apply scroll offset
    for(int i=0; i<blocked_count; i++) {
        // Only render if within visible area
        if (y_off + 35 > 45 && y_off < blocked_win_rect.h) {
            int id = blocked_ids[i];
            char display[64]; snprintf(display, 64, "ID: %d", id);
            for(int p=0; p<MAX_CLIENTS; p++) if(local_players[p].active && local_players[p].id == id) snprintf(display, 64, "%s", local_players[p].username);
            render_text(renderer, display, blocked_win_rect.x + 20, blocked_win_rect.y + y_off, col_white, 0);

            SDL_Rect btn_unblock = {blocked_win_rect.x + 200, blocked_win_rect.y + y_off, 60, 25};
            SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255); SDL_RenderFillRect(renderer, &btn_unblock);
            render_text(renderer, "Show", btn_unblock.x + 30, btn_unblock.y + 2, col_white, 1);
        }
        y_off += 35;
    }
    
    // Reset clipping
    SDL_RenderSetClipRect(renderer, NULL);
}


void render_contributors(SDL_Renderer *renderer, int w, int h) {
    if (!show_contributors) {
        // Clean up cache when window is closed
        if (cached_contributors_tex) {
            SDL_DestroyTexture(cached_contributors_tex);
            cached_contributors_tex = NULL;
        }
        return;
    }

    // Increased Size: 400x450
    SDL_Rect win = {w/2 - 200, h/2 - 225, 400, 450};
    
    // Create cached texture if needed (iOS performance optimization)
    if (!cached_contributors_tex) {
        // Create a target texture
        cached_contributors_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, 
                                                     SDL_TEXTUREACCESS_TARGET, 400, 450);
        if (cached_contributors_tex) {
            SDL_SetTextureBlendMode(cached_contributors_tex, SDL_BLENDMODE_BLEND);
            
            // Render to texture
            SDL_SetRenderTarget(renderer, cached_contributors_tex);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); 
            SDL_RenderClear(renderer);
            
            // Background
            SDL_Rect bg = {0, 0, 400, 450};
            SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255); 
            SDL_RenderFillRect(renderer, &bg);
            SDL_SetRenderDrawColor(renderer, 200, 200, 255, 255); 
            SDL_RenderDrawRect(renderer, &bg);

            // Content
            render_text(renderer, "Project Contributors", 200, 15, col_cyan, 1);
            
            int y = 60;
            int center_x = 200;

            render_text(renderer, "You, for playing this game!", center_x, y, (SDL_Color){200,200,200,255}, 1); y += 40;
            render_text(renderer, "#Main Developer#", center_x, y, col_white, 1); y += 25;
            render_text(renderer, "HALt The Dragon", center_x, y, col_white, 1); y += 40;
            render_text(renderer, "#AI Assistant#", center_x, y, col_white, 1); y += 25;
            render_text(renderer, "Gemini", center_x, y, col_white, 1); y += 40;
            render_text(renderer, "#Multiplayer Tests#", center_x, y, col_white, 1); y += 25;
            render_text(renderer, "PugzAreCute", center_x, y, col_white, 1); y += 40;
            render_text(renderer, "#Libraries#", center_x, y, col_white, 1); y += 25;
            render_text(renderer, "SDL2, SDL_ttf, SDL_image, SDL_mixer", center_x, y, col_white, 1); y += 25;
            render_text(renderer, "sqlite3", center_x, y, col_white, 1);
            
            // Restore render target
            SDL_SetRenderTarget(renderer, NULL);
            
            contributors_tex_w = 400;
            contributors_tex_h = 450;
        }
    }
    
    // Draw cached texture
    if (cached_contributors_tex) {
        SDL_RenderCopy(renderer, cached_contributors_tex, NULL, &win);
    }
    
    // Draw close button on top (not cached since it needs to be interactive)
    SDL_Rect btn_close = {win.x + 360, win.y + 5, 30, 30};
    SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); 
    SDL_RenderFillRect(renderer, &btn_close);
    render_text(renderer, "X", btn_close.x + 10, btn_close.y + 5, col_white, 0);
}

void render_documentation(SDL_Renderer *renderer, int w, int h) {
    if (!show_documentation) {
        // Clean up cache when window is closed
        if (cached_documentation_tex) {
            SDL_DestroyTexture(cached_documentation_tex);
            cached_documentation_tex = NULL;
        }
        return;
    }

    SDL_Rect win = {w/2 - 250, h/2 - 250, 500, 500};
    
    // Create cached texture if needed (iOS performance optimization)
    if (!cached_documentation_tex) {
        // Create a target texture
        cached_documentation_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, 
                                                      SDL_TEXTUREACCESS_TARGET, 500, 500);
        if (cached_documentation_tex) {
            SDL_SetTextureBlendMode(cached_documentation_tex, SDL_BLENDMODE_BLEND);
            
            // Render to texture
            SDL_SetRenderTarget(renderer, cached_documentation_tex);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); 
            SDL_RenderClear(renderer);
            
            // Background
            SDL_Rect bg = {0, 0, 500, 500};
            SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); 
            SDL_RenderFillRect(renderer, &bg);
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); 
            SDL_RenderDrawRect(renderer, &bg);

            // Content
            render_text(renderer, "Game Documentation", 250, 15, col_green, 1);

            int start_x = 20;
            int y = 60;

            // 1. Text Formatting
            render_text(renderer, "#Text Formatting#", start_x, y, col_yellow, 0); y += 30;
            render_text(renderer, "Style: *Italic*, #Bold#, ~Strike~", start_x, y, col_white, 0); y += 25;
            render_raw_text(renderer, "Usage: wrap text in symbols (*, #, ~)", start_x, y, (SDL_Color){150,150,150,255}, 0); y += 35;

            // 2. Colors
            render_text(renderer, "#Colors#", start_x, y, col_yellow, 0); y += 30;
            render_text(renderer, "^1Red ^2Green ^3Blue ^4Yellow ^5Cyan ^6Magenta", start_x, y, col_white, 0); y += 25;
            render_text(renderer, "^7White ^8Gray ^9Black", start_x, y, col_white, 0); y += 25;
            render_raw_text(renderer, "Usage: type ^ (caret) + number", start_x, y, (SDL_Color){150,150,150,255}, 0); y += 35;

            // 3. Shortcuts
            render_text(renderer, "#Shortcuts & Editing#", start_x, y, col_yellow, 0); y += 30;
            render_text(renderer, "- Select: Shift + Arrows OR Mouse Drag", start_x, y, col_white, 0); y += 25;
            render_text(renderer, "- Copy/Paste: Ctrl+C, Ctrl+V, Ctrl+X", start_x, y, col_white, 0); y += 25;
            render_text(renderer, "- Select All: Ctrl+A", start_x, y, col_white, 0); y += 25;
            render_text(renderer, "- Cursor: Click to move, Arrows to nav", start_x, y, col_white, 0); y += 35;
            
            // Restore render target
            SDL_SetRenderTarget(renderer, NULL);
            
            documentation_tex_w = 500;
            documentation_tex_h = 500;
        }
    }
    
    // Draw cached texture
    if (cached_documentation_tex) {
        SDL_RenderCopy(renderer, cached_documentation_tex, NULL, &win);
    }
    
    // Draw close button on top (not cached since it needs to be interactive)
    SDL_Rect btn_close = {win.x + 460, win.y + 5, 30, 30};
    SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); 
    SDL_RenderFillRect(renderer, &btn_close);
    render_text(renderer, "X", btn_close.x + 10, btn_close.y + 5, col_white, 0);
}


void render_mobile_controls(SDL_Renderer *renderer, int h) {
    #if defined(__IPHONEOS__) || defined(__ANDROID__)
    if (!joystick_active || touch_id_dpad == -1) return;
    
    // Draw Base
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 100);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderFillRect(renderer, &dpad_rect);
    
    // Draw Knob
    int center_x = dpad_rect.x + (dpad_rect.w / 2);
    int center_y = dpad_rect.y + (dpad_rect.h / 2);
    int knob_x = center_x + (vjoy_dx * 40) - 20;
    int knob_y = center_y + (vjoy_dy * 40) - 20;
    
    SDL_Rect knob = {knob_x, knob_y, 40, 40};
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 150);
    SDL_RenderFillRect(renderer, &knob);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    #endif
}

void render_game(SDL_Renderer *renderer) {
    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
    
    // Apply game world zoom
    SDL_RenderSetScale(renderer, game_zoom, game_zoom);
    
    // Adjust dimensions for zoom
    float zoomed_w = w / game_zoom;
    float zoomed_h = h / game_zoom;
    
    float px=0, py=0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].active && local_players[i].id == local_player_id) { px=local_players[i].x; py=local_players[i].y; }
    int cam_x = (int)px - (zoomed_w/2) + 16; int cam_y = (int)py - (zoomed_h/2) + 16;
    
    // Clamp camera to map boundaries (only when viewport is smaller than map)
    if (zoomed_w > map_w) cam_x = -(zoomed_w - map_w)/2; 
    else {
        if (cam_x < 0) cam_x = 0;
        if (cam_x + zoomed_w > map_w) cam_x = map_w - zoomed_w;
    }
    if (zoomed_h > map_h) cam_y = -(zoomed_h - map_h)/2;
    else {
        if (cam_y < 0) cam_y = 0;
        if (cam_y + zoomed_h > map_h) cam_y = map_h - zoomed_h;
    } 

    // 1. Draw Map
    SDL_RenderClear(renderer);

    if (tex_map) { SDL_Rect dst = {-cam_x, -cam_y, map_w, map_h}; SDL_RenderCopy(renderer, tex_map, NULL, &dst); }
    else {
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        for(int x=0; x<map_w; x+=50) SDL_RenderDrawLine(renderer, x-cam_x, 0-cam_y, x-cam_x, map_h-cam_y);
        for(int y=0; y<map_h; y+=50) SDL_RenderDrawLine(renderer, 0-cam_x, y-cam_y, map_w-cam_x, y-cam_y);
    }

    if (show_debug_info) {
        for(int t=0; t<trigger_count; t++) {
            // Only draw triggers for the current map
            if (strcmp(triggers[t].src_map, current_map_file) == 0) {
                SDL_Rect r = triggers[t].rect;
                r.x -= cam_x; // Adjust for camera position
                r.y -= cam_y;
                
                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 100); // Semi-transparent Green
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_RenderFillRect(renderer, &r);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); // Solid Outline
                SDL_RenderDrawRect(renderer, &r);
            }
        }
    }
    // 2. Draw Players & Floating Text
    Uint32 now = SDL_GetTicks();
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (local_players[i].active) {
            if (strcmp(local_players[i].map_name, current_map_file) != 0) continue;
            if (is_blocked(local_players[i].id)) continue;
            
            // Only render players within map boundaries
            if (local_players[i].x < 0 || local_players[i].x > map_w || 
                local_players[i].y < 0 || local_players[i].y > map_h) continue;
            
            SDL_Rect dst = { (int)local_players[i].x - cam_x, (int)local_players[i].y - cam_y, PLAYER_WIDTH, PLAYER_HEIGHT };
            SDL_Color c1 = {local_players[i].r, local_players[i].g, local_players[i].b, 255};
            SDL_Color c2 = {local_players[i].r2, local_players[i].g2, local_players[i].b2, 255};
            
            if (tex_player) SDL_RenderCopy(renderer, tex_player, NULL, &dst);
            else {
                if(local_players[i].id == local_player_id) SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
                else SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
                SDL_RenderFillRect(renderer, &dst);
            }
            if (local_players[i].id == selected_player_id) { 
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDrawRect(renderer, &dst); 
            }
            render_text_gradient(renderer, local_players[i].username, dst.x+16, dst.y - 18, c1, c2, 1);
            
            if (now - floating_texts[i].timestamp < 4000) {
                int fw, fh; TTF_SizeText(font, floating_texts[i].msg, &fw, &fh);
                SDL_Rect bg_rect = { (dst.x + 16) - (fw / 2) - 4, (dst.y - 38) - 2, fw + 8, fh + 4 };
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160); 
                SDL_RenderFillRect(renderer, &bg_rect);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                render_text(renderer, floating_texts[i].msg, dst.x+16, dst.y - 38, col_white, 1);
            }
        }
    }

    // 3. Reset scale before applying UI scale
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);
    
    // 4. Apply UI scaling (affects all UI elements, not game world)
    // Save the current render scale
    float old_scale_x, old_scale_y;
    SDL_RenderGetScale(renderer, &old_scale_x, &old_scale_y);
    
    // Apply UI scale to renderer
    SDL_RenderSetScale(renderer, ui_scale, ui_scale);
    
    // Adjust width/height for scaled rendering
    float scaled_w = w / ui_scale;
    float scaled_h = h / ui_scale;
    
    // 4. Draw UI Windows (Order matters!)
    render_profile(renderer);
    render_popup(renderer, scaled_w, scaled_h);
    render_settings_menu(renderer, scaled_w, scaled_h);
    render_blocked_list(renderer, scaled_w, scaled_h); 
    render_friend_list(renderer, scaled_w, scaled_h);
    render_inbox(renderer, scaled_w, scaled_h);           
    render_add_friend_popup(renderer, scaled_w, scaled_h); 
    render_contributors(renderer, scaled_w, scaled_h);
    render_documentation(renderer, scaled_w, scaled_h);
    render_role_list(renderer, scaled_w, scaled_h);
    render_sanction_popup(renderer, scaled_w, scaled_h);
    render_my_warnings(renderer, scaled_w, scaled_h);
    render_debug_overlay(renderer, scaled_w);
    render_hud(renderer, scaled_w, scaled_h);
    render_mobile_controls(renderer, scaled_w);

    // 5. Draw HUD Buttons
    btn_chat_toggle = (SDL_Rect){10, scaled_h-40, 100, 30};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_chat_toggle);
    render_text(renderer, is_chat_open ? "Close" : "Chat", btn_chat_toggle.x+50, btn_chat_toggle.y+5, col_white, 1);
    // --- NEW: Unread Badge ---
    if (show_unread_counter && unread_chat_count > 0 && !is_chat_open) {
        // Draw Red Box
        SDL_Rect badge = {btn_chat_toggle.x + 80, btn_chat_toggle.y - 10, 20, 20};
        SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255); 
        SDL_RenderFillRect(renderer, &badge);
        
        // Draw Number
        char num[8]; 
        snprintf(num, 8, "%d", unread_chat_count > 9 ? 9 : unread_chat_count); // Cap visual at 9
        render_text(renderer, num, badge.x + 10, badge.y + 2, col_white, 1);
    }

    btn_view_friends = (SDL_Rect){120, scaled_h-40, 100, 30};
    SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255); SDL_RenderFillRect(renderer, &btn_view_friends);
    render_text(renderer, "Friends", btn_view_friends.x+50, btn_view_friends.y+5, col_white, 1);

    btn_settings_toggle = (SDL_Rect){230, scaled_h-40, 100, 30};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_settings_toggle);
    render_text(renderer, "Settings", btn_settings_toggle.x+50, btn_settings_toggle.y+5, col_white, 1);

 // 6. Draw Chat Overlay
    if(is_chat_open) {
        // Calculate chat window position with mobile keyboard shift
        int chat_y = scaled_h-240;
        #if defined(__ANDROID__) || defined(__IPHONEOS__)
        // Shift chat window up when keyboard is active
        if (chat_input_active && keyboard_height > 0) {
            chat_window_shift = keyboard_height / ui_scale;  // Scale the keyboard shift
            chat_y -= chat_window_shift;
            // Ensure chat doesn't go off-screen
            if (chat_y < 50) chat_y = 50;
        } else {
            chat_window_shift = 0;
        }
        #endif
        
        SDL_Rect win = {10, chat_y, 300, 190};
        
        // Set text input hint for mobile keyboard positioning (needs screen coordinates, not scaled)
        #if defined(__ANDROID__) || defined(__IPHONEOS__)
        SDL_Rect input_hint = {(int)(15 * ui_scale), (int)((win.y + win.h - 24) * ui_scale), (int)(270 * ui_scale), (int)(24 * ui_scale)};
        SDL_SetTextInputRect(&input_hint);
        #endif
        
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0,0,0,200); SDL_RenderFillRect(renderer, &win);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        
        SDL_RenderSetClipRect(renderer, &win); 

        // Draw History
        for(int i=0; i<CHAT_HISTORY; i++) {
            SDL_Color line_col = col_white;
            if (strncmp(chat_log[i], "To [", 4) == 0 || strncmp(chat_log[i], "From [", 6) == 0) line_col = col_magenta;
            render_text(renderer, chat_log[i], 15, win.y+10+(i*15), line_col, 0);
        }

        SDL_RenderSetClipRect(renderer, NULL);

        // --- CHAT INPUT & CURSOR LOGIC ---
        char full_str[256];
        char prefix[64];
        SDL_Color input_col = col_cyan;
        
        // 1. Construct Prefix
        if (chat_target_id != -1) {
            char *name = "Unknown";
            for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == chat_target_id) name = local_players[i].username;
            snprintf(prefix, 64, "To %s: ", name);
            input_col = col_magenta;
        } else {
            strcpy(prefix, "> ");
        }

        // 2. Combine for Display
        snprintf(full_str, 256, "%s%s", prefix, input_buffer);

        // --- NEW: Render Chat Selection Highlight ---
        if (selection_len != 0) {
            int start = selection_start;
            int len = selection_len;
            if (len < 0) { start += len; len = -len; }

            // Calculate offset X (Prefix + Text before selection)
            char text_before[256];
            snprintf(text_before, 256, "%s", prefix);
            strncat(text_before, input_buffer, start); // Add buffer up to start index
            
            int w_before = 0, h;
            TTF_SizeText(font, text_before, &w_before, &h);

            // Calculate width of selection
            char text_sel[256];
            strncpy(text_sel, input_buffer + start, len);
            text_sel[len] = 0;
            
            int w_sel = 0;
            if (len > 0) TTF_SizeText(font, text_sel, &w_sel, &h);

            // Draw Blue Box
            int render_x = 15; 
            int render_y = win.y + win.h - 20;
            
            // Note: scroll offset will be applied when rendering, so calculate without it
            SDL_Rect sel_rect = {render_x + w_before, render_y + 2, w_sel, 20};
            
            // Clip to chat window width so it doesn't spill out
            if (sel_rect.x + sel_rect.w > 285) sel_rect.w = 285 - sel_rect.x;

            if (sel_rect.w > 0) {
                SDL_SetRenderDrawColor(renderer, 0, 100, 255, 128);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_RenderFillRect(renderer, &sel_rect);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }
        }

        // 3. Render Text with proper clipping and scrolling
        int w, h;
        TTF_SizeText(font, full_str, &w, &h);
        int render_x = 15;
        int render_y = win.y+win.h-20;
        int available_width = 270;
        
        // Set clipping to chat input area
        SDL_Rect clip_chat = {10, win.y + win.h - 30, 300, 30};
        SDL_RenderSetClipRect(renderer, &clip_chat);
        
        // Calculate cursor position in the full string
        int prefix_len = strlen(prefix);
        int cursor_pos_in_full = prefix_len + cursor_pos;
        
        char temp_to_cursor[256];
        strncpy(temp_to_cursor, full_str, cursor_pos_in_full);
        temp_to_cursor[cursor_pos_in_full] = 0;
        int cursor_x = 0;
        if (cursor_pos_in_full > 0) {
            TTF_SizeText(font, temp_to_cursor, &cursor_x, &h);
        }
        
        // Calculate scroll offset for chat input
        // Note: static variable is fine here as chat input is separate from other fields
        static int chat_scroll_offset = 0;
        if (chat_input_active) {
            if (cursor_x - chat_scroll_offset > available_width - 10) {
                chat_scroll_offset = cursor_x - available_width + 10;
            } else if (cursor_x < chat_scroll_offset) {
                chat_scroll_offset = cursor_x;
            }
        } else {
            chat_scroll_offset = 0;
        }
        
        if (chat_scroll_offset < 0) chat_scroll_offset = 0;
        
        // Render text with scroll offset
        render_raw_text(renderer, full_str, render_x - chat_scroll_offset, render_y, input_col, 0);

        // 4. Render Blinking Cursor
        if ((SDL_GetTicks() / 500) % 2 == 0) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            int cursor_render_x = render_x + cursor_x - chat_scroll_offset;
            SDL_RenderDrawLine(renderer, cursor_render_x, render_y + 2, cursor_render_x, render_y + 14);
        }
        
        // Remove clipping
        SDL_RenderSetClipRect(renderer, NULL);
        // ---------------------------------
    }
    
    // Restore original render scale
    SDL_RenderSetScale(renderer, old_scale_x, old_scale_y);
    
    SDL_RenderPresent(renderer);
}

void handle_game_click(int mx, int my, int cam_x, int cam_y, int w, int h) {

    // ============================================================
    // LAYER 1: HUD BUTTONS
    // ============================================================
    
    SDL_Rect btn_inbox_check = {w - 50, 10, 40, 40};
    if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_inbox_check)) {
        is_inbox_open = !is_inbox_open;
        if (is_inbox_open) { show_friend_list = 0; show_add_friend_popup = 0; is_settings_open = 0; }
        return;
    }

    // --- CHAT INPUT ---
    if (is_chat_open) {
        SDL_Rect input_area = {10, h-70, 300, 30}; 
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &input_area)) {
            int prefix_w = 0, ph; char prefix[64];
            if (chat_target_id != -1) {
                char *name = "Unknown"; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == chat_target_id) name = local_players[i].username;
                snprintf(prefix, 64, "To %s: ", name);
            } else { strcpy(prefix, "> "); }
            TTF_SizeText(font, prefix, &prefix_w, &ph);
            active_input_rect = input_area; active_input_rect.x += (5 + prefix_w); 
            cursor_pos = get_cursor_pos_from_click(input_buffer, mx, active_input_rect.x);
            selection_start = cursor_pos; selection_len = 0; is_dragging = 1;
            chat_input_active = 1; // Activate input
            SDL_StartTextInput(); return;
        }
    }

    // ============================================================
    // LAYER 2: POPUPS & MODALS
    // ============================================================

    // 2A. Sanction Popup
    if (show_sanction_popup) {
        SDL_Rect win = {w/2 - 150, h/2 - 150, 300, 300};
        int y = win.y + 90;
        
        // Mode Buttons
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){win.x+20, win.y+40, 120, 30})) { sanction_mode = 0; return; }
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){win.x+160, win.y+40, 120, 30})) { sanction_mode = 1; return; }

        // Reason Input
        SDL_Rect r_reason = {win.x+20, y+25, 260, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &r_reason)) { 
            active_field = 30; SDL_StartTextInput(); active_input_rect = r_reason;
            cursor_pos = get_cursor_pos_from_click(input_sanction_reason, mx, r_reason.x);
            selection_start = cursor_pos; selection_len=0; is_dragging=1; return; 
        }
        
        // Time Input
        if (sanction_mode == 1) {
            y += 70;
            SDL_Rect r_time = {win.x+20, y+25, 100, 30};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &r_time)) { 
                active_field = 31; SDL_StartTextInput(); active_input_rect = r_time;
                cursor_pos = get_cursor_pos_from_click(input_ban_time, mx, r_time.x);
                selection_start = cursor_pos; selection_len=0; is_dragging=1; return; 
            }
        }

        // Execute
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){win.x+20, win.y+240, 120, 30})) {
            Packet pkt; pkt.type = PACKET_SANCTION_REQUEST;
            pkt.target_id = sanction_target_id;
            pkt.sanction_type = sanction_mode;
            strncpy(pkt.sanction_reason, input_sanction_reason, 63);
            strncpy(pkt.ban_duration, input_ban_time, 15);
            send_packet(&pkt);
            show_sanction_popup = 0; return;
        }
        // Cancel
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){win.x+160, win.y+240, 120, 30})) { show_sanction_popup = 0; return; }
        return;
    }

    // 2B. My Warnings
    if (show_my_warnings) {
        SDL_Rect win = {w/2 - 200, h/2 - 200, 400, 400};
        SDL_Rect btn_close = {win.x + 360, win.y + 5, 30, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_close)) show_my_warnings = 0;
        return;
    }

    // 2C. Inbox
    if (is_inbox_open) {
        SDL_Rect win = {w - 320, 60, 300, 300};
        int y = win.y + 40;
        for(int i=0; i<inbox_count; i++) {
            SDL_Rect btn_acc = {win.x+170, y+25, 50, 20}; SDL_Rect btn_deny = {win.x+230, y+25, 50, 20};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_acc)) {
                Packet pkt; pkt.type = PACKET_FRIEND_RESPONSE; pkt.target_id = inbox[i].id; pkt.response_accepted = 1; send_packet(&pkt);
                for(int k=i; k<inbox_count-1; k++) inbox[k] = inbox[k+1]; inbox_count--; return;
            }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_deny)) {
                Packet pkt; pkt.type = PACKET_FRIEND_RESPONSE; pkt.target_id = inbox[i].id; pkt.response_accepted = 0; send_packet(&pkt);
                for(int k=i; k<inbox_count-1; k++) inbox[k] = inbox[k+1]; inbox_count--; return;
            }
            y += 55;
        }
        if (!SDL_PointInRect(&(SDL_Point){mx, my}, &win)) is_inbox_open = 0; 
        return; 
    }

    // 2D. Add Friend Popup
    if (show_add_friend_popup) {
        SDL_Rect pop = {w/2 - 150, h/2 - 100, 300, 200};
        SDL_Rect input = {pop.x+50, pop.y+60, 200, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &input)) { 
            active_field = 20; SDL_StartTextInput(); active_input_rect = input;
            cursor_pos = get_cursor_pos_from_click(input_friend_id, mx, input.x); 
            selection_start = cursor_pos; selection_len = 0; is_dragging = 1; return; 
        }
        SDL_Rect btn_ok = {pop.x+50, pop.y+130, 80, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_ok)) {
            int id = atoi(input_friend_id);
            if (id > 0) { Packet pkt; pkt.type = PACKET_FRIEND_REQUEST; pkt.target_id = id; send_packet(&pkt); show_add_friend_popup = 0; active_field = -1; } return;
        }
        SDL_Rect btn_cancel = {pop.x+170, pop.y+130, 80, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_cancel)) { show_add_friend_popup = 0; active_field = -1; return; }
        return; 
    }

    // 2E. Blocked List
    if (show_blocked_list) {
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_blocked_close_rect)) { show_blocked_list = 0; return; }
        int y_off = 50;
        for(int i=0; i<blocked_count; i++) {
            SDL_Rect btn_unblock = {blocked_win_rect.x + 200, blocked_win_rect.y + y_off, 60, 25};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_unblock)) { toggle_block(blocked_ids[i]); return; }
            y_off += 35;
        }
        return;
    }

    // 2F. Friends List (FIXED VARIABLE NAME)
    if (show_friend_list) {
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_friend_add_id_rect)) { show_add_friend_popup = 1; input_friend_id[0] = 0; return; }
        
        int win_w = friend_list_win.w; // Used correct global
        int y_off = 85; 
        for(int i=0; i<friend_count; i++) {
            SDL_Rect btn_del = {friend_list_win.x + win_w - 50, friend_list_win.y + y_off, 40, 20};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_del)) { Packet pkt; pkt.type = PACKET_FRIEND_REMOVE; pkt.target_id = my_friends[i].id; send_packet(&pkt); return; }
            y_off += 30;
        }
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_friend_close_rect)) { show_friend_list = 0; return; }
        return; 
    }

    // 2G. Info Windows
    if (show_contributors) {
        SDL_Rect win = {w/2 - 200, h/2 - 225, 400, 450}; 
        SDL_Rect btn_close = {win.x + 360, win.y + 5, 30, 30}; 
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_close)) show_contributors = 0; return; 
    }
    if (show_documentation) {
        SDL_Rect win = {w/2 - 250, h/2 - 250, 500, 500};
        SDL_Rect btn_close = {win.x + 460, win.y + 5, 30, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_close)) show_documentation = 0; return; 
    }
    if (show_role_list) {
        SDL_Rect win = {w/2 - 200, h/2 - 225, 400, 450}; 
        SDL_Rect btn_close = {win.x + 360, win.y + 5, 30, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_close)) show_role_list = 0; return; 
    }

    // ============================================================
    // LAYER 3: SETTINGS MENU
    // ============================================================

    if (is_settings_open) {
        if (show_nick_popup) {
            SDL_Rect pop = {w/2 - 150, h/2 - 150, 300, 300};
            int py = pop.y + 60; 
            SDL_Rect r_new = {pop.x+20, py+20, 260, 25};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &r_new)) { 
                active_field = 10; SDL_StartTextInput(); active_input_rect = r_new; cursor_pos = get_cursor_pos_from_click(nick_new, mx, r_new.x);
                selection_start = cursor_pos; selection_len = 0; is_dragging = 1; return; 
            }
            py += 60;
            SDL_Rect r_con = {pop.x+20, py+20, 260, 25};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &r_con)) { 
                active_field = 11; SDL_StartTextInput(); active_input_rect = r_con; cursor_pos = get_cursor_pos_from_click(nick_confirm, mx, r_con.x);
                selection_start = cursor_pos; selection_len = 0; is_dragging = 1; return; 
            }
            py += 60;
            SDL_Rect r_pass = {pop.x+20, py+20, 200, 25}; 
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &r_pass)) { 
                active_field = 12; SDL_StartTextInput(); active_input_rect = r_pass; cursor_pos = get_cursor_pos_from_click(nick_pass, mx, r_pass.x);
                selection_start = cursor_pos; selection_len = 0; is_dragging = 1; return; 
            }
            SDL_Rect btn_check = {pop.x + 230, py + 25, 15, 15};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_check)) { show_nick_pass = !show_nick_pass; return; }

            if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){pop.x+20, pop.y+240, 120, 30})) {
                Packet pkt; pkt.type = PACKET_CHANGE_NICK_REQUEST;
                strncpy(pkt.username, nick_new, 31); strncpy(pkt.msg, nick_confirm, 63); strncpy(pkt.password, nick_pass, 31);
                send_packet(&pkt); strcpy(auth_message, "Processing..."); return;
            }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){pop.x+160, pop.y+240, 120, 30})) { show_nick_popup = 0; active_field = -1; return; }
            return;
        }

        // Main Settings Scrolled
        SDL_Rect s_win = {w/2 - 175, h/2 - 300, 350, 600}; 
        SDL_Rect viewport = {s_win.x + 10, s_win.y + 40, s_win.w - 20, s_win.h - 50};

        if (SDL_PointInRect(&(SDL_Point){mx, my}, &viewport)) {
            // Toggles
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_toggle_debug)) { show_debug_info = !show_debug_info; save_config(); return; }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_toggle_fps)) { show_fps = !show_fps; save_config(); return; }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_toggle_coords)) { show_coords = !show_coords; save_config(); return; }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_toggle_unread)) { show_unread_counter = !show_unread_counter; save_config(); return; }

            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_view_blocked)) { show_blocked_list = 1; return; } 
            
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_cycle_status)) {
                int my_status = 0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == local_player_id) my_status = local_players[i].status;
                my_status++; if (my_status > 4) my_status = 0; 
                Packet pkt; pkt.type = PACKET_STATUS_CHANGE; pkt.new_status = my_status; send_packet(&pkt); return;
            }

            SDL_Rect btn_nick = {btn_cycle_status.x, btn_cycle_status.y + 40, 300, 30};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_nick)) {
                show_nick_popup = 1; nick_new[0] = 0; nick_confirm[0] = 0; nick_pass[0] = 0; strcpy(auth_message, "Enter details."); return;
            }

            // --- Generic Slider Hit Detection (Replaces all specific slider logic) ---
            SDL_Rect touch_pad = {0,0,0,0};
            int margin = 15; // Hit margin makes them easier to grab with touch

            // Helper macro for cleaner code
            #define CHECK_SLIDER(rect, id) \
                touch_pad = rect; \
                touch_pad.x -= margin; touch_pad.y -= margin; \
                touch_pad.w += margin*2; touch_pad.h += margin*2; \
                if (SDL_PointInRect(&(SDL_Point){mx, my}, &touch_pad)) { \
                    active_slider = id; \
                    process_slider_drag(mx); /* Update immediately on click */ \
                    return; \
                }

            // check all sliders
            CHECK_SLIDER(slider_r, SLIDER_R);
            CHECK_SLIDER(slider_g, SLIDER_G);
            CHECK_SLIDER(slider_b, SLIDER_B);
            
            CHECK_SLIDER(slider_r2, SLIDER_R2);
            CHECK_SLIDER(slider_g2, SLIDER_G2);
            CHECK_SLIDER(slider_b2, SLIDER_B2);
            
            CHECK_SLIDER(slider_volume, SLIDER_VOL);
            CHECK_SLIDER(slider_afk, SLIDER_AFK);
            CHECK_SLIDER(slider_ui_scale, SLIDER_UI_SCALE);
            CHECK_SLIDER(slider_game_zoom, SLIDER_GAME_ZOOM);

            // Bottom Buttons
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_disconnect_rect)) {
                if(sock > 0) close(sock); sock = -1; is_connected = 0; Mix_HaltMusic();
                client_state = STATE_AUTH; is_settings_open = 0; local_player_id = -1;
                for(int i=0; i<MAX_CLIENTS; i++) { local_players[i].active = 0; local_players[i].id = -1; }
                friend_count = 0; chat_log_count = 0; for(int i=0; i<CHAT_HISTORY; i++) strcpy(chat_log[i], "");
                strcpy(auth_message, "Logged out."); return;
            }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_documentation_rect)) { show_documentation = 1; return; }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_contributors_rect)) { show_contributors = 1; return; }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_staff_list_rect)) { 
                show_role_list = 1; staff_count = 0; Packet req; req.type = PACKET_ROLE_LIST_REQUEST; send_packet(&req); return; 
            }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_my_warnings)) {
                show_my_warnings = 1; my_warning_count = 0; Packet req; req.type = PACKET_WARNINGS_REQUEST; send_packet(&req); return;
            }
        }
        return; 
    }

    // ============================================================
    // LAYER 4: GAME WORLD & TOASTS
    // ============================================================

    if (pending_friend_req_id != -1) {
        SDL_Rect btn_accept = {popup_win.x+20, popup_win.y+70, 120, 30};
        SDL_Rect btn_deny = {popup_win.x+160, popup_win.y+70, 120, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_accept)) {
            Packet pkt; pkt.type = PACKET_FRIEND_RESPONSE; pkt.target_id = pending_friend_req_id; pkt.response_accepted = 1; send_packet(&pkt); pending_friend_req_id = -1;
        } else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_deny)) pending_friend_req_id = -1;
        return;
    }

    if (selected_player_id != -1 && selected_player_id != local_player_id) {
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_add_friend)) {
            int is_friend = 0; for(int i=0; i<friend_count; i++) if(my_friends[i].id == selected_player_id) is_friend = 1;
            if(!is_friend) { Packet pkt; pkt.type = PACKET_FRIEND_REQUEST; pkt.target_id = selected_player_id; send_packet(&pkt); } 
            else { Packet pkt; pkt.type = PACKET_FRIEND_REMOVE; pkt.target_id = selected_player_id; send_packet(&pkt); }
            selected_player_id = -1; return;
        }
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_send_pm)) {
            chat_target_id = selected_player_id; is_chat_open = 1; input_buffer[0] = 0; 
            chat_input_active = 1; // Activate input for immediate typing
            SDL_StartTextInput(); 
            selected_player_id = -1; return;
        }
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_hide_player_dyn)) {
            toggle_block(selected_player_id); selected_player_id = -1; return;
        }
        
        // Sanction Button (Admin)
        int my_role = 0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == local_player_id) my_role = local_players[i].role;
        if (my_role >= ROLE_ADMIN && SDL_PointInRect(&(SDL_Point){mx, my}, &btn_sanction_open)) {
            sanction_target_id = selected_player_id;
            show_sanction_popup = 1;
            input_sanction_reason[0] = 0; input_ban_time[0] = 0;
            sanction_mode = 0; 
            return;
        }
    }

    // Check for player clicks in game world (convert scaled UI coords to game world coords)
    // mx, my are in UI coordinate space (divided by ui_scale)
    // Game world is scaled by game_zoom
    // So we need to: multiply by ui_scale to get screen coords, then divide by game_zoom to get game world coords
    int game_mx = (int)((mx * ui_scale) / game_zoom);
    int game_my = (int)((my * ui_scale) / game_zoom);
    
    int clicked = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (local_players[i].active) {
            SDL_Rect r = { (int)local_players[i].x - cam_x, (int)local_players[i].y - cam_y, PLAYER_WIDTH, PLAYER_HEIGHT };
            if (SDL_PointInRect(&(SDL_Point){game_mx, game_my}, &r)) { selected_player_id = local_players[i].id; clicked = 1; }
        }
    }
    
    if (!clicked && !SDL_PointInRect(&(SDL_Point){mx, my}, &profile_win) && !SDL_PointInRect(&(SDL_Point){mx, my}, &btn_chat_toggle) && !SDL_PointInRect(&(SDL_Point){mx, my}, &btn_settings_toggle) && !SDL_PointInRect(&(SDL_Point){mx, my}, &btn_view_friends)) {
        selected_player_id = -1;
    }
}

void handle_auth_click(int mx, int my) {
    // 1. Server List Logic
    if (show_server_list) {
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_close_servers)) {
            show_server_list = 0;
            return;
        }
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_add_server)) {
            char name[32]; 
            snprintf(name, 32, "Server_%d", server_count+1); // Fixed underscore
            add_server_to_list(name, input_ip, atoi(input_port));
            return;
        }
        int y_s = 50;
        for(int i=0; i<server_count; i++) {
            SDL_Rect row = {server_list_win.x + 10, server_list_win.y + y_s, 230, 30};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &row)) {
                strcpy(input_ip, server_list[i].ip);
                sprintf(input_port, "%d", server_list[i].port);
                show_server_list = 0; 
                return;
            }
            y_s += 35;
        }
        if (!SDL_PointInRect(&(SDL_Point){mx, my}, &server_list_win)) show_server_list = 0;
        return; // Return early if list was open to prevent clicking through it
    }
    
    // 2. Profile List Logic
    if (show_profile_list) {
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_close_profiles)) {
            show_profile_list = 0;
            return;
        }
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_add_profile)) {
            if (strlen(auth_username) > 0 && strlen(auth_password) > 0) {
                add_profile_to_list(auth_username, auth_password);
            }
            return;
        }
        int y_p = 50;
        for(int i=0; i<profile_count; i++) {
            SDL_Rect row = {profile_list_win.x + 10, profile_list_win.y + y_p, 230, 30};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &row)) {
                strcpy(auth_username, profile_list[i].username);
                strcpy(auth_password, profile_list[i].password);
                show_profile_list = 0;
                return;
            }
            y_p += 35;
        }
        if (!SDL_PointInRect(&(SDL_Point){mx, my}, &profile_list_win)) show_profile_list = 0;
        return; // Return early if list was open to prevent clicking through it
    }

    if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_open_servers)) {
        show_server_list = !show_server_list;
        return;
    }
    
    if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_open_profiles)) {
        show_profile_list = !show_profile_list;
        return;
    }

    // --- FIX: Restore Password Checkbox Logic ---
    if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_show_pass)) {
        show_password = !show_password;
        return;
    }
    // --------------------------------------------

    // Login / Register Buttons
    if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_login)) {
        if (!is_connected) { if(!try_connect()) return; } 
        Packet pkt; pkt.type = PACKET_LOGIN_REQUEST; 
        strncpy(pkt.username, auth_username, 31); strncpy(pkt.password, auth_password, 31); 
        send_packet(&pkt);
        strcpy(auth_message, "Logging in...");
    } 
    else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_register)) {
        if (!is_connected) { if(!try_connect()) return; } 
        Packet pkt; pkt.type = PACKET_REGISTER_REQUEST; 
        strncpy(pkt.username, auth_username, 31); strncpy(pkt.password, auth_password, 31); 
        send_packet(&pkt);
        strcpy(auth_message, "Registering...");
    } 
    
int y_start = auth_box.y + 80;
// Helper macro to reduce repetition
    #define CHECK_FIELD(rect, id, buffer) \
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &rect)) { \
            active_field = id; \
            SDL_StartTextInput(); \
            active_input_rect = rect; \
            cursor_pos = get_cursor_pos_from_click(buffer, mx, rect.x); \
            selection_start = cursor_pos; \
            selection_len = 0; \
            is_dragging = 1; \
            return; \
        }
    
    SDL_Rect r_ip = {auth_box.x+130, y_start-5, 200, 25};
    CHECK_FIELD(r_ip, 2, input_ip);

    SDL_Rect r_port = {auth_box.x+130, y_start+35, 80, 25};
    CHECK_FIELD(r_port, 3, input_port);

    SDL_Rect r_user = {auth_box.x+130, y_start+85, 200, 25};
    CHECK_FIELD(r_user, 0, auth_username);

    SDL_Rect r_pass = {auth_box.x+130, y_start+135, 200, 25};
    CHECK_FIELD(r_pass, 1, auth_password);
        // Click outside inputs: deactivate
    active_field = -1;
    selection_len = 0;
    selection_start = 0;
    is_dragging = 0;
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    // 1. Init SDL & Libraries
    //printf("DEBUG: Packet Size is %d bytes\n", (int)sizeof(Packet));
    #ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed. Error Code : %d", WSAGetLastError());
        return 1;
    }
    FreeConsole();
    #endif
    #ifdef SDL_HINT_WINDOWS_RAWKEYBOARD
    SDL_SetHint(SDL_HINT_WINDOWS_RAWKEYBOARD, "1");
    #endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) return 1;
    if (TTF_Init() == -1) return 1;
    if (!(IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG))) printf("IMG Init Error: %s\n", IMG_GetError());
    // Probe GL strings even if active renderer is non-GL
    if (!gl_probe_done) {
        SDL_Window *tmpw = SDL_CreateWindow("probe", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1, 1, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        if (tmpw) {
            SDL_GLContext tmpc = SDL_GL_CreateContext(tmpw);
            if (tmpc) {
                const char *gr = (const char*)glGetString(GL_RENDERER);
                const char *gv = (const char*)glGetString(GL_VENDOR);
                if (gr) strncpy(gl_renderer_cache, gr, sizeof(gl_renderer_cache)-1);
                if (gv) strncpy(gl_vendor_cache, gv, sizeof(gl_vendor_cache)-1);
                SDL_GL_DeleteContext(tmpc);
            }
            SDL_DestroyWindow(tmpw);
        }
        gl_probe_done = 1;
    }
    font = TTF_OpenFont(FONT_PATH, FONT_SIZE);
    if (!font) { printf("Font missing: %s\n", FONT_PATH); return 1; }
    #if defined(__APPLE__) && !defined(__IPHONEOS__)
    // Additional hints for macOS window decoration rendering
    SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, "0");
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    #endif
    SDL_EventState(SDL_KEYDOWN, SDL_ENABLE);
    SDL_EventState(SDL_KEYUP, SDL_ENABLE);
    Uint32 win_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

    #ifdef __IPHONEOS__
    SDL_DisplayMode dm;
    int win_w = 800, win_h = 600;
    if (SDL_GetCurrentDisplayMode(0, &dm) == 0) { win_w = dm.w; win_h = dm.h; }
    win_flags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_ALLOW_HIGHDPI;
    #else
    int win_w = 800, win_h = 600;
    #if defined(__APPLE__) && !defined(__IPHONEOS__)
    // Add HIGHDPI flag on macOS to help with window decoration rendering
    win_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
    #endif
    #endif

    SDL_Window *window = SDL_CreateWindow("C MMO Client", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h, win_flags);
    if (!window) { printf("Window creation failed: %s\n", SDL_GetError()); return 1; }
    
    #if defined(__APPLE__) && !defined(__IPHONEOS__)
    // Small delay to allow macOS to properly initialize window compositing
    SDL_Delay(100);
    // Use software renderer for compatibility with older hardware
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    #else
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    #endif
    if (!renderer) { printf("Renderer creation failed: %s\n", SDL_GetError()); return 1; }
    
    #if defined(__APPLE__) && !defined(__IPHONEOS__)
    // Fix black window decorations on macOS by forcing non-transparent titlebar
    // Transparent titlebars cause black decorations on some macOS configurations
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info) && info.info.cocoa.window) {
        void *nswin = info.info.cocoa.window;
        
        // Use objc_msgSend to set window properties in C-compatible way
        // setTitlebarAppearsTransparent:NO
        ((void (*)(void*, SEL, BOOL))objc_msgSend)(nswin, sel_getUid("setTitlebarAppearsTransparent:"), NO);
        
        // setTitleVisibility:NSWindowTitleVisible
        ((void (*)(void*, SEL, NSInteger))objc_msgSend)(nswin, sel_getUid("setTitleVisibility:"), NSWindowTitleVisible);
        
        // setAppearance:[NSAppearance appearanceNamed:@"NSAppearanceNameAqua"]
        void *appearanceClass = objc_getClass("NSAppearance");
        void *aquaAppearance = ((void* (*)(void*, SEL, void*))objc_msgSend)(appearanceClass, sel_getUid("appearanceNamed:"), CFSTR("NSAppearanceNameAqua"));
        ((void (*)(void*, SEL, void*))objc_msgSend)(nswin, sel_getUid("setAppearance:"), aquaAppearance);
        
        // setTabbingMode:NSWindowTabbingModeDisallowed
        ((void (*)(void*, SEL, NSInteger))objc_msgSend)(nswin, sel_getUid("setTabbingMode:"), NSWindowTabbingModeDisallowed);
        
        // Force window to redraw its frame by calling display (invalidates window shadow and frame)
        ((void (*)(void*, SEL))objc_msgSend)(nswin, sel_getUid("display"));
        
        // Also invalidate the window's shadow which forces full window refresh
        ((void (*)(void*, SEL))objc_msgSend)(nswin, sel_getUid("invalidateShadow"));
    }
    #endif
    
    global_renderer = renderer;

    // 2. Load Assets
    // FIX: Use current_map_file instead of MAP_FILE macro
    SDL_Surface *temp = IMG_Load(current_map_file); 
    if (temp) { 
        tex_map = SDL_CreateTextureFromSurface(renderer, temp); 
        map_w=temp->w; map_h=temp->h; 
        SDL_FreeSurface(temp); 
    }
    temp = IMG_Load(PLAYER_FILE); 
    if (temp) { 
        tex_player = SDL_CreateTextureFromSurface(renderer, temp); 
        SDL_FreeSurface(temp); 
    }
    ensure_save_file("servers.txt", "servers.txt");
    ensure_save_file("profiles.txt", "profiles.txt");
    ensure_save_file("config.txt", "config.txt");
    ensure_save_file("configs.txt", "config.txt");

    init_audio();
    load_servers();
    load_profiles();
    load_triggers();
    load_config(); // Initial load
    sock = -1;

    for(int i=0; i<MAX_CLIENTS; i++) { avatar_cache[i] = NULL; avatar_status[i] = 0; }

    int running = 1; SDL_Event event;
    int key_up=0, key_down=0, key_left=0, key_right=0;
    
    // State Tracking
    int last_active_field = -1;
    int was_chat_open = 0;
    int text_input_enabled = SDL_IsTextInputActive();
    last_input_tick = SDL_GetTicks(); 

    while (running) {
        frame_count++;
        Uint32 now_fps = SDL_GetTicks();
        if (now_fps > last_fps_check + 1000) { current_fps = frame_count; frame_count = 0; last_fps_check = now_fps; }
        
        if (client_state == STATE_GAME && music_count > 0 && !Mix_PlayingMusic()) play_next_track();

        int screen_w, screen_h; SDL_GetRendererOutputSize(renderer, &screen_w, &screen_h);
        int win_w, win_h; SDL_GetWindowSize(window, &win_w, &win_h);
        float scale_x = (float)screen_w / win_w;
        float scale_y = (float)screen_h / win_h;

        // Reset Cursor on Field Change
        if (active_field != last_active_field) {
            int len = 0;
            if(active_field==0) len=strlen(auth_username);
            else if(active_field==1) len=strlen(auth_password);
            else if(active_field==2) len=strlen(input_ip);
            else if(active_field==3) len=strlen(input_port);
            else if(active_field==10) len=strlen(nick_new);
            else if(active_field==11) len=strlen(nick_confirm);
            else if(active_field==12) len=strlen(nick_pass);
            else if(active_field==20) len=strlen(input_friend_id);
            else if(active_field==30) len=strlen(input_sanction_reason);
            else if(active_field==31) len=strlen(input_ban_time);
            
            cursor_pos = len;
            selection_len = 0;
            last_active_field = active_field;
        }

        int should_text_input = (active_field >= 0) || chat_input_active;
        if (should_text_input && !text_input_enabled) { SDL_StartTextInput(); text_input_enabled = 1; }
        else if (!should_text_input && text_input_enabled) { SDL_StopTextInput(); text_input_enabled = 0; }

        SDL_PumpEvents(); // Ensure keyboard state stays fresh even when no events are queued
        while (SDL_PollEvent(&event)) {
            // Auto-AFK Reset
            if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEMOTION) {
                last_input_tick = SDL_GetTicks();
                if (is_auto_afk && is_connected) {
                    Packet pkt; pkt.type = PACKET_STATUS_CHANGE; pkt.new_status = STATUS_ONLINE; send_packet(&pkt);
                    is_auto_afk = 0;
                }
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                key_up = key_down = key_left = key_right = 0;
            }

            if (event.type == SDL_QUIT) running = 0;
            
            // --- Scroll Handling ---
            else if (event.type == SDL_MOUSEWHEEL) {
                int scroll_amount = event.wheel.y * SCROLL_SPEED;
                
                if (is_settings_open) {
                    settings_scroll_y -= scroll_amount; 
                    if (settings_scroll_y < 0) settings_scroll_y = 0;
                    
                    int max_scroll = settings_content_h - settings_view_port.h;
                    if (max_scroll < 0) max_scroll = 0;
                    if (settings_scroll_y > max_scroll) settings_scroll_y = max_scroll;
                }
                else if (show_friend_list) {
                    friend_list_scroll -= scroll_amount;
                    if (friend_list_scroll < 0) friend_list_scroll = 0;
                    // Max scroll based on friend count (30px per friend + 85px header)
                    int content_height = 85 + (friend_count * 30);
                    int visible_height = 400 - 80; // Window height minus header
                    int max_scroll = content_height - visible_height;
                    if (max_scroll < 0) max_scroll = 0;
                    if (friend_list_scroll > max_scroll) friend_list_scroll = max_scroll;
                }
                else if (is_inbox_open) {
                    inbox_scroll -= scroll_amount;
                    if (inbox_scroll < 0) inbox_scroll = 0;
                    // Max scroll based on inbox count (55px per request + 40px header)
                    int content_height = 40 + (inbox_count * 55);
                    int visible_height = 300 - 35; // Window height minus header
                    int max_scroll = content_height - visible_height;
                    if (max_scroll < 0) max_scroll = 0;
                    if (inbox_scroll > max_scroll) inbox_scroll = max_scroll;
                }
                else if (show_contributors) {
                    contributors_scroll -= scroll_amount;
                    if (contributors_scroll < 0) contributors_scroll = 0;
                    // Contributors window: 450px content height, 400px visible area
                    // Content includes title (15px) + close btn area (30px) + ~9 text lines
                    int content_height = 450;
                    int visible_height = 400;
                    int max_scroll = content_height - visible_height;
                    if (max_scroll < 0) max_scroll = 0;
                    if (contributors_scroll > max_scroll) contributors_scroll = max_scroll;
                }
                else if (show_documentation) {
                    documentation_scroll -= scroll_amount;
                    if (documentation_scroll < 0) documentation_scroll = 0;
                    // Documentation window: 500px content height, 450px visible area
                    // Content includes title + close btn + ~12 text lines with sections
                    int content_height = 500;
                    int visible_height = 450;
                    int max_scroll = content_height - visible_height;
                    if (max_scroll < 0) max_scroll = 0;
                    if (documentation_scroll > max_scroll) documentation_scroll = max_scroll;
                }
                else if (show_my_warnings) {
                    warnings_scroll -= scroll_amount;
                    if (warnings_scroll < 0) warnings_scroll = 0;
                    // Max scroll based on actual warning count (45px per warning + 50px header)
                    int content_height = 50 + (my_warning_count * 45);
                    int visible_height = 400 - 45; // Window height minus header
                    int max_scroll = content_height - visible_height;
                    if (max_scroll < 0) max_scroll = 0;
                    if (warnings_scroll > max_scroll) warnings_scroll = max_scroll;
                }
                else if (show_blocked_list) {
                    blocked_scroll -= scroll_amount;
                    if (blocked_scroll < 0) blocked_scroll = 0;
                    // Max scroll based on actual blocked count (35px per entry + 50px header)
                    int content_height = 50 + (blocked_count * 35);
                    int visible_height = 400 - 45; // Window height minus header
                    int max_scroll = content_height - visible_height;
                    if (max_scroll < 0) max_scroll = 0;
                    if (blocked_scroll > max_scroll) blocked_scroll = max_scroll;
                }
            }
            else if (event.type == SDL_FINGERDOWN) {
                int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
                // Convert touch coordinates to UI space
                int tx = (int)((event.tfinger.x * w) / ui_scale);
                int ty = (int)((event.tfinger.y * h) / ui_scale);
                printf("[FINGERDOWN] tx=%d ty=%d w=%d h=%d fingerId=%lld state=%d ui_scale=%.2f\n", tx, ty, w, h, (long long)event.tfinger.fingerId, client_state, ui_scale);

                if (is_settings_open && SDL_PointInRect(&(SDL_Point){tx, ty}, &settings_view_port)) {
                    scroll_touch_id = event.tfinger.fingerId;
                    scroll_last_y = ty;
                    printf("[FINGERDOWN] Scroll touch started\n");
                                    } 
                // Game-specific touch handling only in STATE_GAME
                else if (client_state == STATE_GAME) {
                    // Use scaled height for UI coordinate calculations
                    int scaled_h = (int)(h / ui_scale);
                    if (is_chat_open) {
                        SDL_Rect chat_win = (SDL_Rect){10, scaled_h-240, 300, 190};
                        SDL_Rect chat_input = (SDL_Rect){15, chat_win.y + chat_win.h - 24, 270, 24};
                        if (SDL_PointInRect(&(SDL_Point){tx, ty}, &chat_input)) {
                            chat_input_active = 1;
                            SDL_StartTextInput();
                            #if defined(__ANDROID__) || defined(__IPHONEOS__)
                            // Estimate keyboard height as 40% of screen height on mobile
                            keyboard_height = h * 0.4;
                            #endif
                        } else if (!SDL_PointInRect(&(SDL_Point){tx, ty}, &chat_win)) {
                            chat_input_active = 0;
                            if (active_field < 0) SDL_StopTextInput();
                            #if defined(__ANDROID__) || defined(__IPHONEOS__)
                            keyboard_height = 0;
                            #endif
                        }
                    } else if (touch_id_dpad == -1) {
                        // Create joystick only in game state
                        dpad_rect = (SDL_Rect){tx - 75, ty - 75, 150, 150};
                        touch_id_dpad = event.tfinger.fingerId;
                        vjoy_dx = 0; vjoy_dy = 0;
                        joystick_active = 1;
                        printf("[FINGERDOWN] Joystick created at tx=%d ty=%d, fingerId=%lld\n", tx, ty, (long long)touch_id_dpad);
                    } else {
                        // Treat other touches as Mouse Clicks for UI (use scaled coordinates)
                        handle_game_click(tx, ty, 0, 0, (int)(w / ui_scale), (int)(h / ui_scale)); 
                        printf("[FINGERDOWN] Game click\n");
                    }
                }
            }
            else if (event.type == SDL_FINGERMOTION) {
                if (event.tfinger.fingerId == scroll_touch_id) {
                    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
                    int ty = (int)((event.tfinger.y * h) / ui_scale);
                    int delta = scroll_last_y - ty;
                    settings_scroll_y += delta;
                    if (settings_scroll_y < 0) settings_scroll_y = 0;
                    int max_scroll = settings_content_h - settings_view_port.h; 
                    if (max_scroll < 0) max_scroll = 0;
                    if (settings_scroll_y > max_scroll) settings_scroll_y = max_scroll;
                    scroll_last_y = ty;
                    printf("[FINGERMOTION] Scroll: delta=%d scroll_y=%d\n", delta, settings_scroll_y);
                } else if (event.tfinger.fingerId == touch_id_dpad) {
                    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
                    float cx = dpad_rect.x + dpad_rect.w/2;
                    float cy = dpad_rect.y + dpad_rect.h/2;
                    vjoy_dx = (event.tfinger.x * w - cx) / 60.0f;
                    vjoy_dy = (event.tfinger.y * h - cy) / 60.0f;
                    // Clamp
                    if(vjoy_dx > 1.0f) vjoy_dx = 1.0f; if(vjoy_dx < -1.0f) vjoy_dx = -1.0f;
                    if(vjoy_dy > 1.0f) vjoy_dy = 1.0f; if(vjoy_dy < -1.0f) vjoy_dy = -1.0f;
                    printf("[FINGERMOTION] Joystick: fingerId=%lld tx=%.2f ty=%.2f vjoy_dx=%.2f vjoy_dy=%.2f\n", 
                    (long long)event.tfinger.fingerId, event.tfinger.x * w, event.tfinger.y * h, vjoy_dx, vjoy_dy);
                } else {
                    printf("[FINGERMOTION] Unknown finger: %lld (scroll=%lld, dpad=%lld)\n", 
                           (long long)event.tfinger.fingerId, (long long)scroll_touch_id, (long long)touch_id_dpad);
                }
            }
            else if (event.type == SDL_FINGERUP) {
                if (event.tfinger.fingerId == scroll_touch_id) {
                    scroll_touch_id = -1;
                    scroll_last_y = 0;
                } else if (event.tfinger.fingerId == touch_id_dpad) {
                    touch_id_dpad = -1;
                    vjoy_dx = 0; vjoy_dy = 0;
                    joystick_active = 0;
                }
            }
            
            // --- Mouse Drag Handling ---
            else if (event.type == SDL_MOUSEBUTTONUP) {
                is_dragging = 0;
                // Apply pending UI scale change on release
                if (active_slider == SLIDER_UI_SCALE && pending_ui_scale != ui_scale) {
                    ui_scale = pending_ui_scale;
                    save_config();
                }
                // Apply pending game zoom change on release
                if (active_slider == SLIDER_GAME_ZOOM && pending_game_zoom != game_zoom) {
                    game_zoom = pending_game_zoom;
                    save_config();
                }
                active_slider = SLIDER_NONE;
            }
            else if (event.type == SDL_MOUSEMOTION) {
                // Convert mouse coordinates to UI space
                int mx = (int)((event.motion.x * scale_x) / ui_scale);
                if (is_dragging) {
                    char *target = NULL;
                    if (is_chat_open) target = input_buffer;
                    else if (active_field == 0) target = auth_username;
                    else if (active_field == 1) target = auth_password;
                    else if (active_field == 2) target = input_ip;
                    else if (active_field == 3) target = input_port;
                    else if (active_field == 10) target = nick_new;
                    else if (active_field == 11) target = nick_confirm;
                    else if (active_field == 12) target = nick_pass;
                    else if (active_field == 20) target = input_friend_id;
                    else if (active_field == 30) target = input_sanction_reason;
                    else if (active_field == 31) target = input_ban_time;

                    if (target) {
                        cursor_pos = get_cursor_pos_from_click(target, mx, active_input_rect.x);
                        selection_len = cursor_pos - selection_start;
                    }
                }
                if (active_slider != SLIDER_NONE) {
                    process_slider_drag(mx);
                }
            }

            else if (event.type == SDL_DROPFILE) {
                if (is_settings_open && sock > 0) {
                    char* dropped_file = event.drop.file; FILE *fp = fopen(dropped_file, "rb");
                    if (fp) {
                        fseek(fp, 0, SEEK_END); long fsize = ftell(fp); fseek(fp, 0, SEEK_SET);
                        if (fsize > 0 && fsize <= MAX_AVATAR_SIZE) {
                            Packet pkt; pkt.type = PACKET_AVATAR_UPLOAD; pkt.image_size = fsize;
                            send_packet(&pkt); fread(temp_avatar_buf, 1, fsize, fp);
                            send(sock, temp_avatar_buf, fsize, 0); printf("Uploaded.\n");
                        } else { printf("Too large.\n"); }
                        fclose(fp);
                    } SDL_free(dropped_file);
                }
            }

            if (client_state == STATE_AUTH) {
                if(event.type == SDL_MOUSEBUTTONDOWN) {
                    int mx = event.button.x * scale_x; int my = event.button.y * scale_y;
                    handle_auth_click(mx, my);
                }
                
                char *target = NULL; int max = 31;
                if(active_field == 0) target = auth_username;
                else if(active_field == 1) target = auth_password;
                else if(active_field == 2) { target = input_ip; max = 63; }
                else if(active_field == 3) { target = input_port; max = 5; }
                
                if (target) handle_text_edit(target, max, &event);

                if(event.type == SDL_KEYDOWN) {
                    if(event.key.keysym.sym == SDLK_RETURN) handle_auth_click(btn_login.x+1, btn_login.y+1);
                    if(event.key.keysym.sym == SDLK_TAB) { active_field++; if(active_field > 3) active_field = 0; }
                }
            } else {
                // GAME INPUTS
                if(event.type == SDL_KEYDOWN) {
                    SDL_Scancode sc = event.key.keysym.scancode;
                    if(sc == SDL_SCANCODE_W || sc == SDL_SCANCODE_UP) key_up = 1;
                    else if(sc == SDL_SCANCODE_S || sc == SDL_SCANCODE_DOWN) key_down = 1;
                    else if(sc == SDL_SCANCODE_A || sc == SDL_SCANCODE_LEFT) key_left = 1;
                    else if(sc == SDL_SCANCODE_D || sc == SDL_SCANCODE_RIGHT) key_right = 1;
                } else if(event.type == SDL_KEYUP) {
                    SDL_Scancode sc = event.key.keysym.scancode;
                    if(sc == SDL_SCANCODE_W || sc == SDL_SCANCODE_UP) key_up = 0;
                    else if(sc == SDL_SCANCODE_S || sc == SDL_SCANCODE_DOWN) key_down = 0;
                    else if(sc == SDL_SCANCODE_A || sc == SDL_SCANCODE_LEFT) key_left = 0;
                    else if(sc == SDL_SCANCODE_D || sc == SDL_SCANCODE_RIGHT) key_right = 0;
                }
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    active_field = -1; selection_len = 0; selection_start = 0;
                     // Convert mouse coordinates to UI space (accounting for window scaling and UI scaling)
                     int mx = (int)((event.button.x * scale_x) / ui_scale); 
                     int my = (int)((event.button.y * scale_y) / ui_scale);
                     
                     if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_chat_toggle)) {
                        is_chat_open = !is_chat_open; 
                        if(is_chat_open) { 
                            unread_chat_count = 0; 
                            #if defined(__ANDROID__) || defined(__IPHONEOS__)
                            // On mobile, don't auto-activate input (user taps input field to show keyboard)
                            chat_input_active = 0;
                            #else
                            // On desktop, auto-activate input for immediate typing
                            chat_input_active = 1;
                            SDL_StartTextInput();
                            #endif
                        } 
                        else { 
                            chat_input_active = 0; 
                            SDL_StopTextInput();
                            #if defined(__ANDROID__) || defined(__IPHONEOS__)
                            keyboard_height = 0;
                            #endif
                        }
                     } 
                     else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_settings_toggle)) {
                        is_settings_open = !is_settings_open;
                        if (!is_settings_open) { show_nick_popup = 0; show_blocked_list = 0; active_field = -1; }
                        else {
                            // Sync Globals
                            for(int i=0; i<MAX_CLIENTS; i++) {
                                if(local_players[i].active && local_players[i].id == local_player_id) {
                                    my_r = local_players[i].r; 
                                    my_g = local_players[i].g; 
                                    my_b = local_players[i].b;
                                    my_r2 = local_players[i].r2; 
                                    my_g2 = local_players[i].g2; 
                                    my_b2 = local_players[i].b2;
                                }
                            }
                        }
                     }
                     else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_view_friends)) {
                        show_friend_list = !show_friend_list;
                     }
                                          else if (is_chat_open) {
                         SDL_Rect chat_win = (SDL_Rect){10, screen_h-240, 300, 190};
                         SDL_Rect chat_input = (SDL_Rect){15, chat_win.y + chat_win.h - 24, 270, 24};
                         if (SDL_PointInRect(&(SDL_Point){mx, my}, &chat_input)) {
                             chat_input_active = 1;
                             SDL_StartTextInput();
                             #if defined(__ANDROID__) || defined(__IPHONEOS__)
                             // Estimate keyboard height as 40% of screen height on mobile
                             keyboard_height = screen_h * 0.4;
                             #endif
                         } else if (!SDL_PointInRect(&(SDL_Point){mx, my}, &chat_win)) {
                             chat_input_active = 0;
                             if (active_field < 0) SDL_StopTextInput();
                             #if defined(__ANDROID__) || defined(__IPHONEOS__)
                             keyboard_height = 0;
                             #endif
                         }
                     }
                     else {
                         float px=0, py=0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].active && local_players[i].id == local_player_id) { px=local_players[i].x; py=local_players[i].y; }
                         int cam_x = (int)px - (screen_w/2) + 16; int cam_y = (int)py - (screen_h/2) + 16;
                        if (screen_w > map_w) cam_x = -(screen_w - map_w)/2; if (screen_h > map_h) cam_y = -(screen_h - map_h)/2;
                        // Pass scaled dimensions to match scaled mouse coordinates
                        handle_game_click(mx, my, cam_x, cam_y, (int)(screen_w / ui_scale), (int)(screen_h / ui_scale));
                     }
                }
                
                if (is_chat_open) {
                    handle_text_edit(input_buffer, 60, &event);
                    if(event.type == SDL_KEYDOWN) {
                        if(event.key.keysym.sym == SDLK_RETURN) {
                            if(strlen(input_buffer) > 0) { 
                                // FIX: Removed the outer 'if (chat_target_id != -1)' wrapper
                                Packet pkt;
                                memset(&pkt, 0, sizeof(Packet)); 
                                
                                if (chat_target_id != -1) {
                                    pkt.type = PACKET_PRIVATE_MESSAGE; 
                                    pkt.target_id = chat_target_id; 
                                } else { 
                                    pkt.type = PACKET_CHAT; 
                                }
                                strcpy(pkt.msg, input_buffer); 
                                send_packet(&pkt);
                                
                                input_buffer[0] = 0; 
                                cursor_pos = 0;
                            }
                            is_chat_open = 0;
                            chat_target_id = -1;  // Reset private message mode after sending
                            chat_input_active = 0;
                            SDL_StopTextInput();
                            #if defined(__ANDROID__) || defined(__IPHONEOS__)
                            keyboard_height = 0;
                            #endif
                        }
                        else if(event.key.keysym.sym == SDLK_ESCAPE) { 
                            is_chat_open = 0; 
                            chat_target_id = -1;
                            chat_input_active = 0;
                            SDL_StopTextInput();
                            #if defined(__ANDROID__) || defined(__IPHONEOS__)
                            keyboard_height = 0;
                            #endif
                        }
                    }
                }
                else if (is_settings_open && show_nick_popup) {
                    char *target = NULL;
                    if(active_field == 10) target = nick_new;
                    else if(active_field == 11) target = nick_confirm;
                    else if(active_field == 12) target = nick_pass;
                    if (target) handle_text_edit(target, 31, &event);
                }
                else if (show_add_friend_popup) {
                    if (active_field == 20) handle_text_edit(input_friend_id, 8, &event);
                }
                else if (show_sanction_popup) {
                    char *target = NULL; int max = 63;
                    if (active_field == 30) { target = input_sanction_reason; max = 63; }
                    if (active_field == 31) { target = input_ban_time; max = 15; }
                    if (target) handle_text_edit(target, max, &event);
                }
                else {
                    // Chat Trigger
                    if (event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_t)) { 
                        is_chat_open=1; input_buffer[0]=0; cursor_pos=0; unread_chat_count=0; SDL_StartTextInput(); 
                    }
                }
            }
        } // End PollEvent

        if (is_chat_open != was_chat_open) {
            if(is_chat_open) cursor_pos = strlen(input_buffer);
            was_chat_open = is_chat_open;
        }

        Uint32 now = SDL_GetTicks();
        if (is_connected && client_state == STATE_GAME && !is_auto_afk) {
            int my_status = 0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == local_player_id) my_status = local_players[i].status;
            if (my_status == STATUS_ONLINE) { 
                if (now > last_input_tick + (afk_timeout_minutes * 60 * 1000)) {
                    Packet pkt; pkt.type = PACKET_STATUS_CHANGE; pkt.new_status = STATUS_AFK; send_packet(&pkt);
                    is_auto_afk = 1; printf("Auto-AFK Triggered.\n");
                }
            }
        }

            if (is_connected && client_state == STATE_GAME) {
            if (now - last_ping_sent > 1000) { Packet pkt; pkt.type = PACKET_PING; pkt.timestamp = now; send_packet(&pkt); last_ping_sent = now; }
            if (!is_chat_open && !show_nick_popup && !show_add_friend_popup && pending_friend_req_id == -1) {
                
                // --- FIXED MOVEMENT LOGIC START ---
                float dx = 0, dy = 0;
                const Uint8 *state = SDL_GetKeyboardState(NULL);
                if (key_up || state[SDL_SCANCODE_W] || state[SDL_SCANCODE_UP]) dy = -1;
                if (key_down || state[SDL_SCANCODE_S] || state[SDL_SCANCODE_DOWN]) dy = 1;
                if (key_left || state[SDL_SCANCODE_A] || state[SDL_SCANCODE_LEFT]) dx = -1;
                if (key_right || state[SDL_SCANCODE_D] || state[SDL_SCANCODE_RIGHT]) dx = 1;
                if (vjoy_dx > 0.2f) dx = 1; else if (vjoy_dx < -0.2f) dx = -1;
                if (vjoy_dy > 0.2f) dy = 1; else if (vjoy_dy < -0.2f) dy = -1;
                
                float my_x=0, my_y=0; 
                for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == local_player_id) { my_x=local_players[i].x; my_y=local_players[i].y; }
                SDL_Rect player_box = {(int)my_x, (int)my_y, PLAYER_WIDTH, PLAYER_HEIGHT};
                for(int t=0; t<trigger_count; t++) {
                   if (strcmp(triggers[t].src_map, current_map_file) == 0) {
                       if (SDL_HasIntersection(&player_box, &triggers[t].rect)) {
                           switch_map(triggers[t].target_map, triggers[t].spawn_x, triggers[t].spawn_y);
                           break; 
                       }
                   }
                }

                if (dx != 0 || dy != 0) { 
                    if (now - last_move_time > 30) {
                        Packet pkt; 
                        memset(&pkt, 0, sizeof(Packet));
                        pkt.type = PACKET_MOVE; 
                        pkt.dx = dx; pkt.dy = dy; 
                        send_packet(&pkt);
                        last_move_time = now;
                        
                        // Update local position immediately for trigger detection
                        float length = sqrt(dx * dx + dy * dy);
                        if (length > 0) {
                            float norm_dx = dx / length;
                            float norm_dy = dy / length;
                            for(int i=0; i<MAX_CLIENTS; i++) {
                                if(local_players[i].id == local_player_id) {
                                    local_players[i].x += norm_dx * PLAYER_SPEED;
                                    local_players[i].y += norm_dy * PLAYER_SPEED;
                                    if (local_players[i].x < 0) local_players[i].x = 0;
                                    if (local_players[i].x > MAP_WIDTH - 32) local_players[i].x = MAP_WIDTH - 32;
                                    if (local_players[i].y < 0) local_players[i].y = 0;
                                    if (local_players[i].y > MAP_HEIGHT - 32) local_players[i].y = MAP_HEIGHT - 32;
                                }
                            }
                        }
                    }
                }
                // --- FIXED MOVEMENT LOGIC END ---
             }
         }
           
        if (is_connected && sock != 0) {
            // FIX: Use select() instead of ioctl/ioctlsocket
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(sock, &readfds);
            
            // Timeout 0 = Non-blocking check
            struct timeval tv = {0, 0}; 

            // Check if socket is readable
            int activity = select(sock + 1, &readfds, NULL, NULL, &tv);

            if (activity > 0 && FD_ISSET(sock, &readfds)) {
                Packet pkt; 
                if (recv_total(sock, &pkt, sizeof(Packet)) > 0) {
                    if (pkt.type == PACKET_AUTH_RESPONSE) {
                        if (pkt.status == AUTH_SUCCESS) { 
                            client_state = STATE_GAME; 
                            local_player_id = pkt.player_id; 
                            SDL_StopTextInput(); 
                            if (music_count > 0) play_next_track();
                            
                            load_config(); 
                            
                            Packet cpkt; cpkt.type = PACKET_COLOR_CHANGE; 
                            cpkt.r = saved_r; cpkt.g = saved_g; cpkt.b = saved_b; 
                            cpkt.r2 = my_r2; cpkt.g2 = my_g2; cpkt.b2 = my_b2;
                            send_packet(&cpkt);
                        }
                        else if (pkt.status == AUTH_REGISTER_SUCCESS) strcpy(auth_message, "Success! Login now.");
                        else strcpy(auth_message, "Error.");
                    }
                    if (pkt.type == PACKET_UPDATE) {
                        memcpy(local_players, pkt.players, sizeof(local_players));
                        for(int i=0; i<MAX_CLIENTS; i++) {
                            if (local_players[i].id == local_player_id) {
                                if (strcmp(local_players[i].map_name, current_map_file) != 0) {
                                    switch_map(local_players[i].map_name, local_players[i].x, local_players[i].y);
                                }
                            }
                        }
                     }
                    if (pkt.type == PACKET_CHAT || pkt.type == PACKET_PRIVATE_MESSAGE) {
                        add_chat_message(&pkt);
                        if (!is_chat_open) unread_chat_count++;
                        if (pkt.player_id == -1 && show_add_friend_popup) strncpy(friend_popup_msg, pkt.msg, 127);
                    }
                    if (pkt.type == PACKET_FRIEND_LIST) { friend_count = pkt.friend_count; memcpy(my_friends, pkt.friends, sizeof(pkt.friends)); }                
                    if (pkt.type == PACKET_FRIEND_INCOMING) { 
                        if (inbox_count < 10) {
                            int has = 0; for(int i=0; i<inbox_count; i++) if(inbox[i].id == pkt.player_id) has=1;
                            if(!has) { inbox[inbox_count].id = pkt.player_id; strncpy(inbox[inbox_count].name, pkt.username, 31); inbox_count++; }
                        }
                    }
                    if (pkt.type == PACKET_PING) current_ping = SDL_GetTicks() - pkt.timestamp;
                    if (pkt.type == PACKET_CHANGE_NICK_RESPONSE) {
                        if (pkt.status == AUTH_SUCCESS) { show_nick_popup = 0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == local_player_id) strncpy(local_players[i].username, pkt.username, 31); } 
                        else strncpy(auth_message, pkt.msg, 127);
                    }
                    if (pkt.type == PACKET_ROLE_LIST_RESPONSE) {
                        staff_count = pkt.role_count;
                        for(int i=0; i<staff_count; i++) {
                            staff_list[i].id = pkt.roles[i].id;
                            strncpy(staff_list[i].name, pkt.roles[i].username, 31);
                            staff_list[i].role = pkt.roles[i].role;
                        }
                    }
                    if (pkt.type == PACKET_KICK) {
                        printf("Kicked: %s\n", pkt.msg);
                        strcpy(auth_message, pkt.msg);
                        close(sock); sock = -1; is_connected = 0;
                        client_state = STATE_AUTH;
                    }
                    if (pkt.type == PACKET_WARNINGS_RESPONSE) {
                        my_warning_count = pkt.warning_count;
                        for(int i=0; i<my_warning_count; i++) {
                            strcpy(my_warning_list[i].reason, pkt.warnings[i].reason);
                            strcpy(my_warning_list[i].date, pkt.warnings[i].date);
                        }
                    }
                    if (pkt.type == PACKET_AVATAR_RESPONSE) {
                        if (pkt.image_size > 0 && pkt.image_size <= MAX_AVATAR_SIZE) {
                            int total = 0;
                            while(total < pkt.image_size) { int n = recv(sock, temp_avatar_buf + total, pkt.image_size - total, 0); if(n > 0) total += n; }
                            int target_idx = -1;
                            for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].active && local_players[i].id == pkt.player_id) target_idx = i;
                            if (target_idx != -1) {
                                SDL_RWops *rw = SDL_RWFromMem(temp_avatar_buf, pkt.image_size); SDL_Surface *surf = IMG_Load_RW(rw, 1); 
                                if (surf) { if (avatar_cache[target_idx]) SDL_DestroyTexture(avatar_cache[target_idx]); avatar_cache[target_idx] = SDL_CreateTextureFromSurface(renderer, surf); avatar_status[target_idx] = 2; SDL_FreeSurface(surf); }
                            }
                        }
                    }
                    if (pkt.type == PACKET_TRIGGERS_DATA) {
                        receive_triggers_from_server(&pkt);
                    }
                }    
            }
        }

        if (client_state == STATE_AUTH) render_auth_screen(renderer); else render_game(renderer);
        SDL_Delay(16);
    }
    
    if(tex_map) SDL_DestroyTexture(tex_map); if(tex_player) SDL_DestroyTexture(tex_player);
    if(cached_contributors_tex) SDL_DestroyTexture(cached_contributors_tex);
    if(cached_documentation_tex) SDL_DestroyTexture(cached_documentation_tex);
    if(bgm) Mix_FreeMusic(bgm); Mix_CloseAudio();
    if(sock > 0) close(sock); 
    TTF_CloseFont(font); TTF_Quit(); IMG_Quit();
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}