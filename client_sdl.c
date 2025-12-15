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
#include <SDL2/SDL_mixer.h>
#include <dirent.h> 

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
FriendInfo my_friends[20]; // Changed from int array to Struct array
int friend_count = 0;

// BLOCKED SYSTEM
int blocked_ids[50];
int blocked_count = 0;
int show_blocked_list = 0; 

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

// --- Add Friend Popup State ---
int show_add_friend_popup = 0;
char input_friend_id[10] = "";
char friend_popup_msg[128] = "";

SDL_Rect btn_friend_add_id_rect; // Global to store position for click handler
SDL_Rect friend_win_rect;
SDL_Rect btn_friend_close_rect;

SDL_Rect blocked_win_rect;
SDL_Rect btn_blocked_close_rect;

int cursor_pos = 0; // Tracks current text cursor index

int unread_chat_count = 0;
int show_unread_counter = 1; // Default ON
SDL_Rect btn_toggle_unread;

Uint32 last_input_tick = 0;
int afk_timeout_minutes = 2; // Default 2 minutes
int is_auto_afk = 0;         // Flag to know if we triggered it automatically
SDL_Rect slider_afk;

// --- Helpers ---
void send_packet(Packet *pkt) { send(sock, pkt, sizeof(Packet), 0); }

void load_servers() {
    FILE *fp = fopen("servers.txt", "r");
    if (!fp) return;
    server_count = 0;
    while (fscanf(fp, "%s %s %d", server_list[server_count].name, server_list[server_count].ip, &server_list[server_count].port) == 3) {
        server_count++;
        if (server_count >= 10) break;
    }
    fclose(fp);
}

void handle_text_edit(char *buffer, int max_len, SDL_Event *ev) {
    int len = strlen(buffer);

    if (ev->type == SDL_TEXTINPUT) {
        int add_len = strlen(ev->text.text);
        if (len + add_len <= max_len) {
            // Shift content to the right to make space
            memmove(buffer + cursor_pos + add_len, buffer + cursor_pos, len - cursor_pos + 1);
            // Insert new text
            memcpy(buffer + cursor_pos, ev->text.text, add_len);
            cursor_pos += add_len;
        }
    } 
    else if (ev->type == SDL_KEYDOWN) {
        if (ev->key.keysym.sym == SDLK_LEFT) {
            if (cursor_pos > 0) {
                // Move back one character (handle multi-byte UTF-8)
                do { cursor_pos--; } while (cursor_pos > 0 && (buffer[cursor_pos] & 0xC0) == 0x80);
            }
        }
        else if (ev->key.keysym.sym == SDLK_RIGHT) {
            if (cursor_pos < len) {
                // Move forward one character
                do { cursor_pos++; } while (cursor_pos < len && (buffer[cursor_pos] & 0xC0) == 0x80);
            }
        }
        else if (ev->key.keysym.sym == SDLK_BACKSPACE) {
            if (cursor_pos > 0) {
                int end = cursor_pos;
                int start = end;
                // Find start of previous character
                do { start--; } while (start > 0 && (buffer[start] & 0xC0) == 0x80);
                
                // Shift content left to overwrite
                memmove(buffer + start, buffer + end, len - end + 1);
                cursor_pos = start;
            }
        }
        else if (ev->key.keysym.sym == SDLK_DELETE) {
            if (cursor_pos < len) {
                int start = cursor_pos;
                int end = start;
                // Find end of current character
                do { end++; } while (end < len && (buffer[end] & 0xC0) == 0x80);
                
                // Shift content left
                memmove(buffer + start, buffer + end, len - end + 1);
            }
        }
        else if (ev->key.keysym.sym == SDLK_HOME) cursor_pos = 0;
        else if (ev->key.keysym.sym == SDLK_END) cursor_pos = len;
    }
}

void save_servers() {
    FILE *fp = fopen("servers.txt", "w");
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

int try_connect() {
    if (is_connected) close(sock);
    
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        strcpy(auth_message, "Socket Error");
        return 0;
    }
    
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(atoi(input_port));
    
    if (inet_pton(AF_INET, input_ip, &serv_addr.sin_addr) <= 0) {
        strcpy(auth_message, "Invalid IP Address");
        return 0;
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
    char path[128];
    snprintf(path, 128, "music/%s", music_playlist[current_track]);
    bgm = Mix_LoadMUS(path);
    if (bgm) Mix_PlayMusic(bgm, 1);
}

void init_audio() {
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) return;
    DIR *d; struct dirent *dir;
    d = opendir("music");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strstr(dir->d_name, ".mp3") || strstr(dir->d_name, ".ogg") || strstr(dir->d_name, ".wav")) {
                if (music_count < 20) strncpy(music_playlist[music_count++], dir->d_name, 63);
            }
        }
        closedir(d);
    } else system("mkdir -p music");
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

void render_text(SDL_Renderer *renderer, const char *text, int x, int y, SDL_Color color, int center) {
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


void render_input_with_cursor(SDL_Renderer *renderer, SDL_Rect rect, char *buffer, int is_active, int is_password) {
    // 1. Draw Box
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, is_active ? 0 : 100, is_active ? 255 : 100, 0, 255); SDL_RenderDrawRect(renderer, &rect);

    // 2. Prepare Display Text
    char display[256];
    if (is_password) {
        memset(display, '*', strlen(buffer));
        display[strlen(buffer)] = 0;
    } else {
        strcpy(display, buffer);
    }

    // 3. Render Text
    render_text(renderer, display, rect.x + 5, rect.y + 2, col_white, 0);

    // 4. Render Cursor (Only if active)
    if (is_active) {
        // Measure text width UP TO the cursor position
        char temp[256];
        strncpy(temp, display, cursor_pos);
        temp[cursor_pos] = 0;
        
        int w = 0, h = 0;
        if (strlen(temp) > 0) TTF_SizeText(font, temp, &w, &h);
        
        // Draw blinking line (Blink every 500ms)
        if ((SDL_GetTicks() / 500) % 2 == 0) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawLine(renderer, rect.x + 5 + w, rect.y + 4, rect.x + 5 + w, rect.y + 20);
        }
    }
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

void render_hud(SDL_Renderer *renderer, int screen_h) {
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
    int screen_w; SDL_GetRendererOutputSize(renderer, &screen_w, NULL);

    // INBOX BUTTON (Top Right)
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

    int y = win.y + 40;
    if (inbox_count == 0) render_text(renderer, "No new requests.", win.x + 80, y, col_white, 0);

    for(int i=0; i<inbox_count; i++) {
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

        y += 55;
    }
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

void render_debug_overlay(SDL_Renderer *renderer, int screen_w) {
    if (!show_debug_info) return;
    char lines[10][128]; int line_count = 0;
    snprintf(lines[line_count++], 128, "Ping: %d ms", current_ping);
    snprintf(lines[line_count++], 128, "Server IP: %s", server_ip);
    float px=0, py=0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].active && local_players[i].id == local_player_id) { px=local_players[i].x; py=local_players[i].y; }
    snprintf(lines[line_count++], 128, "Pos: %.1f, %.1f", px, py);
    const unsigned char *renderer_str = glGetString(GL_RENDERER);
    if (renderer_str) snprintf(lines[line_count++], 128, "GPU: %s", renderer_str); else snprintf(lines[line_count++], 128, "GPU: Unknown");
    SDL_version compiled; SDL_VERSION(&compiled); snprintf(lines[line_count++], 128, "SDL: %d.%d.%d", compiled.major, compiled.minor, compiled.patch);
    struct utsname buffer; if (uname(&buffer) == 0) snprintf(lines[line_count++], 128, "OS: %s %s", buffer.sysname, buffer.release); else snprintf(lines[line_count++], 128, "OS: Unknown");
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
        render_text(renderer, lines[i], dbg_box.x + 10, y, color, 0); y += 20;
    }
}

void render_settings_menu(SDL_Renderer *renderer, int screen_w, int screen_h) {
    if (!is_settings_open) return;
    
    // Increased Height to 650 to prevent overlap
    settings_win = (SDL_Rect){screen_w/2 - 150, screen_h/2 - 325, 300, 720}; 
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255); SDL_RenderFillRect(renderer, &settings_win);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &settings_win);
    render_text(renderer, "Settings", settings_win.x + 150, settings_win.y + 10, col_white, 1);

    // 1. Toggles
    btn_toggle_debug = (SDL_Rect){settings_win.x + 20, settings_win.y + 50, 20, 20};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &btn_toggle_debug);
    if (show_debug_info) { SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_Rect c={btn_toggle_debug.x+4,btn_toggle_debug.y+4,12,12}; SDL_RenderFillRect(renderer,&c); }
    render_text(renderer, "Show Debug Info", settings_win.x + 50, settings_win.y + 50, col_white, 0);

    btn_toggle_fps = (SDL_Rect){settings_win.x + 20, settings_win.y + 90, 20, 20};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &btn_toggle_fps);
    if (show_fps) { SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_Rect c={btn_toggle_fps.x+4,btn_toggle_fps.y+4,12,12}; SDL_RenderFillRect(renderer,&c); }
    render_text(renderer, "Show FPS", settings_win.x + 50, settings_win.y + 90, col_white, 0);

    btn_toggle_coords = (SDL_Rect){settings_win.x + 20, settings_win.y + 130, 20, 20};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &btn_toggle_coords);
    if (show_coords) { SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_Rect c={btn_toggle_coords.x+4,btn_toggle_coords.y+4,12,12}; SDL_RenderFillRect(renderer,&c); }
    render_text(renderer, "Show Coordinates", settings_win.x + 50, settings_win.y + 130, col_white, 0);

    btn_toggle_unread = (SDL_Rect){settings_win.x + 20, settings_win.y + 170, 20, 20};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &btn_toggle_unread);
    if (show_unread_counter) { SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_Rect c={btn_toggle_unread.x+4,btn_toggle_unread.y+4,12,12}; SDL_RenderFillRect(renderer,&c); }
    render_text(renderer, "Show Unread Counter", settings_win.x + 50, settings_win.y + 170, col_white, 0);

    // 2. Blocked Players
    btn_view_blocked = (SDL_Rect){settings_win.x + 20, settings_win.y + 210, 260, 30};
    SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255); SDL_RenderFillRect(renderer, &btn_view_blocked);
    render_text(renderer, "Manage Blocked Players", btn_view_blocked.x + 130, btn_view_blocked.y + 5, col_white, 1);

    // 3. ID & Status
    char id_str[32]; snprintf(id_str, 32, "My ID: %d", local_player_id);
    render_text(renderer, id_str, settings_win.x + 150, settings_win.y + 250, (SDL_Color){150, 150, 255, 255}, 1);

    int my_status = 0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == local_player_id) my_status = local_players[i].status;
    btn_cycle_status = (SDL_Rect){settings_win.x + 20, settings_win.y + 270, 260, 30};
    SDL_SetRenderDrawColor(renderer, 50, 50, 100, 255); SDL_RenderFillRect(renderer, &btn_cycle_status);
    char status_str[64]; snprintf(status_str, 64, "Status: %s", status_names[my_status]);
    render_text(renderer, status_str, btn_cycle_status.x + 130, btn_cycle_status.y + 5, get_status_color(my_status), 1);

    SDL_Rect btn_nick = {settings_win.x + 20, settings_win.y + 310, 260, 30};
    SDL_SetRenderDrawColor(renderer, 100, 50, 150, 255); 
    SDL_RenderFillRect(renderer, &btn_nick);
    render_text(renderer, "Change Nickname", btn_nick.x + 130, btn_nick.y + 5, col_white, 1);

    // 4. Sliders
    int start_y = settings_win.y + 360;
    int my_r=255, my_g=255, my_b=255;
    for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == local_player_id) { my_r=local_players[i].r; my_g=local_players[i].g; my_b=local_players[i].b; }
    render_text(renderer, "Name Color", settings_win.x + 150, start_y, (SDL_Color){my_r, my_g, my_b, 255}, 1);
    
    slider_r = (SDL_Rect){settings_win.x + 50, start_y + 30, 200, 20};
    slider_g = (SDL_Rect){settings_win.x + 50, start_y + 60, 200, 20};
    slider_b = (SDL_Rect){settings_win.x + 50, start_y + 90, 200, 20};
    
    SDL_SetRenderDrawColor(renderer, 50, 0, 0, 255); SDL_RenderFillRect(renderer, &slider_r);
    SDL_Rect fill_r = {slider_r.x, slider_r.y, (int)((my_r/255.0)*200), 20};
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); SDL_RenderFillRect(renderer, &fill_r);

    SDL_SetRenderDrawColor(renderer, 0, 50, 0, 255); SDL_RenderFillRect(renderer, &slider_g);
    SDL_Rect fill_g = {slider_g.x, slider_g.y, (int)((my_g/255.0)*200), 20};
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); SDL_RenderFillRect(renderer, &fill_g);

    SDL_SetRenderDrawColor(renderer, 0, 0, 50, 255); SDL_RenderFillRect(renderer, &slider_b);
    SDL_Rect fill_b = {slider_b.x, slider_b.y, (int)((my_b/255.0)*200), 20};
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); SDL_RenderFillRect(renderer, &fill_b);

    // Volume
    render_text(renderer, "Music Volume", settings_win.x + 150, start_y + 120, col_white, 1);
    slider_volume = (SDL_Rect){settings_win.x + 50, start_y + 150, 200, 20};
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); SDL_RenderFillRect(renderer, &slider_volume);
    SDL_Rect fill_vol = {slider_volume.x, slider_volume.y, (int)((music_volume/128.0)*200), 20};
    SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255); SDL_RenderFillRect(renderer, &fill_vol);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &slider_volume);

    // Auto-AFK Slider (with Labels)
    char afk_str[64]; snprintf(afk_str, 64, "Auto-AFK: %d min", afk_timeout_minutes);
    render_text(renderer, afk_str, settings_win.x + 150, start_y + 180, col_white, 1);
    
    slider_afk = (SDL_Rect){settings_win.x + 50, start_y + 210, 200, 20};
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); SDL_RenderFillRect(renderer, &slider_afk);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &slider_afk);
    
    float afk_pct = (afk_timeout_minutes - 2) / 8.0f; 
    SDL_Rect fill_afk = {slider_afk.x, slider_afk.y, (int)(afk_pct * 200), 20};
    SDL_SetRenderDrawColor(renderer, 255, 165, 0, 255); SDL_RenderFillRect(renderer, &fill_afk);

    // --- Labels for Slider ---
    render_text(renderer, "2m", slider_afk.x - 15, slider_afk.y + 2, col_white, 0);  // Min
    render_text(renderer, "6m", slider_afk.x + 100 - 10, slider_afk.y + 2, col_white, 0); // Mid
    render_text(renderer, "10m", slider_afk.x + 205, slider_afk.y + 2, col_white, 0); // Max
    // -------------------------

    // 5. Disconnect (Shifted to +250 relative to start_y)
    SDL_Rect btn_disconnect = {settings_win.x + 20, start_y + 250, 260, 30};
    SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_disconnect);
    render_text(renderer, "Disconnect / Logout", btn_disconnect.x + 130, btn_disconnect.y + 5, col_white, 1);

    render_text(renderer, "Drag & Drop Image here", settings_win.x + 150, start_y + 290, col_yellow, 1);
    render_text(renderer, "to upload Avatar (<16KB)", settings_win.x + 150, start_y + 310, col_yellow, 1);

    // --- Nickname Popup ---
    if (show_nick_popup) {
        SDL_Rect pop = {screen_w/2 - 150, screen_h/2 - 150, 300, 300};
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); SDL_RenderFillRect(renderer, &pop);
        SDL_SetRenderDrawColor(renderer, 200, 200, 0, 255); SDL_RenderDrawRect(renderer, &pop);
        
        render_text(renderer, "Change Nickname", pop.x+150, pop.y+10, col_yellow, 1);
        render_text(renderer, auth_message, pop.x+150, pop.y+35, col_red, 1);

        int y = pop.y + 60;
        render_text(renderer, "New Name:", pop.x+20, y, col_white, 0);
        render_input_with_cursor(renderer, (SDL_Rect){pop.x+20, y+20, 260, 25}, nick_new, active_field==10, 0);

        y += 60;
        render_text(renderer, "Type 'CONFIRM':", pop.x+20, y, col_white, 0);
        render_input_with_cursor(renderer, (SDL_Rect){pop.x+20, y+20, 260, 25}, nick_confirm, active_field==11, 0);

        y += 60;
        render_text(renderer, "Current Password:", pop.x+20, y, col_white, 0);
        render_input_with_cursor(renderer, (SDL_Rect){pop.x+20, y+20, 260, 25}, nick_pass, active_field==12, 1); 

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

    // 1. Calculate Required Width (Increased padding for Delete button)
    int max_text_w = 200; 
    for(int i=0; i<friend_count; i++) {
        char temp_str[128];
        if (my_friends[i].is_online) snprintf(temp_str, 128, "%s (Online)", my_friends[i].username);
        else snprintf(temp_str, 128, "%s (Last: %s)", my_friends[i].username, my_friends[i].last_login);
        int fw, fh; TTF_SizeText(font, temp_str, &fw, &fh);
        if (fw > max_text_w) max_text_w = fw;
    }

    // Increased padding from 60 to 110 to fit the "Del" button
    int win_w = max_text_w + 110; 
    if (win_w < 300) win_w = 300; 

    // 2. Setup Window
    friend_list_win = (SDL_Rect){w/2 - (win_w/2), h/2 - 200, win_w, 400};

    // Draw Background
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); SDL_RenderFillRect(renderer, &friend_list_win);
    SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255); SDL_RenderDrawRect(renderer, &friend_list_win);
    
    // Draw Title
    int title_w, title_h; TTF_SizeText(font, "Friends List", &title_w, &title_h);
    render_text(renderer, "Friends List", friend_list_win.x + (win_w/2) - (title_w/2), friend_list_win.y + 10, col_green, 0);

    // 3. Close Button
    SDL_Rect btn_close = {friend_list_win.x + win_w - 40, friend_list_win.y + 5, 30, 30};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_close);
    render_text(renderer, "X", btn_close.x + 10, btn_close.y + 5, col_white, 1);

    // 4. "Add ID" Button
    btn_friend_add_id_rect = (SDL_Rect){friend_list_win.x + 20, friend_list_win.y + 45, 100, 30};
    SDL_SetRenderDrawColor(renderer, 50, 50, 150, 255); SDL_RenderFillRect(renderer, &btn_friend_add_id_rect);
    render_text(renderer, "+ ID", btn_friend_add_id_rect.x+30, btn_friend_add_id_rect.y+5, col_white, 0);

    // 5. Render List with Delete Buttons
    int y_off = 85; 
    for(int i=0; i<friend_count; i++) {
        // Text
        char display[128];
        SDL_Color text_col = col_white;
        if (my_friends[i].is_online) {
            snprintf(display, 128, "%s (Online)", my_friends[i].username);
            text_col = col_green;
        } else {
            snprintf(display, 128, "%s (Last: %s)", my_friends[i].username, my_friends[i].last_login);
            text_col = (SDL_Color){150, 150, 150, 255}; 
        }
        render_text(renderer, display, friend_list_win.x + 20, friend_list_win.y + y_off, text_col, 0);

        // Delete Button
        SDL_Rect btn_del = {friend_list_win.x + win_w - 50, friend_list_win.y + y_off, 40, 20};
        SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_del);
        render_text(renderer, "Del", btn_del.x+8, btn_del.y+2, col_white, 0);

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
    SDL_Rect btn_accept = {popup_win.x + 20, popup_win.y + 70, 120, 30}; SDL_Rect btn_deny = {popup_win.x + 160, popup_win.y + 70, 120, 30};
    SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_accept);
    render_text(renderer, "Accept", btn_accept.x + 60, btn_accept.y + 5, col_white, 1);
    SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_deny);
    render_text(renderer, "Deny", btn_deny.x + 60, btn_deny.y + 5, col_white, 1);
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
    profile_win.h = 280; 

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
    
    // --- FIX: STRUCT ACCESS ---
    int is_friend = 0; 
    for(int i=0; i<friend_count; i++) if(my_friends[i].id == selected_player_id) is_friend = 1;
    // --------------------------

    SDL_Rect btn = {profile_win.x + 20, start_y, btn_w, 30}; 
    if (!is_friend) { SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255); SDL_RenderFillRect(renderer, &btn); render_text(renderer, "+ Add Friend", btn.x + (btn_w/2), btn.y + 5, col_white, 1); } 
    else { SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn); render_text(renderer, "- Remove Friend", btn.x + (btn_w/2), btn.y + 5, col_white, 1); }
    btn_add_friend = btn;

    btn_send_pm = (SDL_Rect){profile_win.x + 20, start_y + 40, btn_w, 30};
    SDL_SetRenderDrawColor(renderer, 100, 0, 100, 255); SDL_RenderFillRect(renderer, &btn_send_pm);
    render_text(renderer, "Send Message", btn_send_pm.x + (btn_w/2), btn_send_pm.y + 5, col_white, 1);

    SDL_Rect btn_hide = {profile_win.x + 20, start_y + 80, btn_w, 30};
    int hidden = is_blocked(selected_player_id);
    if (hidden) { SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_hide); render_text(renderer, "Unhide", btn_hide.x + (btn_w/2), btn_hide.y + 5, col_white, 1); } 
    else { SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255); SDL_RenderFillRect(renderer, &btn_hide); render_text(renderer, "Hide", btn_hide.x + (btn_w/2), btn_hide.y + 5, col_white, 1); }
    btn_hide_player_dyn = btn_hide;
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

    // Buttons & Server List (Keep existing)
    btn_login = (SDL_Rect){auth_box.x+20, auth_box.y+280, 160, 40};
    btn_register = (SDL_Rect){auth_box.x+220, auth_box.y+280, 160, 40};
    btn_open_servers = (SDL_Rect){auth_box.x+100, auth_box.y+340, 200, 30};

    SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_login);
    render_text(renderer, "Login", btn_login.x + 80, btn_login.y + 10, col_white, 1);
    SDL_SetRenderDrawColor(renderer, 0, 0, 150, 255); SDL_RenderFillRect(renderer, &btn_register);
    render_text(renderer, "Register", btn_register.x + 80, btn_register.y + 10, col_white, 1);
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_open_servers);
    render_text(renderer, "Saved Servers", btn_open_servers.x + 100, btn_open_servers.y + 5, col_white, 1);

    if (show_server_list) {
        server_list_win = (SDL_Rect){w/2 + 220, h/2 - 200, 250, 400}; 
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255); SDL_RenderFillRect(renderer, &server_list_win);
        SDL_SetRenderDrawColor(renderer, 200, 200, 0, 255); SDL_RenderDrawRect(renderer, &server_list_win);
        render_text(renderer, "Select Server", server_list_win.x + 125, server_list_win.y + 10, col_yellow, 1);
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
    SDL_RenderPresent(renderer);
}

void render_blocked_list(SDL_Renderer *renderer, int w, int h) {
    if (!show_blocked_list) return;

    blocked_win = (SDL_Rect){w/2 - 150, h/2 - 200, 300, 400};
    blocked_win_rect = blocked_win; // <--- SAVE GLOBAL
    
    // Background
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); SDL_RenderFillRect(renderer, &blocked_win);
    SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255); SDL_RenderDrawRect(renderer, &blocked_win);
    
    render_text(renderer, "Blocked Players", blocked_win.x + 150, blocked_win.y + 10, col_red, 1);

    // Close Button
    btn_close_blocked = (SDL_Rect){blocked_win.x + 260, blocked_win.y + 5, 30, 30};
    btn_blocked_close_rect = btn_close_blocked; // <--- SAVE GLOBAL

    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_close_blocked);
    render_text(renderer, "X", btn_close_blocked.x + 15, btn_close_blocked.y + 5, col_white, 1);

    // List
    int y_off = 50;
    for(int i=0; i<blocked_count; i++) {
        int id = blocked_ids[i];
        
        char display[64]; snprintf(display, 64, "ID: %d", id);
        for(int p=0; p<MAX_CLIENTS; p++) 
            if(local_players[p].active && local_players[p].id == id) 
                snprintf(display, 64, "%s", local_players[p].username);

        render_text(renderer, display, blocked_win.x + 20, blocked_win.y + y_off, col_white, 0);

        SDL_Rect btn_unblock = {blocked_win.x + 200, blocked_win.y + y_off, 60, 25};
        SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255); SDL_RenderFillRect(renderer, &btn_unblock);
        render_text(renderer, "Show", btn_unblock.x + 30, btn_unblock.y + 2, col_white, 1);

        y_off += 35;
    }
    
    if (blocked_count == 0) render_text(renderer, "(No hidden players)", blocked_win.x + 150, blocked_win.y + 100, col_white, 1);
}

void render_game(SDL_Renderer *renderer) {
    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
    float px=0, py=0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].active && local_players[i].id == local_player_id) { px=local_players[i].x; py=local_players[i].y; }
    int cam_x = (int)px - (w/2) + 16; int cam_y = (int)py - (h/2) + 16;
    if (w > map_w) cam_x = -(w - map_w)/2; if (h > map_h) cam_y = -(h - map_h)/2; 

    // 1. Draw Map
    SDL_RenderClear(renderer);
    if (tex_map) { SDL_Rect dst = {-cam_x, -cam_y, map_w, map_h}; SDL_RenderCopy(renderer, tex_map, NULL, &dst); }
    else {
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        for(int x=0; x<map_w; x+=50) SDL_RenderDrawLine(renderer, x-cam_x, 0-cam_y, x-cam_x, map_h-cam_y);
        for(int y=0; y<map_h; y+=50) SDL_RenderDrawLine(renderer, 0-cam_x, y-cam_y, map_w-cam_x, y-cam_y);
    }

    // 2. Draw Players & Floating Text
    Uint32 now = SDL_GetTicks();
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (local_players[i].active) {
            if (is_blocked(local_players[i].id)) continue;
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

    // 3. Draw UI Windows (Order matters!)
    render_profile(renderer);
    render_popup(renderer, w, h);
    render_settings_menu(renderer, w, h);
    render_blocked_list(renderer, w, h); 
    render_friend_list(renderer, w, h);
    
    // --- MISSING FUNCTIONS ADDED HERE ---
    render_inbox(renderer, w, h);           
    render_add_friend_popup(renderer, w, h); 
    // ------------------------------------

    render_debug_overlay(renderer, w);
    render_hud(renderer, h);

    // 4. Draw HUD Buttons
    btn_chat_toggle = (SDL_Rect){10, h-40, 100, 30};
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

    btn_view_friends = (SDL_Rect){120, h-40, 100, 30};
    SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255); SDL_RenderFillRect(renderer, &btn_view_friends);
    render_text(renderer, "Friends", btn_view_friends.x+50, btn_view_friends.y+5, col_white, 1);

    btn_settings_toggle = (SDL_Rect){230, h-40, 100, 30};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_settings_toggle);
    render_text(renderer, "Settings", btn_settings_toggle.x+50, btn_settings_toggle.y+5, col_white, 1);

 // 5. Draw Chat Overlay
    if(is_chat_open) {
        SDL_Rect win = {10, h-240, 300, 190};
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

        // 3. Render Text (Simple scrolling check)
        int w, h;
        TTF_SizeText(font, full_str, &w, &h);
        int render_x = 15;
        int render_y = win.y+win.h-20;

        if (w <= 270) {
            render_text(renderer, full_str, render_x, render_y, input_col, 0);
        } else {
            // If too long, just show the end (simplification for now)
            int len = strlen(full_str);
            for(int i=0; i<len; i++) {
                const char* sub = &full_str[i];
                int sub_w;
                TTF_SizeText(font, sub, &sub_w, &h);
                if (sub_w <= 270) {
                    render_text(renderer, sub, render_x, render_y, input_col, 0);
                    break;
                }
            }
        }

        // 4. Render Blinking Cursor
        // We calculate width of (Prefix + InputBuffer UP TO cursor_pos)
        char cursor_sub[256];
        snprintf(cursor_sub, 256, "%s", prefix);
        strncat(cursor_sub, input_buffer, cursor_pos); // Append only up to cursor
        
        int cw, ch;
        TTF_SizeText(font, cursor_sub, &cw, &ch);
        
        // Only draw if within bounds (simple check)
        if (cw <= 270) {
            if ((SDL_GetTicks() / 500) % 2 == 0) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDrawLine(renderer, render_x + cw, render_y + 2, render_x + cw, render_y + 14);
            }
        }
        // ---------------------------------
    }
    SDL_RenderPresent(renderer);
}

void handle_game_click(int mx, int my, int cam_x, int cam_y, int w, int h) {

    // 1. TOP PRIORITY: HUD BUTTONS
    SDL_Rect btn_inbox_check = {w - 50, 10, 40, 40};
    if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_inbox_check)) {
        is_inbox_open = !is_inbox_open;
        if (is_inbox_open) { show_friend_list = 0; show_add_friend_popup = 0; is_settings_open = 0; }
        return;
    }

    // 2A. INBOX WINDOW
    if (is_inbox_open) {
        SDL_Rect win = {w - 320, 60, 300, 300};
        int y = win.y + 40;
        for(int i=0; i<inbox_count; i++) {
            SDL_Rect btn_acc = {win.x+170, y+25, 50, 20};
            SDL_Rect btn_deny = {win.x+230, y+25, 50, 20};
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

    // 2B. ADD FRIEND POPUP
    if (show_add_friend_popup) {
        SDL_Rect pop = {w/2 - 150, h/2 - 100, 300, 200};
        SDL_Rect input = {pop.x+50, pop.y+60, 200, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &input)) { active_field = 20; SDL_StartTextInput(); return; }
        SDL_Rect btn_ok = {pop.x+50, pop.y+130, 80, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_ok)) {
            int id = atoi(input_friend_id);
            if (id > 0) {
                Packet pkt; pkt.type = PACKET_FRIEND_REQUEST; pkt.target_id = id; send_packet(&pkt);
                show_add_friend_popup = 0; active_field = -1;
            } return;
        }
        SDL_Rect btn_cancel = {pop.x+170, pop.y+130, 80, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_cancel)) { show_add_friend_popup = 0; active_field = -1; return; }
        return; 
    }

    // 2C. BLOCKED LIST
    if (show_blocked_list) {
        SDL_Rect b_win = {w/2 - 150, h/2 - 200, 300, 400}; 
        SDL_Rect btn_close_b = {b_win.x + 260, b_win.y + 5, 30, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_close_b)) { show_blocked_list = 0; return; }
        int y_off = 50;
        for(int i=0; i<blocked_count; i++) {
            SDL_Rect btn_unblock = {b_win.x + 200, b_win.y + y_off, 60, 25};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_unblock)) { toggle_block(blocked_ids[i]); return; }
            y_off += 35;
        }
        return;
    }

    // 3. SETTINGS
    if (is_settings_open) {
        if (show_nick_popup) {
            SDL_Rect pop = {w/2 - 150, h/2 - 150, 300, 300};
            int y = pop.y + 60;
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){pop.x+20, y+20, 260, 25})) { active_field = 10; SDL_StartTextInput(); return; }
            y += 60; if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){pop.x+20, y+20, 260, 25})) { active_field = 11; SDL_StartTextInput(); return; }
            y += 60; if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){pop.x+20, y+20, 260, 25})) { active_field = 12; SDL_StartTextInput(); return; }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){pop.x+20, pop.y+240, 120, 30})) {
                Packet pkt; pkt.type = PACKET_CHANGE_NICK_REQUEST;
                strncpy(pkt.username, nick_new, 31); strncpy(pkt.msg, nick_confirm, 63); strncpy(pkt.password, nick_pass, 31);
                send_packet(&pkt); strcpy(auth_message, "Processing..."); return;
            }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){pop.x+160, pop.y+240, 120, 30})) { show_nick_popup = 0; active_field = -1; return; }
            return;
        }

        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_toggle_debug)) show_debug_info = !show_debug_info;
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_toggle_fps)) show_fps = !show_fps;
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_toggle_coords)) show_coords = !show_coords;
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_toggle_unread)) show_unread_counter = !show_unread_counter;
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_view_blocked)) show_blocked_list = 1; 
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_cycle_status)) {
            int my_status = 0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == local_player_id) my_status = local_players[i].status;
            my_status++; if (my_status > 4) my_status = 0; 
            Packet pkt; pkt.type = PACKET_STATUS_CHANGE; pkt.new_status = my_status; send_packet(&pkt);
        }
        
        SDL_Rect btn_nick = {settings_win.x + 20, settings_win.y + 310, 260, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_nick)) {
            show_nick_popup = 1; nick_new[0] = 0; nick_confirm[0] = 0; nick_pass[0] = 0;
            strcpy(auth_message, "Enter details."); return;
        }

        int changed = 0; int my_r = 0, my_g = 0, my_b = 0; 
        for(int i=0; i<MAX_CLIENTS; i++) { if(local_players[i].active && local_players[i].id == local_player_id) { my_r=local_players[i].r; my_g=local_players[i].g; my_b=local_players[i].b; } }
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &slider_r)) { my_r = (int)(((float)(mx - slider_r.x) / slider_r.w) * 255); changed = 1; } 
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &slider_g)) { my_g = (int)(((float)(mx - slider_g.x) / slider_g.w) * 255); changed = 1; }
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &slider_b)) { my_b = (int)(((float)(mx - slider_b.x) / slider_b.w) * 255); changed = 1; }
        if (changed) { 
            if(my_r < 0) my_r = 0; if(my_r > 255) my_r = 255; if(my_g < 0) my_g = 0; if(my_g > 255) my_g = 255; if(my_b < 0) my_b = 0; if(my_b > 255) my_b = 255;
            Packet pkt; pkt.type = PACKET_COLOR_CHANGE; pkt.r = my_r; pkt.g = my_g; pkt.b = my_b; send_packet(&pkt); 
        }

        if (SDL_PointInRect(&(SDL_Point){mx, my}, &slider_volume)) {
            float pct = (mx - slider_volume.x) / 200.0f; if (pct < 0) pct = 0; if (pct > 1) pct = 1;
            music_volume = (int)(pct * 128); Mix_VolumeMusic(music_volume);
        }

        // --- NEW: Handle AFK Slider ---
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &slider_afk)) {
            float pct = (mx - slider_afk.x) / 200.0f; 
            if (pct < 0) pct = 0; if (pct > 1) pct = 1;
            
            // Map 0.0-1.0 to 2-10 integer range
            afk_timeout_minutes = 2 + (int)(pct * 8.0f + 0.5f); // +0.5 for rounding
        }
        // ------------------------------

        SDL_Rect btn_disconnect = {settings_win.x + 20, settings_win.y + 610, 260, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_disconnect)) {
            if(sock > 0) close(sock); sock = -1; is_connected = 0; Mix_HaltMusic();
            client_state = STATE_AUTH; is_settings_open = 0; local_player_id = -1;
            for(int i=0; i<MAX_CLIENTS; i++) { local_players[i].active = 0; local_players[i].id = -1; }
            friend_count = 0; chat_log_count = 0; for(int i=0; i<CHAT_HISTORY; i++) strcpy(chat_log[i], "");
            strcpy(auth_message, "Logged out."); return;
        }
        return; 
    }

    // 4. FRIENDS LIST
    if (show_friend_list) {
        int max_text_w = 200;
        for(int i=0; i<friend_count; i++) {
            char temp_str[128];
            if (my_friends[i].is_online) snprintf(temp_str, 128, "%s (Online)", my_friends[i].username);
            else snprintf(temp_str, 128, "%s (Last: %s)", my_friends[i].username, my_friends[i].last_login);
            int fw, fh; TTF_SizeText(font, temp_str, &fw, &fh);
            if (fw > max_text_w) max_text_w = fw;
        }
        // Match updated render padding (+110)
        int win_w = max_text_w + 110; if (win_w < 300) win_w = 300; 
        SDL_Rect f_win = {w/2 - (win_w/2), h/2 - 200, win_w, 400};
        
        // "Add ID" Button
        SDL_Rect btn_add_id = {f_win.x + 20, f_win.y + 45, 100, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_add_id)) { show_add_friend_popup = 1; input_friend_id[0] = 0; friend_popup_msg[0] = 0; return; }

        // --- NEW: Delete Buttons ---
        int y_off = 85; 
        for(int i=0; i<friend_count; i++) {
            SDL_Rect btn_del = {f_win.x + win_w - 50, f_win.y + y_off, 40, 20};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_del)) {
                Packet pkt; pkt.type = PACKET_FRIEND_REMOVE; pkt.target_id = my_friends[i].id; send_packet(&pkt);
                return;
            }
            y_off += 30;
        }
        // ----------------------------

        SDL_Rect btn_close = {f_win.x + win_w - 40, f_win.y + 5, 30, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_close)) show_friend_list = 0;
        return; 
    }

    // 5. TOASTS
    if (pending_friend_req_id != -1) {
        SDL_Rect btn_accept = {popup_win.x+20, popup_win.y+70, 120, 30};
        SDL_Rect btn_deny = {popup_win.x+160, popup_win.y+70, 120, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_accept)) {
            Packet pkt; pkt.type = PACKET_FRIEND_RESPONSE; pkt.target_id = pending_friend_req_id; pkt.response_accepted = 1; send_packet(&pkt); pending_friend_req_id = -1;
        } else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_deny)) pending_friend_req_id = -1;
        return;
    }

    // 6. PROFILE & WORLD
    if (selected_player_id != -1 && selected_player_id != local_player_id) {
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_add_friend)) {
            int is_friend = 0; for(int i=0; i<friend_count; i++) if(my_friends[i].id == selected_player_id) is_friend = 1;
            if(!is_friend) { Packet pkt; pkt.type = PACKET_FRIEND_REQUEST; pkt.target_id = selected_player_id; send_packet(&pkt); } 
            else { Packet pkt; pkt.type = PACKET_FRIEND_REMOVE; pkt.target_id = selected_player_id; send_packet(&pkt); }
            selected_player_id = -1; return;
        }
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_send_pm)) {
            chat_target_id = selected_player_id; is_chat_open = 1; input_buffer[0] = 0; SDL_StartTextInput(); selected_player_id = -1; return;
        }
        else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_hide_player_dyn)) {
            toggle_block(selected_player_id); selected_player_id = -1; return;
        }
    }

    int clicked = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (local_players[i].active) {
            SDL_Rect r = { (int)local_players[i].x - cam_x, (int)local_players[i].y - cam_y, PLAYER_WIDTH, PLAYER_HEIGHT };
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &r)) { selected_player_id = local_players[i].id; clicked = 1; }
        }
    }
    
    if (!clicked && !SDL_PointInRect(&(SDL_Point){mx, my}, &profile_win) && !SDL_PointInRect(&(SDL_Point){mx, my}, &btn_chat_toggle) && !SDL_PointInRect(&(SDL_Point){mx, my}, &btn_settings_toggle) && !SDL_PointInRect(&(SDL_Point){mx, my}, &btn_view_friends)) {
        selected_player_id = -1;
    }
}

void handle_auth_click(int mx, int my) {
    // 1. Server List Logic
    if (show_server_list) {
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

    if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_open_servers)) {
        show_server_list = !show_server_list;
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
    
    // Field Selection
    int y_start = auth_box.y + 80;
    if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){auth_box.x+130, y_start-5, 200, 25})) { active_field = 2; SDL_StartTextInput(); }
    else if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){auth_box.x+130, y_start+35, 80, 25})) { active_field = 3; SDL_StartTextInput(); }
    else if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){auth_box.x+130, y_start+85, 200, 25})) { active_field = 0; SDL_StartTextInput(); }
    else if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){auth_box.x+130, y_start+135, 200, 25})) { active_field = 1; SDL_StartTextInput(); }
}

int main(int argc, char const *argv[]) {
    // 1. Initialization
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) return 1;
    if (TTF_Init() == -1) return 1;
    if (!(IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG))) printf("IMG Init Error: %s\n", IMG_GetError());

    font = TTF_OpenFont(FONT_PATH, FONT_SIZE);
    if (!font) { printf("Font missing: %s\n", FONT_PATH); return 1; }

    SDL_Window *window = SDL_CreateWindow("C MMO Client", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // Load Assets
    SDL_Surface *temp = IMG_Load(MAP_FILE); if (temp) { tex_map = SDL_CreateTextureFromSurface(renderer, temp); map_w=temp->w; map_h=temp->h; SDL_FreeSurface(temp); }
    temp = IMG_Load(PLAYER_FILE); if (temp) { tex_player = SDL_CreateTextureFromSurface(renderer, temp); SDL_FreeSurface(temp); }

    init_audio();
    load_servers(); // Load saved list
    sock = -1;      // Start Disconnected

    for(int i=0; i<MAX_CLIENTS; i++) { avatar_cache[i] = NULL; avatar_status[i] = 0; }

    SDL_StartTextInput();
    int running = 1; SDL_Event event;
    int key_up=0, key_down=0, key_left=0, key_right=0;
    
    // Cursor State Tracking
    int last_active_field = -1;
    int was_chat_open = 0;

    while (running) {
        frame_count++;
        Uint32 now_fps = SDL_GetTicks();
        if (now_fps > last_fps_check + 1000) { current_fps = frame_count; frame_count = 0; last_fps_check = now_fps; }
        
        if (client_state == STATE_GAME && music_count > 0 && !Mix_PlayingMusic()) play_next_track();

        // --- COORDINATE SCALING ---
        int screen_w, screen_h; SDL_GetRendererOutputSize(renderer, &screen_w, &screen_h);
        int win_w, win_h; SDL_GetWindowSize(window, &win_w, &win_h);
        float scale_x = (float)screen_w / win_w;
        float scale_y = (float)screen_h / win_h;

        // --- CURSOR RESET LOGIC ---
        // If field changed, move cursor to end of new text
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
            cursor_pos = len;
            last_active_field = active_field;
        }
        if (is_chat_open != was_chat_open) {
            if(is_chat_open) cursor_pos = strlen(input_buffer);
            was_chat_open = is_chat_open;
        }

        while (SDL_PollEvent(&event)) {
            // --- NEW: Auto-AFK Reset Logic ---
            if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEMOTION) {
                last_input_tick = SDL_GetTicks();
                
                // If we were auto-afk, wake up!
                if (is_auto_afk && sock > 0) {
                    Packet pkt; pkt.type = PACKET_STATUS_CHANGE; 
                    pkt.new_status = STATUS_ONLINE; // 0 = Online
                    send_packet(&pkt);
                    is_auto_afk = 0;
                    printf("Welcome back! Status set to Online.\n");
                }
            }
            // ---------------------------------
            if (event.type == SDL_QUIT) running = 0;
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
                // Click (Scaled)
                if(event.type == SDL_MOUSEBUTTONDOWN) {
                    int mx = event.button.x * scale_x; int my = event.button.y * scale_y;
                    handle_auth_click(mx, my);
                }
                
                // Text Input (Cursor Aware)
                char *target = NULL; int max = 31;
                if(active_field == 0) target = auth_username;
                else if(active_field == 1) target = auth_password;
                else if(active_field == 2) { target = input_ip; max = 63; }
                else if(active_field == 3) { target = input_port; max = 5; }
                
                if (target) handle_text_edit(target, max, &event);

                // Keys
                if(event.type == SDL_KEYDOWN) {
                    if(event.key.keysym.sym == SDLK_RETURN) handle_auth_click(btn_login.x+1, btn_login.y+1);
                    if(event.key.keysym.sym == SDLK_TAB) { active_field++; if(active_field > 3) active_field = 0; }
                }
            } else {
                // STATE_GAME
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                     int mx = event.button.x * scale_x; int my = event.button.y * scale_y;
                     
                        // HUD Logic
                        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_chat_toggle)) {
                        is_chat_open = !is_chat_open; 
                        
                        if(is_chat_open) {
                            unread_chat_count = 0; // Reset counter
                            SDL_StartTextInput();
                        } else {
                            SDL_StopTextInput();
                        }
                     }
                     else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_settings_toggle)) {
                        is_settings_open = !is_settings_open;
                        if (!is_settings_open) { show_nick_popup = 0; show_blocked_list = 0; active_field = -1; }
                     }
                     else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_view_friends)) {
                        show_friend_list = !show_friend_list;
                     }
                     else {
                        // Calc Camera & Pass to Handler
                        float px=0, py=0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].active && local_players[i].id == local_player_id) { px=local_players[i].x; py=local_players[i].y; }
                        int cam_x = (int)px - (screen_w/2) + 16; int cam_y = (int)py - (screen_h/2) + 16;
                        if (screen_w > map_w) cam_x = -(screen_w - map_w)/2; if (screen_h > map_h) cam_y = -(screen_h - map_h)/2;
                        handle_game_click(mx, my, cam_x, cam_y, screen_w, screen_h);
                     }
                }
                
                // --- GAME TEXT INPUTS (Cursor Aware) ---
                if (is_chat_open) {
                    handle_text_edit(input_buffer, 60, &event);
                    if(event.type == SDL_KEYDOWN) {
                        if(event.key.keysym.sym == SDLK_RETURN) {
                            if(strlen(input_buffer)>0) { 
                                if (chat_target_id != -1) {
                                    Packet pkt; pkt.type = PACKET_PRIVATE_MESSAGE; pkt.target_id = chat_target_id; strcpy(pkt.msg, input_buffer); send_packet(&pkt);
                                    chat_target_id = -1;
                                } else { Packet pkt; pkt.type = PACKET_CHAT; strcpy(pkt.msg, input_buffer); send_packet(&pkt); }
                                input_buffer[0]=0; cursor_pos = 0;
                            }
                            is_chat_open=0; SDL_StopTextInput();
                        }
                        else if(event.key.keysym.sym == SDLK_ESCAPE) { is_chat_open=0; chat_target_id = -1; SDL_StopTextInput(); }
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
                else {
                    // Movement (WASD)
                    if (event.type == SDL_KEYDOWN) {
                        if (event.key.keysym.sym == SDLK_w) key_up=1; if (event.key.keysym.sym == SDLK_s) key_down=1;
                        if (event.key.keysym.sym == SDLK_a) key_left=1; if (event.key.keysym.sym == SDLK_d) key_right=1;
                        if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_t) { is_chat_open=1; input_buffer[0]=0; cursor_pos=0; key_up=0; key_down=0; key_left=0; key_right=0; SDL_StartTextInput(); }
                    }
                    if (event.type == SDL_KEYUP) {
                        if (event.key.keysym.sym == SDLK_w) key_up=0; if (event.key.keysym.sym == SDLK_s) key_down=0;
                        if (event.key.keysym.sym == SDLK_a) key_left=0; if (event.key.keysym.sym == SDLK_d) key_right=0;
                    }
                }
            }
        }

        Uint32 now = SDL_GetTicks();
        // --- NEW: Check Auto-AFK Timer ---
        if (sock > 0 && client_state == STATE_GAME && !is_auto_afk) {
            // Find my current status
            int my_status = 0;
            for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == local_player_id) my_status = local_players[i].status;

            // Only trigger if currently Online (don't override DND or manual AFK)
            if (my_status == STATUS_ONLINE) { 
                if (now > last_input_tick + (afk_timeout_minutes * 60 * 1000)) {
                    Packet pkt; pkt.type = PACKET_STATUS_CHANGE; 
                    pkt.new_status = STATUS_AFK; // 1 = AFK
                    send_packet(&pkt);
                    is_auto_afk = 1;
                    printf("Auto-AFK Triggered.\n");
                }
            }
        }
        // ---------------------------------
        if (sock > 0 && client_state == STATE_GAME) {
            if (now - last_ping_sent > 1000) { Packet pkt; pkt.type = PACKET_PING; pkt.timestamp = now; send_packet(&pkt); last_ping_sent = now; }
            if (!is_chat_open && !show_nick_popup && !show_add_friend_popup && pending_friend_req_id == -1) {
                float dx = 0, dy = 0;
                if (key_up) dy = -1; if (key_down) dy = 1; if (key_left) dx = -1; if (key_right) dx = 1;
                if (dx != 0 || dy != 0) { Packet pkt; pkt.type = PACKET_MOVE; pkt.dx = dx; pkt.dy = dy; send_packet(&pkt); }
            }
        }

        if (sock > 0) {
            int bytes; 
            if (ioctl(sock, FIONREAD, &bytes) != -1 && bytes > 0) {
                Packet pkt; 
                if (recv(sock, &pkt, sizeof(Packet), MSG_WAITALL) > 0) {
                    if (pkt.type == PACKET_AUTH_RESPONSE) {
                        if (pkt.status == AUTH_SUCCESS) { client_state = STATE_GAME; local_player_id = pkt.player_id; SDL_StopTextInput(); if (music_count > 0) play_next_track(); }
                        else if (pkt.status == AUTH_REGISTER_SUCCESS) strcpy(auth_message, "Success! Login now.");
                        else strcpy(auth_message, "Error.");
                    }
                    if (pkt.type == PACKET_UPDATE) memcpy(local_players, pkt.players, sizeof(local_players));
                    if (pkt.type == PACKET_CHAT || pkt.type == PACKET_PRIVATE_MESSAGE) {
                        add_chat_message(&pkt);
                        
                        // Increment ONLY if it is a chat packet and chat is closed
                        if (!is_chat_open) unread_chat_count++;

                        // Also capture system messages for the popup here
                        if (pkt.player_id == -1 && show_add_friend_popup) {
                            strncpy(friend_popup_msg, pkt.msg, 127);
                        }
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
                }    
            }
        }

        if (client_state == STATE_AUTH) render_auth_screen(renderer); else render_game(renderer);
        SDL_Delay(16);
    }
    
    if(tex_map) SDL_DestroyTexture(tex_map); if(tex_player) SDL_DestroyTexture(tex_player);
    if(bgm) Mix_FreeMusic(bgm); Mix_CloseAudio();
    if(sock > 0) close(sock); 
    TTF_CloseFont(font); TTF_Quit(); IMG_Quit();
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}