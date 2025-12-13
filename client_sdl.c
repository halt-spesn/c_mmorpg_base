#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <sys/utsname.h> // For Kernel info
#include <GL/gl.h>       // For GPU info
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
char server_ip[16] = "127.0.0.1"; // Store IP for debug
TTF_Font *font = NULL;
SDL_Texture *tex_map = NULL;
SDL_Texture *tex_player = NULL;
int map_w = MAP_WIDTH, map_h = MAP_HEIGHT;

int local_player_id = -1;
Player local_players[MAX_CLIENTS];
int my_friends[20];
int friend_count = 0;

// UI STATE
int selected_player_id = -1;
int pending_friend_req_id = -1;
char pending_friend_name[32] = "";

SDL_Rect profile_win = {10, 10, 200, 80};
SDL_Rect btn_add_friend = {20, 50, 180, 30};
SDL_Rect popup_win;

// Settings & Debug State
int is_settings_open = 0;
int show_debug_info = 0;
int current_ping = 0;
Uint32 last_ping_sent = 0;

SDL_Rect btn_settings_toggle;
SDL_Rect settings_win;
SDL_Rect btn_toggle_debug;

// Chat & Auth
typedef struct { char msg[64]; Uint32 timestamp; } FloatingText;
FloatingText floating_texts[MAX_CLIENTS];
#define CHAT_HISTORY 10
char chat_log[CHAT_HISTORY][64];
int chat_log_count = 0;
int is_chat_open = 0;
char input_buffer[64] = "";

typedef enum { STATE_AUTH, STATE_GAME } ClientState;
ClientState client_state = STATE_AUTH;

#define MAX_INPUT_LEN 31
char auth_username[MAX_INPUT_LEN+1] = "";
char auth_password[MAX_INPUT_LEN+1] = "";
int active_field = 0;
char auth_message[128] = "Enter Credentials";

SDL_Rect auth_box, btn_login, btn_register, btn_chat_toggle;
SDL_Color col_white = {255,255,255,255};
SDL_Color col_red = {255,50,50,255};
SDL_Color col_yellow = {255,255,0,255};
SDL_Color col_cyan = {0,255,255,255};
SDL_Color col_green = {0,255,0,255}; 
SDL_Color col_btn = {100,100,100,255};
SDL_Color col_black = {0,0,0,255};

// --- Helpers ---
void send_packet(Packet *pkt) { send(sock, pkt, sizeof(Packet), 0); }

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

void add_chat_message(int player_id, char *msg) {
    for(int i=0; i<CHAT_HISTORY-1; i++) strcpy(chat_log[i], chat_log[i+1]);
    char *name = "System";
    if (player_id != -1) {
        name = "Unknown";
        for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == player_id) name = local_players[i].username;
    }
    snprintf(chat_log[CHAT_HISTORY-1], 64, "[%s]: %s", name, msg);
    if (player_id != -1) {
        for(int i=0; i<MAX_CLIENTS; i++) {
            if(local_players[i].id == player_id) {
                strncpy(floating_texts[i].msg, msg, 63);
                floating_texts[i].timestamp = SDL_GetTicks();
            }
        }
    }
}

// --- Debug Renderer (Dynamic Size) ---
void render_debug_overlay(SDL_Renderer *renderer, int screen_w) {
    if (!show_debug_info) return;

    // Buffer to hold our lines before rendering
    char lines[10][128];
    int line_count = 0;

    // 1. Prepare all strings
    snprintf(lines[line_count++], 128, "Ping: %d ms", current_ping);
    snprintf(lines[line_count++], 128, "Server IP: %s", server_ip);

    float px=0, py=0;
    for(int i=0; i<MAX_CLIENTS; i++) 
        if(local_players[i].active && local_players[i].id == local_player_id) 
        { px=local_players[i].x; py=local_players[i].y; }
    snprintf(lines[line_count++], 128, "Pos: %.1f, %.1f", px, py);

    // GPU (No truncation!)
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
    #elif defined(_MSC_VER)
        strcpy(compiler_name, "MSVC");
    #endif
    snprintf(lines[line_count++], 128, "Compiler: %s %s", compiler_name, __VERSION__);

    // 2. Measure Widths
    int max_w = 200; // Minimum width
    for(int i=0; i<line_count; i++) {
        int w, h;
        TTF_SizeText(font, lines[i], &w, &h);
        if (w > max_w) max_w = w;
    }

    // 3. Calculate Box Geometry
    int box_w = max_w + 20;       // Add padding
    int box_h = (line_count * 20) + 10;
    int start_x = screen_w - box_w - 10; // Anchor to right
    int start_y = 10;

    // Safety: If window is too narrow, don't go off-screen left
    if (start_x < 10) start_x = 10;

    // 4. Render
    SDL_Rect dbg_box = {start_x, start_y, box_w, box_h};
    
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    SDL_RenderFillRect(renderer, &dbg_box);

    int y = start_y + 5;
    for(int i=0; i<line_count; i++) {
        // Render text color based on content (Green for Ping)
        SDL_Color color = col_white;
        if (strncmp(lines[i], "Ping:", 5) == 0) color = col_green;

        render_text(renderer, lines[i], dbg_box.x + 10, y, color, 0);
        y += 20;
    }
}

// --- UI Rendering ---

void render_settings_menu(SDL_Renderer *renderer, int screen_w, int screen_h) {
    if (!is_settings_open) return;

    settings_win = (SDL_Rect){screen_w/2 - 150, screen_h/2 - 100, 300, 200};
    
    // Window Body
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255); SDL_RenderFillRect(renderer, &settings_win);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &settings_win);
    
    render_text(renderer, "Settings", settings_win.x + 150, settings_win.y + 10, col_white, 1);

    // Option 1: Debug Info
    btn_toggle_debug = (SDL_Rect){settings_win.x + 20, settings_win.y + 50, 20, 20};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &btn_toggle_debug);
    
    if (show_debug_info) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); 
        SDL_Rect check = {btn_toggle_debug.x + 4, btn_toggle_debug.y + 4, 12, 12};
        SDL_RenderFillRect(renderer, &check);
    }
    
    render_text(renderer, "Show Debug Info", settings_win.x + 50, settings_win.y + 50, col_white, 0);
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
    
    // Check if player still exists/visible
    int exists = 0; char *name = "Unknown";
    for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == selected_player_id) { exists=1; name=local_players[i].username; }
    if (!exists) { selected_player_id = -1; return; }

    SDL_SetRenderDrawColor(renderer, 30, 30, 50, 230); SDL_RenderFillRect(renderer, &profile_win);
    SDL_SetRenderDrawColor(renderer, 200, 200, 255, 255); SDL_RenderDrawRect(renderer, &profile_win);
    render_text(renderer, name, profile_win.x + 100, profile_win.y + 10, col_white, 1);

    int is_friend = 0; for(int i=0; i<friend_count; i++) if(my_friends[i] == selected_player_id) is_friend = 1;

    SDL_Rect btn = btn_add_friend;
    if (!is_friend) {
        SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255); SDL_RenderFillRect(renderer, &btn);
        render_text(renderer, "+ Add Friend", btn.x + 90, btn.y + 5, col_white, 1);
    } else {
        SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn);
        render_text(renderer, "- Remove Friend", btn.x + 90, btn.y + 5, col_white, 1);
    }
}

// ... (render_auth_screen is same) ...
void render_auth_screen(SDL_Renderer *renderer) {
    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
    auth_box = (SDL_Rect){w/2-200, h/2-150, 400, 300};
    btn_login = (SDL_Rect){auth_box.x+20, auth_box.y+200, 160, 40};
    btn_register = (SDL_Rect){auth_box.x+220, auth_box.y+200, 160, 40};
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
    char masked[MAX_INPUT_LEN+1]; memset(masked, '*', strlen(auth_password)); masked[strlen(auth_password)]=0;
    render_text(renderer, masked, pass_rect.x + 5, pass_rect.y + 2, col_white, 0);
    SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_login);
    render_text(renderer, "Login", btn_login.x + 80, btn_login.y + 10, col_white, 1);
    SDL_SetRenderDrawColor(renderer, 0, 0, 150, 255); SDL_RenderFillRect(renderer, &btn_register);
    render_text(renderer, "Register", btn_register.x + 80, btn_register.y + 10, col_white, 1);
    SDL_RenderPresent(renderer);
}

void render_game(SDL_Renderer *renderer) {
    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
    float px=0, py=0;
    for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].active && local_players[i].id == local_player_id) { px=local_players[i].x; py=local_players[i].y; }
    
    int cam_x = (int)px - (w/2) + 16;
    int cam_y = (int)py - (h/2) + 16;
    if (cam_x < 0) cam_x = 0; if (cam_y < 0) cam_y = 0;
    if (cam_x > map_w - w) cam_x = map_w - w; if (cam_y > map_h - h) cam_y = map_h - h;
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
            SDL_Rect dst = { (int)local_players[i].x - cam_x, (int)local_players[i].y - cam_y, PLAYER_WIDTH, PLAYER_HEIGHT };
            if (tex_player) SDL_RenderCopy(renderer, tex_player, NULL, &dst);
            else {
                if(local_players[i].id == local_player_id) SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
                else SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
                SDL_RenderFillRect(renderer, &dst);
            }
            SDL_Color name_col = col_yellow; 
            if (local_players[i].id == local_player_id) name_col = col_cyan; 
            else { for(int f=0; f<friend_count; f++) if (my_friends[f] == local_players[i].id) name_col = col_green; }
            
            if (local_players[i].id == selected_player_id) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDrawRect(renderer, &dst);
            }

            render_text(renderer, local_players[i].username, dst.x+16, dst.y - 18, name_col, 1);
            if (now - floating_texts[i].timestamp < 4000) render_text(renderer, floating_texts[i].msg, dst.x+16, dst.y - 38, col_white, 1);
        }
    }

    render_profile(renderer);
    render_popup(renderer, w, h);

    // Chat Toggle
    btn_chat_toggle = (SDL_Rect){10, h-40, 100, 30};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_chat_toggle);
    render_text(renderer, is_chat_open ? "Close" : "Chat", btn_chat_toggle.x+50, btn_chat_toggle.y+5, col_white, 1);

    // Settings Toggle (Next to Chat)
    btn_settings_toggle = (SDL_Rect){120, h-40, 100, 30};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_settings_toggle);
    render_text(renderer, "Settings", btn_settings_toggle.x+50, btn_settings_toggle.y+5, col_white, 1);

    if(is_chat_open) {
        SDL_Rect win = {10, h-240, 300, 190};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0,0,0,200); SDL_RenderFillRect(renderer, &win);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        for(int i=0; i<CHAT_HISTORY; i++) render_text(renderer, chat_log[i], 15, win.y+10+(i*15), col_white, 0);
        char buf[80]; snprintf(buf, 80, "> %s_", input_buffer);
        render_text(renderer, buf, 15, win.y+win.h-20, col_cyan, 0);
    }

    render_settings_menu(renderer, w, h);
    render_debug_overlay(renderer, w);
    
    SDL_RenderPresent(renderer);
}

void handle_game_click(int mx, int my, int cam_x, int cam_y) {
    if (is_settings_open) {
        // Toggle Debug
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_toggle_debug)) {
            show_debug_info = !show_debug_info;
        }
        // Click outside closes settings? Or a Close button. 
        // For now, toggle button closes it.
        return;
    }

    if (pending_friend_req_id != -1) {
        SDL_Rect btn_accept = {popup_win.x+20, popup_win.y+70, 120, 30};
        SDL_Rect btn_deny = {popup_win.x+160, popup_win.y+70, 120, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_accept)) {
            Packet pkt; pkt.type = PACKET_FRIEND_RESPONSE; pkt.target_id = pending_friend_req_id; pkt.response_accepted = 1; send_packet(&pkt);
            pending_friend_req_id = -1;
        } else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_deny)) {
            pending_friend_req_id = -1;
        }
        return;
    }

    if (selected_player_id != -1 && selected_player_id != local_player_id) {
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_add_friend)) {
            int is_friend = 0; for(int i=0; i<friend_count; i++) if(my_friends[i] == selected_player_id) is_friend = 1;
            if(!is_friend) { Packet pkt; pkt.type = PACKET_FRIEND_REQUEST; pkt.target_id = selected_player_id; send_packet(&pkt); } 
            else { Packet pkt; pkt.type = PACKET_FRIEND_REMOVE; pkt.target_id = selected_player_id; send_packet(&pkt); }
            selected_player_id = -1; 
            return;
        }
    }

    int clicked = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (local_players[i].active) {
            SDL_Rect r = { (int)local_players[i].x - cam_x, (int)local_players[i].y - cam_y, PLAYER_WIDTH, PLAYER_HEIGHT };
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &r)) {
                selected_player_id = local_players[i].id;
                clicked = 1;
            }
        }
    }
    if (!clicked && !SDL_PointInRect(&(SDL_Point){mx, my}, &profile_win) && !SDL_PointInRect(&(SDL_Point){mx, my}, &btn_chat_toggle)) {
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
    inet_pton(AF_INET, server_ip, &serv_addr.sin_addr); // Use the variable
    connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    SDL_StartTextInput();
    int running = 1; SDL_Event event;
    int key_up=0, key_down=0, key_left=0, key_right=0;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
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
                // Game Inputs
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                     if (SDL_PointInRect(&(SDL_Point){event.button.x, event.button.y}, &btn_chat_toggle)) {
                        is_chat_open = !is_chat_open; if(is_chat_open) SDL_StartTextInput(); else SDL_StopTextInput();
                     } 
                     else if (SDL_PointInRect(&(SDL_Point){event.button.x, event.button.y}, &btn_settings_toggle)) {
                        is_settings_open = !is_settings_open;
                     }
                     else {
                        // Calculate Camera
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
                            if(strlen(input_buffer)>0) { Packet pkt; pkt.type = PACKET_CHAT; strcpy(pkt.msg, input_buffer); send_packet(&pkt); input_buffer[0]=0; }
                            is_chat_open=0; SDL_StopTextInput();
                        }
                        else if(event.key.keysym.sym == SDLK_BACKSPACE && strlen(input_buffer)>0) input_buffer[strlen(input_buffer)-1]=0;
                        else if(event.key.keysym.sym == SDLK_ESCAPE) { is_chat_open=0; SDL_StopTextInput(); }
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

        // PING LOGIC (Every 1 second)
        Uint32 now = SDL_GetTicks();
        if (client_state == STATE_GAME && now - last_ping_sent > 1000) {
            Packet pkt; pkt.type = PACKET_PING; pkt.timestamp = now;
            send_packet(&pkt);
            last_ping_sent = now;
        }

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
                if (pkt.type == PACKET_CHAT) add_chat_message(pkt.player_id, pkt.msg);
                if (pkt.type == PACKET_FRIEND_LIST) {
                    friend_count = pkt.friend_count;
                    for(int i=0; i<friend_count; i++) my_friends[i] = pkt.friend_ids[i];
                }
                if (pkt.type == PACKET_FRIEND_INCOMING) {
                    pending_friend_req_id = pkt.player_id;
                    strcpy(pending_friend_name, pkt.username);
                }
                if (pkt.type == PACKET_PING) {
                    current_ping = SDL_GetTicks() - pkt.timestamp;
                }
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