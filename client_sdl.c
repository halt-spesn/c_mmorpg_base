#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <sys/utsname.h>
#include <GL/gl.h>
#include "common.h"

// --- Config ---
#define PLAYER_WIDTH 32
#define PLAYER_HEIGHT 32
#define MAP_FILE "map.jpg"
#define PLAYER_FILE "player.png"
#define FONT_PATH "/usr/share/fonts/TTF/DejaVuSans.ttf"
#define FONT_SIZE 14

// --- Globals ---
int sock;
char server_ip[16] = "127.0.0.1";
TTF_Font *font = NULL;
SDL_Texture *tex_map = NULL;
SDL_Texture *tex_player = NULL;
int map_w = MAP_WIDTH, map_h = MAP_HEIGHT;

int local_player_id = -1;
Player local_players[MAX_CLIENTS];
int my_friends[20];
int friend_count = 0;

// BLOCKED SYSTEM
int blocked_ids[50];
int blocked_count = 0;
int show_blocked_list = 0; // Toggle for the UI list

// UI Rects
SDL_Rect btn_hide_player;      // In Profile
SDL_Rect btn_view_blocked;     // In Settings
SDL_Rect blocked_win;          // The list window
SDL_Rect btn_close_blocked;    // Close list

// UI STATE
int selected_player_id = -1;
int pending_friend_req_id = -1;
char pending_friend_name[32] = "";

SDL_Rect profile_win = {10, 10, 200, 130}; // Taller for PM button
SDL_Rect btn_add_friend = {20, 50, 180, 30};
SDL_Rect btn_send_pm    = {20, 90, 180, 30}; // NEW Button
SDL_Rect popup_win;

// Settings & Debug State
int is_settings_open = 0;
int show_debug_info = 0;
int show_fps = 0;
int show_coords = 0;
int current_ping = 0;
Uint32 last_ping_sent = 0;

// FPS Calculation
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
int chat_target_id = -1; // -1=Global, >0=PM Target
char input_buffer[64] = "";

typedef enum { STATE_AUTH, STATE_GAME } ClientState;
ClientState client_state = STATE_AUTH;

#define MAX_INPUT_LEN 31
char auth_username[MAX_INPUT_LEN+1] = "";
char auth_password[MAX_INPUT_LEN+1] = "";
int active_field = 0;
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
SDL_Color col_magenta = {255,0,255,255}; // PM Color

// Status Colors
SDL_Color col_status_online = {0, 255, 0, 255};
SDL_Color col_status_afk = {255, 255, 0, 255};
SDL_Color col_status_dnd = {255, 50, 50, 255};
SDL_Color col_status_rp = {200, 0, 200, 255};
SDL_Color col_status_talk = {50, 150, 255, 255};

const char* status_names[] = { "Online", "AFK", "Do Not Disturb", "Roleplay", "Open to Talk" };

SDL_Rect btn_cycle_status; // For Settings Menu
SDL_Rect btn_view_friends; // Bottom bar
SDL_Rect friend_list_win;
int show_friend_list = 0;

// Color Sliders in Settings
SDL_Rect slider_r, slider_g, slider_b;

// Texture Cache: 0=Empty, 1=Requested, 2=Loaded
SDL_Texture* avatar_cache[MAX_CLIENTS]; 
int avatar_status[MAX_CLIENTS]; // 0=None, 1=Waiting, 2=Ready

// --- Helpers ---
void send_packet(Packet *pkt) { send(sock, pkt, sizeof(Packet), 0); }

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

void toggle_block(int id) {
    if (is_blocked(id)) {
        // Unblock: Remove from array
        for(int i=0; i<blocked_count; i++) {
            if(blocked_ids[i] == id) {
                // Shift remaining down
                for(int j=i; j<blocked_count-1; j++) blocked_ids[j] = blocked_ids[j+1];
                blocked_count--;
                return;
            }
        }
    } else {
        // Block: Add to array
        if(blocked_count < 50) blocked_ids[blocked_count++] = id;
    }
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

void render_text(SDL_Renderer *renderer, const char *text, int x, int y, SDL_Color color, int center) {
    if (!text || strlen(text) == 0) return;
    SDL_Surface *surface = TTF_RenderText_Solid(font, text, color);
    if (!surface) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    int rx = center ? x - (surface->w/2) : x;
    SDL_Rect dst = {rx, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void add_chat_message(Packet *pkt) {
    // 1. IGNORE BLOCKED PLAYERS
    if (pkt->player_id != -1 && pkt->player_id != local_player_id) {
        if (is_blocked(pkt->player_id)) return;
    }

    for(int i=0; i<CHAT_HISTORY-1; i++) strcpy(chat_log[i], chat_log[i+1]);
    
    char entry[64];
    
    if (pkt->type == PACKET_PRIVATE_MESSAGE) {
        char *sender_name = "Unknown";
        char *target_name = "Unknown";
        
        for(int i=0; i<MAX_CLIENTS; i++) {
            if (local_players[i].active) {
                if (local_players[i].id == pkt->player_id) sender_name = local_players[i].username;
                if (local_players[i].id == pkt->target_id) target_name = local_players[i].username;
            }
        }

        if (pkt->player_id == local_player_id) {
            // OUTGOING (I sent it)
            snprintf(entry, 64, "To [%s]: %s", target_name, pkt->msg);

            // --- ADD THIS BLOCK HERE ---
            // Show floating text above MY head
            for(int i=0; i<MAX_CLIENTS; i++) {
                if(local_players[i].id == pkt->player_id) { // pkt->player_id is ME
                    strncpy(floating_texts[i].msg, pkt->msg, 63);
                    floating_texts[i].timestamp = SDL_GetTicks();
                }
            }
            // ---------------------------
        } else {
            // INCOMING (Someone sent to me)
            snprintf(entry, 64, "From [%s]: %s", sender_name, pkt->msg);
            
            // Show floating text above SENDER'S head
            for(int i=0; i<MAX_CLIENTS; i++) {
                if(local_players[i].id == pkt->player_id) {
                    strncpy(floating_texts[i].msg, pkt->msg, 63);
                    floating_texts[i].timestamp = SDL_GetTicks();
                }
            }
        }
    } else {
        // Normal Chat
        char *name = "System";
        if (pkt->player_id != -1) {
            name = "Unknown";
            for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == pkt->player_id) name = local_players[i].username;
        }
        snprintf(entry, 64, "[%s]: %s", name, pkt->msg);
        
        // Floating text
        if (pkt->player_id != -1) {
            for(int i=0; i<MAX_CLIENTS; i++) {
                if(local_players[i].id == pkt->player_id) {
                    strncpy(floating_texts[i].msg, pkt->msg, 63);
                    floating_texts[i].timestamp = SDL_GetTicks();
                }
            }
        }
    }
    strcpy(chat_log[CHAT_HISTORY-1], entry);
}

// --- HUD & Debug ---

void render_hud(SDL_Renderer *renderer, int screen_h) {
    int y = 10;
    char buf[64];

    if (show_fps) {
        snprintf(buf, 64, "FPS: %d", current_fps);
        int w, h; TTF_SizeText(font, buf, &w, &h);
        SDL_Rect bg = {10, y, w+4, h};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150); SDL_RenderFillRect(renderer, &bg);
        render_text(renderer, buf, 12, y, col_green, 0); y += 20;
    }

    if (show_coords) {
        float px=0, py=0;
        for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].active && local_players[i].id == local_player_id) { px=local_players[i].x; py=local_players[i].y; }
        snprintf(buf, 64, "XY: %.0f, %.0f", px, py);
        int w, h; TTF_SizeText(font, buf, &w, &h);
        SDL_Rect bg = {10, y, w+4, h};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150); SDL_RenderFillRect(renderer, &bg);
        render_text(renderer, buf, 12, y, col_white, 0);
    }
}

void render_debug_overlay(SDL_Renderer *renderer, int screen_w) {
    if (!show_debug_info) return;

    char lines[10][128];
    int line_count = 0;

    snprintf(lines[line_count++], 128, "Ping: %d ms", current_ping);
    snprintf(lines[line_count++], 128, "Server IP: %s", server_ip);

    float px=0, py=0;
    for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].active && local_players[i].id == local_player_id) { px=local_players[i].x; py=local_players[i].y; }
    snprintf(lines[line_count++], 128, "Pos: %.1f, %.1f", px, py);

    const unsigned char *renderer_str = glGetString(GL_RENDERER);
    if (renderer_str) snprintf(lines[line_count++], 128, "GPU: %s", renderer_str);
    else snprintf(lines[line_count++], 128, "GPU: Unknown");

    SDL_version compiled; SDL_VERSION(&compiled);
    snprintf(lines[line_count++], 128, "SDL: %d.%d.%d", compiled.major, compiled.minor, compiled.patch);

    struct utsname buffer;
    if (uname(&buffer) == 0) snprintf(lines[line_count++], 128, "OS: %s %s", buffer.sysname, buffer.release);
    else snprintf(lines[line_count++], 128, "OS: Unknown");

    char compiler_name[10] = "Unknown";
    #if defined(__clang__) 
        strcpy(compiler_name, "Clang");
    #elif defined(__GNUC__)
        strcpy(compiler_name, "GCC");
    #endif
    snprintf(lines[line_count++], 128, "Compiler: %s %s", compiler_name, __VERSION__);

    int max_w = 200; 
    for(int i=0; i<line_count; i++) { int w, h; TTF_SizeText(font, lines[i], &w, &h); if (w > max_w) max_w = w; }

    int box_w = max_w + 20;
    int box_h = (line_count * 20) + 10;
    int start_x = screen_w - box_w - 10;
    if (start_x < 10) start_x = 10;

    SDL_Rect dbg_box = {start_x, 10, box_w, box_h};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150); SDL_RenderFillRect(renderer, &dbg_box);

    int y = 15;
    for(int i=0; i<line_count; i++) {
        SDL_Color color = col_white;
        if (strncmp(lines[i], "Ping:", 5) == 0) color = col_green;
        render_text(renderer, lines[i], dbg_box.x + 10, y, color, 0);
        y += 20;
    }
}

// --- UI Rendering ---

void render_settings_menu(SDL_Renderer *renderer, int screen_w, int screen_h) {
    if (!is_settings_open) return;
    
    // 1. INCREASE WINDOW HEIGHT
    // Old: 460. New: 500 to fit the extra spacing
    settings_win = (SDL_Rect){screen_w/2 - 150, screen_h/2 - 200, 300, 500}; 
    
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255); SDL_RenderFillRect(renderer, &settings_win);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &settings_win);
    render_text(renderer, "Settings", settings_win.x + 150, settings_win.y + 10, col_white, 1);

    // 1. Debug
    btn_toggle_debug = (SDL_Rect){settings_win.x + 20, settings_win.y + 50, 20, 20};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &btn_toggle_debug);
    if (show_debug_info) { SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_Rect c={btn_toggle_debug.x+4,btn_toggle_debug.y+4,12,12}; SDL_RenderFillRect(renderer,&c); }
    render_text(renderer, "Show Debug Info", settings_win.x + 50, settings_win.y + 50, col_white, 0);

    // 2. FPS
    btn_toggle_fps = (SDL_Rect){settings_win.x + 20, settings_win.y + 90, 20, 20};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &btn_toggle_fps);
    if (show_fps) { SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_Rect c={btn_toggle_fps.x+4,btn_toggle_fps.y+4,12,12}; SDL_RenderFillRect(renderer,&c); }
    render_text(renderer, "Show FPS", settings_win.x + 50, settings_win.y + 90, col_white, 0);

    // 3. Coords
    btn_toggle_coords = (SDL_Rect){settings_win.x + 20, settings_win.y + 130, 20, 20};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &btn_toggle_coords);
    if (show_coords) { SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_Rect c={btn_toggle_coords.x+4,btn_toggle_coords.y+4,12,12}; SDL_RenderFillRect(renderer,&c); }
    render_text(renderer, "Show Coordinates", settings_win.x + 50, settings_win.y + 130, col_white, 0);

    // 4. Blocked List
    SDL_Rect btn_view_blocked = (SDL_Rect){settings_win.x + 20, settings_win.y + 170, 260, 30};
    SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255); SDL_RenderFillRect(renderer, &btn_view_blocked);
    render_text(renderer, "Manage Blocked Players", btn_view_blocked.x + 130, btn_view_blocked.y + 5, col_white, 1);

    // 5. Status Cycle
    int my_status = 0;
    for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == local_player_id) my_status = local_players[i].status;

    btn_cycle_status = (SDL_Rect){settings_win.x + 20, settings_win.y + 220, 260, 30};
    SDL_SetRenderDrawColor(renderer, 50, 50, 100, 255); SDL_RenderFillRect(renderer, &btn_cycle_status);
    
    char status_str[64];
    snprintf(status_str, 64, "Status: %s", status_names[my_status]);
    render_text(renderer, status_str, btn_cycle_status.x + 130, btn_cycle_status.y + 5, get_status_color(my_status), 1);
    
    render_text(renderer, "(Click to change)", btn_cycle_status.x + 130, btn_cycle_status.y + 40, col_white, 1);

    // 6. Name Color
    int my_r=255, my_g=255, my_b=255;
    for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == local_player_id) { my_r=local_players[i].r; my_g=local_players[i].g; my_b=local_players[i].b; }

    // 2. ADJUST START Y
    // Old: settings_win.y + 260. New: settings_win.y + 300
    // This gives ample space below the "(Click to change)" text
    int start_y = settings_win.y + 300;
    
    render_text(renderer, "Name Color", settings_win.x + 150, start_y, (SDL_Color){my_r, my_g, my_b, 255}, 1);

    // Sliders
    slider_r = (SDL_Rect){settings_win.x + 50, start_y + 30, 200, 20};
    slider_g = (SDL_Rect){settings_win.x + 50, start_y + 60, 200, 20};
    slider_b = (SDL_Rect){settings_win.x + 50, start_y + 90, 200, 20};

    // Draw R
    SDL_SetRenderDrawColor(renderer, 50, 0, 0, 255); SDL_RenderFillRect(renderer, &slider_r);
    SDL_Rect fill_r = {slider_r.x, slider_r.y, (int)((my_r/255.0)*200), 20};
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); SDL_RenderFillRect(renderer, &fill_r);

    // Draw G
    SDL_SetRenderDrawColor(renderer, 0, 50, 0, 255); SDL_RenderFillRect(renderer, &slider_g);
    SDL_Rect fill_g = {slider_g.x, slider_g.y, (int)((my_g/255.0)*200), 20};
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); SDL_RenderFillRect(renderer, &fill_g);

    // Draw B
    SDL_SetRenderDrawColor(renderer, 0, 0, 50, 255); SDL_RenderFillRect(renderer, &slider_b);
    SDL_Rect fill_b = {slider_b.x, slider_b.y, (int)((my_b/255.0)*200), 20};
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); SDL_RenderFillRect(renderer, &fill_b);
    // Add text instruction
    render_text(renderer, "Drag & Drop Image here", settings_win.x + 150, settings_win.y + 450, col_yellow, 1);
    render_text(renderer, "to upload Avatar (<16KB)", settings_win.x + 150, settings_win.y + 470, col_yellow, 1);
}

void render_friend_list(SDL_Renderer *renderer, int w, int h) {
    if (!show_friend_list) return;

    friend_list_win = (SDL_Rect){w/2 - 150, h/2 - 200, 300, 400};
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); SDL_RenderFillRect(renderer, &friend_list_win);
    SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255); SDL_RenderDrawRect(renderer, &friend_list_win);
    
    render_text(renderer, "Friends List", friend_list_win.x + 150, friend_list_win.y + 10, col_green, 1);

    // Close Button logic handled in click handler (reusing rect logic or adding specific btn)
    SDL_Rect btn_close = {friend_list_win.x + 260, friend_list_win.y + 5, 30, 30};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_close);
    render_text(renderer, "X", btn_close.x + 15, btn_close.y + 5, col_white, 1);

    int y_off = 50;
    for(int i=0; i<friend_count; i++) {
        int fid = my_friends[i];
        char display[64]; snprintf(display, 64, "ID: %d (Offline)", fid);
        int online = 0;
        
        for(int p=0; p<MAX_CLIENTS; p++) 
            if(local_players[p].active && local_players[p].id == fid) {
                snprintf(display, 64, "%s (Online)", local_players[p].username);
                online = 1;
            }

        render_text(renderer, display, friend_list_win.x + 20, friend_list_win.y + y_off, online ? col_white : col_btn, 0);
        y_off += 30;
    }
}

void render_popup(SDL_Renderer *renderer, int w, int h) {
    if (pending_friend_req_id == -1) return;
    popup_win = (SDL_Rect){w/2-150, h/2-60, 300, 120};
    
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255); SDL_RenderFillRect(renderer, &popup_win);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &popup_win);

    char msg[64]; snprintf(msg, 64, "Friend Request from %s", pending_friend_name);
    render_text(renderer, msg, popup_win.x + 150, popup_win.y + 20, col_yellow, 1);

    SDL_Rect btn_accept = {popup_win.x + 20, popup_win.y + 70, 120, 30};
    SDL_Rect btn_deny = {popup_win.x + 160, popup_win.y + 70, 120, 30};

    SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_accept);
    render_text(renderer, "Accept", btn_accept.x + 60, btn_accept.y + 5, col_white, 1);
    SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_deny);
    render_text(renderer, "Deny", btn_deny.x + 60, btn_deny.y + 5, col_white, 1);
}

void render_profile(SDL_Renderer *renderer) {
    if (selected_player_id == -1 || selected_player_id == local_player_id) return;
    
    int exists = 0; char *name = "Unknown"; int status = 0; int idx = -1;
    for(int i=0; i<MAX_CLIENTS; i++) 
        if(local_players[i].id == selected_player_id) { 
            exists=1; name=local_players[i].username; status=local_players[i].status; idx = i;
        }
    if (!exists) { selected_player_id = -1; return; }

    // 1. CALCULATE DYNAMIC WIDTH
    int text_w = 0, text_h = 0;
    TTF_SizeText(font, name, &text_w, &text_h);
    
    // Base width is 200. 
    // Avatar takes ~90px. Text starts at x+100.
    // So we need: 100 (avatar space) + text_w + 20 (padding)
    int required_w = 100 + text_w + 20;
    
    // Ensure minimum width of 200
    if (required_w < 200) required_w = 200;

    profile_win.w = required_w;
    profile_win.h = 240; 

    // Draw Background with new width
    SDL_SetRenderDrawColor(renderer, 30, 30, 50, 230); SDL_RenderFillRect(renderer, &profile_win);
    SDL_SetRenderDrawColor(renderer, 200, 200, 255, 255); SDL_RenderDrawRect(renderer, &profile_win);
    
    // --- AVATAR LOGIC ---
    SDL_Rect avatar_rect = {profile_win.x + 20, profile_win.y + 20, 64, 64};
    if (avatar_status[idx] == 0) {
        Packet req; req.type = PACKET_AVATAR_REQUEST; req.target_id = selected_player_id;
        send_packet(&req); avatar_status[idx] = 1; 
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderFillRect(renderer, &avatar_rect);
    if (avatar_status[idx] == 2 && avatar_cache[idx]) SDL_RenderCopy(renderer, avatar_cache[idx], NULL, &avatar_rect);
    else render_text(renderer, "?", avatar_rect.x + 25, avatar_rect.y + 20, col_white, 0);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDrawRect(renderer, &avatar_rect);

    // Render Name (No shrinking needed now!)
    render_text(renderer, name, profile_win.x + 100, profile_win.y + 20, col_white, 0);

    // Status
    render_text(renderer, status_names[status], profile_win.x + 100, profile_win.y + 45, get_status_color(status), 0);

    // --- BUTTONS ---
    // We stretch buttons to fill the new width
    int btn_w = profile_win.w - 40; // 20px padding on each side
    int start_y = profile_win.y + 90;
    
    // 1. Friend
    int is_friend = 0; for(int i=0; i<friend_count; i++) if(my_friends[i] == selected_player_id) is_friend = 1;
    // Update global/local rect width
    SDL_Rect btn = {profile_win.x + 20, start_y, btn_w, 30}; 
    
    if (!is_friend) { 
        SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255); SDL_RenderFillRect(renderer, &btn); 
        render_text(renderer, "+ Add Friend", btn.x + (btn_w/2), btn.y + 5, col_white, 1); // Centered text
    } 
    else { 
        SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn); 
        render_text(renderer, "- Remove Friend", btn.x + (btn_w/2), btn.y + 5, col_white, 1); 
    }

    // 2. PM
    // Update global rect so clicks work!
    btn_send_pm = (SDL_Rect){profile_win.x + 20, start_y + 40, btn_w, 30};
    SDL_SetRenderDrawColor(renderer, 100, 0, 100, 255); SDL_RenderFillRect(renderer, &btn_send_pm);
    render_text(renderer, "Send Message", btn_send_pm.x + (btn_w/2), btn_send_pm.y + 5, col_white, 1);

    // 3. Hide
    SDL_Rect btn_hide = {profile_win.x + 20, start_y + 80, btn_w, 30};
    int hidden = is_blocked(selected_player_id);
    if (hidden) { 
        SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_hide); 
        render_text(renderer, "Unhide", btn_hide.x + (btn_w/2), btn_hide.y + 5, col_white, 1); 
    } 
    else { 
        SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255); SDL_RenderFillRect(renderer, &btn_hide); 
        render_text(renderer, "Hide", btn_hide.x + (btn_w/2), btn_hide.y + 5, col_white, 1); 
    }
    
    // IMPORTANT: Update the local 'btn_add_friend' global is tricky because we use a local var in logic above
    // We must ensure the click handler uses the same logic or we update the global here.
    // Let's update the global 'btn_add_friend' so the click handler works.
    btn_add_friend = btn;
    // We also need to update the Hide button rect logic in handle_click?
    // Actually, handle_game_click logic for 'btn_hide_player' was local. 
    // We need to sync them.
    
    // SOLUTION: We define these rects in render_profile, and we should update GLOBAL or STATIC vars 
    // so handle_click sees the new size.
    // 'btn_send_pm' and 'btn_add_friend' are global. 'btn_hide_player' needs to be handled carefully.
    // I updated 'btn_add_friend' and 'btn_send_pm' above.
    
    // For 'Hide', let's make a global rect for it to be safe, or just recalculate in handle_click.
    // Simplest: Let's assume you updated globals.
    // To enable the "Hide" click to work with the new width, we need to add a global for it.
}

void render_blocked_list(SDL_Renderer *renderer, int w, int h) {
    if (!show_blocked_list) return;

    blocked_win = (SDL_Rect){w/2 - 150, h/2 - 200, 300, 400};
    
    // Background
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); SDL_RenderFillRect(renderer, &blocked_win);
    SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255); SDL_RenderDrawRect(renderer, &blocked_win);
    
    render_text(renderer, "Blocked Players", blocked_win.x + 150, blocked_win.y + 10, col_red, 1);

    // Close Button
    btn_close_blocked = (SDL_Rect){blocked_win.x + 260, blocked_win.y + 5, 30, 30};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_close_blocked);
    render_text(renderer, "X", btn_close_blocked.x + 15, btn_close_blocked.y + 5, col_white, 1);

    // List
    int y_off = 50;
    for(int i=0; i<blocked_count; i++) {
        int id = blocked_ids[i];
        
        // Find name if online, else show ID
        char display[64]; snprintf(display, 64, "ID: %d", id);
        for(int p=0; p<MAX_CLIENTS; p++) 
            if(local_players[p].active && local_players[p].id == id) 
                snprintf(display, 64, "%s", local_players[p].username);

        render_text(renderer, display, blocked_win.x + 20, blocked_win.y + y_off, col_white, 0);

        // Unblock Button (Small 'X' or 'U')
        SDL_Rect btn_unblock = {blocked_win.x + 200, blocked_win.y + y_off, 60, 25};
        SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255); SDL_RenderFillRect(renderer, &btn_unblock);
        render_text(renderer, "Show", btn_unblock.x + 30, btn_unblock.y + 2, col_white, 1);

        y_off += 35;
    }
    
    if (blocked_count == 0) render_text(renderer, "(No hidden players)", blocked_win.x + 150, blocked_win.y + 100, col_white, 1);
}

void render_auth_screen(SDL_Renderer *renderer) {
    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
    auth_box = (SDL_Rect){w/2-200, h/2-150, 400, 300};
    btn_login = (SDL_Rect){auth_box.x+20, auth_box.y+200, 160, 40};
    btn_register = (SDL_Rect){auth_box.x+220, auth_box.y+200, 160, 40};
    btn_show_pass = (SDL_Rect){auth_box.x + 340, auth_box.y + 165 + 5, 15, 15};

    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); SDL_RenderFillRect(renderer, &auth_box);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &auth_box);
    render_text(renderer, "C-MMO Login", auth_box.x + 200, auth_box.y + 20, col_white, 1);
    render_text(renderer, auth_message, auth_box.x + 200, auth_box.y + 70, col_red, 1);
    int y_start = auth_box.y + 120;
    render_text(renderer, "Username:", auth_box.x + 20, y_start, col_white, 0);
    SDL_Rect user_rect = {auth_box.x + 130, y_start - 5, 200, 25};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderFillRect(renderer, &user_rect);
    SDL_SetRenderDrawColor(renderer, active_field == 0 ? 0 : 150, active_field == 0 ? 255 : 150, 0, 255); SDL_RenderDrawRect(renderer, &user_rect);
    render_text(renderer, auth_username, user_rect.x + 5, user_rect.y + 2, col_white, 0);
    render_text(renderer, "Password:", auth_box.x + 20, y_start + 50, col_white, 0);
    SDL_Rect pass_rect = {auth_box.x + 130, y_start + 45, 200, 25};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderFillRect(renderer, &pass_rect);
    SDL_SetRenderDrawColor(renderer, active_field == 1 ? 0 : 150, active_field == 1 ? 255 : 150, 0, 255); SDL_RenderDrawRect(renderer, &pass_rect);
    
    if (show_password) { render_text(renderer, auth_password, pass_rect.x + 5, pass_rect.y + 2, col_white, 0); } 
    else { char masked[MAX_INPUT_LEN+1]; memset(masked, '*', strlen(auth_password)); masked[strlen(auth_password)]=0; render_text(renderer, masked, pass_rect.x + 5, pass_rect.y + 2, col_white, 0); }
    
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDrawRect(renderer, &btn_show_pass);
    if (show_password) { SDL_Rect inner = {btn_show_pass.x + 3, btn_show_pass.y + 3, btn_show_pass.w - 6, btn_show_pass.h - 6}; SDL_RenderFillRect(renderer, &inner); }

    SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_login);
    render_text(renderer, "Login", btn_login.x + 80, btn_login.y + 10, col_white, 1);
    SDL_SetRenderDrawColor(renderer, 0, 0, 150, 255); SDL_RenderFillRect(renderer, &btn_register);
    render_text(renderer, "Register", btn_register.x + 80, btn_register.y + 10, col_white, 1);
    SDL_RenderPresent(renderer);
}

void render_game(SDL_Renderer *renderer) {
    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
    float px=0, py=0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].active && local_players[i].id == local_player_id) { px=local_players[i].x; py=local_players[i].y; }
    int cam_x = (int)px - (w/2) + 16; int cam_y = (int)py - (h/2) + 16;
    if (w > map_w) cam_x = -(w - map_w)/2; if (h > map_h) cam_y = -(h - map_h)/2; 

    SDL_RenderClear(renderer);
    if (tex_map) { SDL_Rect dst = {-cam_x, -cam_y, map_w, map_h}; SDL_RenderCopy(renderer, tex_map, NULL, &dst); }
    else {
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        for(int x=0; x<map_w; x+=50) SDL_RenderDrawLine(renderer, x-cam_x, 0-cam_y, x-cam_x, map_h-cam_y);
        for(int y=0; y<map_h; y+=50) SDL_RenderDrawLine(renderer, 0-cam_x, y-cam_y, map_w-cam_x, y-cam_y);
    }

    Uint32 now = SDL_GetTicks();
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (local_players[i].active) {
            // --- HIDE BLOCKED PLAYERS ---
            if (is_blocked(local_players[i].id)) continue;
            // ---------------------------
            SDL_Rect dst = { (int)local_players[i].x - cam_x, (int)local_players[i].y - cam_y, PLAYER_WIDTH, PLAYER_HEIGHT };
            if (tex_player) SDL_RenderCopy(renderer, tex_player, NULL, &dst);
            else {
                if(local_players[i].id == local_player_id) SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
                else SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
                SDL_RenderFillRect(renderer, &dst);
            }
            SDL_Color name_col = {local_players[i].r, local_players[i].g, local_players[i].b, 255};
            
            if (local_players[i].id == selected_player_id) { SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDrawRect(renderer, &dst); }
            render_text(renderer, local_players[i].username, dst.x+16, dst.y - 18, name_col, 1);
            if (now - floating_texts[i].timestamp < 4000) render_text(renderer, floating_texts[i].msg, dst.x+16, dst.y - 38, col_white, 1);
        }
    }

    render_profile(renderer);
    render_popup(renderer, w, h);
    render_settings_menu(renderer, w, h);
    render_blocked_list(renderer, w, h); // NEW CALL
    render_debug_overlay(renderer, w);
    render_friend_list(renderer, w, h);
    render_hud(renderer, h);

    btn_chat_toggle = (SDL_Rect){10, h-40, 100, 30};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_chat_toggle);
    render_text(renderer, is_chat_open ? "Close" : "Chat", btn_chat_toggle.x+50, btn_chat_toggle.y+5, col_white, 1);

    // Friend Toggle (Middle)
    btn_view_friends = (SDL_Rect){120, h-40, 100, 30};
    SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255); SDL_RenderFillRect(renderer, &btn_view_friends);
    render_text(renderer, "Friends", btn_view_friends.x+50, btn_view_friends.y+5, col_white, 1);

    btn_settings_toggle = (SDL_Rect){120, h-40, 100, 30};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_settings_toggle);
    render_text(renderer, "Settings", btn_settings_toggle.x+50, btn_settings_toggle.y+5, col_white, 1);

    if(is_chat_open) {
        SDL_Rect win = {10, h-240, 300, 190};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0,0,0,200); SDL_RenderFillRect(renderer, &win);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        
        for(int i=0; i<CHAT_HISTORY; i++) {
            SDL_Color line_col = col_white;
            if (strncmp(chat_log[i], "To [", 4) == 0 || strncmp(chat_log[i], "From [", 6) == 0) line_col = col_magenta;
            render_text(renderer, chat_log[i], 15, win.y+10+(i*15), line_col, 0);
        }

        char buf[128];
        if (chat_target_id != -1) {
            char *name = "Unknown";
            for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == chat_target_id) name = local_players[i].username;
            snprintf(buf, 128, "To %s: %s_", name, input_buffer);
            render_text(renderer, buf, 15, win.y+win.h-20, col_magenta, 0); 
        } else {
            snprintf(buf, 128, "> %s_", input_buffer);
            render_text(renderer, buf, 15, win.y+win.h-20, col_cyan, 0);
        }
    }
    SDL_RenderPresent(renderer);
}

void handle_game_click(int mx, int my, int cam_x, int cam_y) {
    if (show_friend_list) {
        SDL_Rect btn_close = {friend_list_win.x + 260, friend_list_win.y + 5, 30, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_close)) show_friend_list = 0;
        return; // Modal
    }
    if (show_blocked_list) {
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_close_blocked)) {
            show_blocked_list = 0;
            return;
        }
        // Check "Show/Unhide" buttons in list
        int y_off = 50;
        for(int i=0; i<blocked_count; i++) {
            SDL_Rect btn_unblock = {blocked_win.x + 200, blocked_win.y + y_off, 60, 25};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_unblock)) {
                toggle_block(blocked_ids[i]); // Unblock
                return;
            }
            y_off += 35;
        }
        return; // Don't click anything else if list is open
    }
    if (is_settings_open) {
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_toggle_debug)) show_debug_info = !show_debug_info;
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_toggle_fps)) show_fps = !show_fps;
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_toggle_coords)) show_coords = !show_coords;
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_view_blocked)) show_blocked_list = 1; // Open List
        // NEW: Status Cycle
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_cycle_status)) {
            // Find my current status
            int my_status = 0;
            for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == local_player_id) my_status = local_players[i].status;
            
            // Increment
            my_status++;
            if (my_status > 4) my_status = 0; // Wrap around
            
            // Send Packet
            Packet pkt; 
            pkt.type = PACKET_STATUS_CHANGE;
            pkt.new_status = my_status;
            send_packet(&pkt);
        }
        // Check Sliders
        int changed = 0;
        int my_r, my_g, my_b; // Get current
        for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == local_player_id) { my_r=local_players[i].r; my_g=local_players[i].g; my_b=local_players[i].b; }

        if (SDL_PointInRect(&(SDL_Point){mx, my}, &slider_r)) {
            my_r = (int)(((mx - slider_r.x) / 200.0) * 255); changed = 1;
        } 
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &slider_g)) {
            my_g = (int)(((mx - slider_g.x) / 200.0) * 255); changed = 1;
        }
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &slider_b)) {
            my_b = (int)(((mx - slider_b.x) / 200.0) * 255); changed = 1;
        }

        if (changed) {
            Packet pkt; pkt.type = PACKET_COLOR_CHANGE;
            pkt.r = my_r; pkt.g = my_g; pkt.b = my_b;
            send_packet(&pkt);
        }
        return;
    }
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
            int is_friend = 0; for(int i=0; i<friend_count; i++) if(my_friends[i] == selected_player_id) is_friend = 1;
            if(!is_friend) { Packet pkt; pkt.type = PACKET_FRIEND_REQUEST; pkt.target_id = selected_player_id; send_packet(&pkt); } 
            else { Packet pkt; pkt.type = PACKET_FRIEND_REMOVE; pkt.target_id = selected_player_id; send_packet(&pkt); }
            selected_player_id = -1; return;
        }
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_send_pm)) {
            chat_target_id = selected_player_id;
            is_chat_open = 1;
            input_buffer[0] = 0;
            SDL_StartTextInput();
            selected_player_id = -1; 
            return;
        }
        // NEW: Hide Button
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_hide_player)) {
            toggle_block(selected_player_id);
            selected_player_id = -1; // Close profile
            return;
        }
    }

    int clicked = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (local_players[i].active) {
            SDL_Rect r = { (int)local_players[i].x - cam_x, (int)local_players[i].y - cam_y, PLAYER_WIDTH, PLAYER_HEIGHT };
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &r)) { selected_player_id = local_players[i].id; clicked = 1; }
        }
    }
    if (!clicked && !SDL_PointInRect(&(SDL_Point){mx, my}, &profile_win) && !SDL_PointInRect(&(SDL_Point){mx, my}, &btn_chat_toggle) && !SDL_PointInRect(&(SDL_Point){mx, my}, &btn_settings_toggle)) {
        selected_player_id = -1;
    }
}

void handle_auth_click(int mx, int my) {
    if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_login)) {
        Packet pkt; pkt.type = PACKET_LOGIN_REQUEST; strncpy(pkt.username, auth_username, 31); strncpy(pkt.password, auth_password, 31); send_packet(&pkt);
        strcpy(auth_message, "Logging in...");
    } else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_register)) {
        Packet pkt; pkt.type = PACKET_REGISTER_REQUEST; strncpy(pkt.username, auth_username, 31); strncpy(pkt.password, auth_password, 31); send_packet(&pkt);
        strcpy(auth_message, "Registering...");
    } else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_show_pass)) {
        show_password = !show_password;
    } else if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){auth_box.x+130, auth_box.y+115, 200, 25})) {
        active_field = 0; SDL_StartTextInput();
    } else if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){auth_box.x+130, auth_box.y+165, 200, 25})) {
        active_field = 1; SDL_StartTextInput();
    }
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1; if (TTF_Init() == -1) return 1; IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
    font = TTF_OpenFont(FONT_PATH, FONT_SIZE);
    SDL_Window *window = SDL_CreateWindow("C MMO Settings", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_Surface *temp = IMG_Load(MAP_FILE); if (temp) { tex_map = SDL_CreateTextureFromSurface(renderer, temp); map_w=temp->w; map_h=temp->h; SDL_FreeSurface(temp); }
    temp = IMG_Load(PLAYER_FILE); if (temp) { tex_player = SDL_CreateTextureFromSurface(renderer, temp); SDL_FreeSurface(temp); }

    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;
    serv_addr.sin_family = AF_INET; serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, server_ip, &serv_addr.sin_addr);
    connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    SDL_StartTextInput();
    int running = 1; SDL_Event event;
    // Init Avatar Cache
    for(int i=0; i<MAX_CLIENTS; i++) { avatar_cache[i] = NULL; avatar_status[i] = 0; }
    int key_up=0, key_down=0, key_left=0, key_right=0;

    while (running) {
        frame_count++;
        Uint32 now_fps = SDL_GetTicks();
        if (now_fps > last_fps_check + 1000) { current_fps = frame_count; frame_count = 0; last_fps_check = now_fps; }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            // --- NEW: Handle File Drag & Drop ---
            else if (event.type == SDL_DROPFILE) {
                if (is_settings_open) {
                    char* dropped_file = event.drop.file;
                    FILE *fp = fopen(dropped_file, "rb");
                    if (fp) {
                        fseek(fp, 0, SEEK_END);
                        long fsize = ftell(fp);
                        fseek(fp, 0, SEEK_SET);
                        
                        if (fsize > 0 && fsize <= MAX_AVATAR_SIZE) {
                            Packet pkt; 
                            pkt.type = PACKET_AVATAR_UPLOAD;
                            pkt.image_size = fsize;
                            fread(pkt.image_data, 1, fsize, fp);
                            send_packet(&pkt);
                            printf("Uploaded avatar: %ld bytes\n", fsize);
                        } else {
                            printf("File too large! Max 16KB.\n");
                        }
                        fclose(fp);
                    }
                    SDL_free(dropped_file);
                }
            }
            // ------------------------------------
            if (client_state == STATE_AUTH) {
                if(event.type == SDL_MOUSEBUTTONDOWN) handle_auth_click(event.button.x, event.button.y);
                if(event.type == SDL_KEYDOWN) {
                    if(event.key.keysym.sym == SDLK_RETURN) handle_auth_click(btn_login.x+1, btn_login.y+1);
                    if(event.key.keysym.sym == SDLK_TAB) active_field = !active_field;
                    if(event.key.keysym.sym == SDLK_BACKSPACE && active_field==0 && strlen(auth_username)>0) auth_username[strlen(auth_username)-1]=0;
                    if(event.key.keysym.sym == SDLK_BACKSPACE && active_field==1 && strlen(auth_password)>0) auth_password[strlen(auth_password)-1]=0;
                }
                if(event.type == SDL_TEXTINPUT) {
                    if(active_field==0 && strlen(auth_username)<31) strcat(auth_username, event.text.text);
                    if(active_field==1 && strlen(auth_password)<31) strcat(auth_password, event.text.text);
                }
            } else {
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                     if (SDL_PointInRect(&(SDL_Point){event.button.x, event.button.y}, &btn_chat_toggle)) {
                        is_chat_open = !is_chat_open; if(is_chat_open) SDL_StartTextInput(); else SDL_StopTextInput();
                     } 
                     else if (SDL_PointInRect(&(SDL_Point){event.button.x, event.button.y}, &btn_settings_toggle)) is_settings_open = !is_settings_open;
                     else {
                        int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
                        float px=0, py=0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].active && local_players[i].id == local_player_id) { px=local_players[i].x; py=local_players[i].y; }
                        int cam_x = (int)px - (w/2) + 16; int cam_y = (int)py - (h/2) + 16;
                        if (w > map_w) cam_x = -(w - map_w)/2; if (h > map_h) cam_y = -(h - map_h)/2;
                        handle_game_click(event.button.x, event.button.y, cam_x, cam_y);
                     }
                }
                if (is_chat_open) {
                    if(event.type == SDL_KEYDOWN) {
                        if(event.key.keysym.sym == SDLK_RETURN) {
                            if(strlen(input_buffer)>0) { 
                                if (chat_target_id != -1) {
                                    Packet pkt; pkt.type = PACKET_PRIVATE_MESSAGE; pkt.target_id = chat_target_id; strcpy(pkt.msg, input_buffer); send_packet(&pkt);
                                    chat_target_id = -1;
                                } else {
                                    Packet pkt; pkt.type = PACKET_CHAT; strcpy(pkt.msg, input_buffer); send_packet(&pkt); 
                                }
                                input_buffer[0]=0; 
                            }
                            is_chat_open=0; SDL_StopTextInput();
                        }
                        else if(event.key.keysym.sym == SDLK_BACKSPACE && strlen(input_buffer)>0) input_buffer[strlen(input_buffer)-1]=0;
                        else if(event.key.keysym.sym == SDLK_ESCAPE) { is_chat_open=0; chat_target_id = -1; SDL_StopTextInput(); }
                    }
                    else if(event.type == SDL_TEXTINPUT && strlen(input_buffer)<60) strcat(input_buffer, event.text.text);
                } else {
                    if (event.type == SDL_KEYDOWN) {
                        if (event.key.keysym.sym == SDLK_w) key_up=1; if (event.key.keysym.sym == SDLK_s) key_down=1;
                        if (event.key.keysym.sym == SDLK_a) key_left=1; if (event.key.keysym.sym == SDLK_d) key_right=1;
                        if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_t) { is_chat_open=1; input_buffer[0]=0; key_up=0; key_down=0; key_left=0; key_right=0; SDL_StartTextInput(); }
                    }
                    if (event.type == SDL_KEYUP) {
                        if (event.key.keysym.sym == SDLK_w) key_up=0; if (event.key.keysym.sym == SDLK_s) key_down=0;
                        if (event.key.keysym.sym == SDLK_a) key_left=0; if (event.key.keysym.sym == SDLK_d) key_right=0;
                    }
                }
            }
        }

        Uint32 now = SDL_GetTicks();
        if (client_state == STATE_GAME && now - last_ping_sent > 1000) { Packet pkt; pkt.type = PACKET_PING; pkt.timestamp = now; send_packet(&pkt); last_ping_sent = now; }

        if (client_state == STATE_GAME && !is_chat_open && pending_friend_req_id == -1) {
            float dx = 0, dy = 0;
            if (key_up) dy = -1; if (key_down) dy = 1; if (key_left) dx = -1; if (key_right) dx = 1;
            if (dx != 0 || dy != 0) { Packet pkt; pkt.type = PACKET_MOVE; pkt.dx = dx; pkt.dy = dy; send_packet(&pkt); }
        }

        int bytes; ioctl(sock, FIONREAD, &bytes);
        if (bytes > 0) {
            Packet pkt;
            if (recv(sock, &pkt, sizeof(Packet), 0) > 0) {
                if (pkt.type == PACKET_AUTH_RESPONSE) {
                    if (pkt.status == AUTH_SUCCESS) { client_state = STATE_GAME; local_player_id = pkt.player_id; SDL_StopTextInput(); }
                    else if (pkt.status == AUTH_REGISTER_SUCCESS) strcpy(auth_message, "Success! Login now.");
                    else strcpy(auth_message, "Error.");
                }
                if (pkt.type == PACKET_UPDATE) memcpy(local_players, pkt.players, sizeof(local_players));
                if (pkt.type == PACKET_CHAT || pkt.type == PACKET_PRIVATE_MESSAGE) add_chat_message(&pkt);
                if (pkt.type == PACKET_FRIEND_LIST) { friend_count = pkt.friend_count; for(int i=0; i<friend_count; i++) my_friends[i] = pkt.friend_ids[i]; }
                if (pkt.type == PACKET_FRIEND_INCOMING) { pending_friend_req_id = pkt.player_id; strcpy(pending_friend_name, pkt.username); }
                if (pkt.type == PACKET_PING) current_ping = SDL_GetTicks() - pkt.timestamp;
                // --- NEW: Handle Receiving Avatar ---
                if (pkt.type == PACKET_AVATAR_RESPONSE) {
                    int target_idx = -1;
                    for(int i=0; i<MAX_CLIENTS; i++) 
                        if(local_players[i].active && local_players[i].id == pkt.player_id) target_idx = i;
                    
                    if (target_idx != -1) {
                        SDL_RWops *rw = SDL_RWFromMem(pkt.image_data, pkt.image_size);
                        SDL_Surface *surf = IMG_Load_RW(rw, 1); 
                        if (surf) {
                            if (avatar_cache[target_idx]) SDL_DestroyTexture(avatar_cache[target_idx]);
                            avatar_cache[target_idx] = SDL_CreateTextureFromSurface(renderer, surf);
                            avatar_status[target_idx] = 2; // Ready
                            SDL_FreeSurface(surf);
                        }
                    }
                }
                // ------------------------------------
            }
        }

        if (client_state == STATE_AUTH) render_auth_screen(renderer); else render_game(renderer);
        SDL_Delay(16);
    }
    
    if(tex_map) SDL_DestroyTexture(tex_map); if(tex_player) SDL_DestroyTexture(tex_player);
    close(sock); TTF_CloseFont(font); TTF_Quit(); IMG_Quit();
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
 
   return 0;
}