#ifdef _WIN32
    #pragma comment(lib, "comdlg32")
#endif

#if defined(__APPLE__) && !defined(__IPHONEOS__)
// Objective-C runtime for macOS window manipulation
#include <objc/runtime.h>
#include <objc/message.h>
#include <CoreFoundation/CoreFoundation.h>
// Define NSInteger for macOS Objective-C runtime calls
#if defined(__LP64__) && __LP64__
typedef long NSInteger;
#else
typedef int NSInteger;
#endif
// NSWindow constants
#define NSWindowTitleVisible 0
#define NSWindowTabbingModeDisallowed 2
#endif

#ifdef __ANDROID__
#include <jni.h>
#include <SDL2/SDL_system.h>
#endif

#include "client.h"

// --- Global Variable Definitions ---
char current_map_file[32] = "map.jpg";
RenderBackend render_backend = RENDER_BACKEND_OPENGL;
#ifdef USE_VULKAN
VulkanRenderer vk_renderer;
int use_vulkan = 0;
int backend_set_by_cmdline = 0;
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
FriendInfo my_friends[20];
int friend_count = 0;

// NPCs
NPC client_npcs[50];
int client_npc_count = 0;
#define NPC_INTERACTION_RADIUS 64  // Interaction range in pixels

// Player stats
int my_gold = 100;
int my_xp = 0;
int my_level = 1;

int blocked_ids[50];
int blocked_count = 0;
int show_blocked_list = 0;
int blocked_scroll = 0;

SDL_Rect btn_hide_player;
SDL_Rect btn_view_blocked;
SDL_Rect blocked_win;
SDL_Rect btn_close_blocked;
SDL_Rect btn_hide_player_dyn;
SDL_Rect btn_change_avatar;
SDL_Rect btn_change_password;

int selected_player_id = -1;
int selected_npc_id = -1;
int pending_friend_req_id = -1;
char pending_friend_name[32] = "";

// Dialogue system
int show_dialogue = 0;
char dialogue_text[256] = "";
char dialogue_npc_name[32] = "";
SDL_Rect dialogue_window;

// Shop system
int show_shop = 0;
int shop_tab = 0; // 0=buy, 1=sell
ShopData current_shop;
int shop_scroll = 0;
SDL_Rect shop_window;

// Quest system
int show_quest_log = 0;
Quest my_quests[10];
int my_quest_count = 0;
Quest available_quests[10];  // Quests offered by current NPC
int available_quest_count = 0;
int show_quest_offer = 0;
int quest_scroll = 0;
int quest_offer_scroll = 0;
SDL_Rect quest_window;
SDL_Rect btn_toggle_quest_log;

SDL_Rect profile_win = {10, 10, 200, 130};
SDL_Rect btn_add_friend = {20, 50, 180, 30};
SDL_Rect btn_send_pm = {20, 90, 180, 30};
SDL_Rect popup_win;

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

FloatingText floating_texts[MAX_CLIENTS];
char chat_log[CHAT_HISTORY][64];
int chat_log_count = 0;
int chat_scroll = 0;
int is_chat_open = 0;
int chat_target_id = -1;
int chat_input_active = 0;
char input_buffer[64] = "";
int is_typing = 0;  // Local typing status
Uint32 last_typing_update = 0;  // Last time we sent typing status

ClientState client_state = STATE_AUTH;

char auth_username[MAX_INPUT_LEN+1] = "";
char auth_password[MAX_INPUT_LEN+1] = "";
int active_field = -1;
int show_password = 0;
char auth_message[128];

SDL_Rect auth_box, btn_login, btn_register, btn_chat_toggle, btn_show_pass;
SDL_Color col_white = {255,255,255,255};
SDL_Color col_red = {255,50,50,255};
SDL_Color col_yellow = {255,255,0,255};
SDL_Color col_cyan = {0,255,255,255};
SDL_Color col_green = {0,255,0,255};
SDL_Color col_btn = {100,100,100,255};
SDL_Color col_black = {0,0,0,255};
SDL_Color col_magenta = {255,0,255,255};

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
int friend_list_scroll = 0;

SDL_Rect slider_r, slider_g, slider_b;

SDL_Texture* avatar_cache[MAX_CLIENTS];
int avatar_status[MAX_CLIENTS];

char music_playlist[20][64];
int music_count = 0;
int current_track = -1;
Mix_Music *bgm = NULL;
int music_volume = 64;
SDL_Rect slider_volume;

uint8_t temp_avatar_buf[MAX_AVATAR_SIZE];

// Global scale factors for mouse coordinate conversion
float scale_x = 1.0f;
float scale_y = 1.0f;

char input_ip[64] = "127.0.0.1";
char input_port[16] = "8888";
int is_connected = 0;

ServerEntry server_list[10];
int server_count = 0;
int show_server_list = 0;
SDL_Rect server_list_win;
SDL_Rect btn_open_servers;
SDL_Rect btn_add_server;
SDL_Rect btn_close_servers;

ProfileEntry profile_list[10];
int profile_count = 0;
int show_profile_list = 0;
SDL_Rect profile_list_win;
SDL_Rect btn_open_profiles;
SDL_Rect btn_add_profile;
SDL_Rect btn_close_profiles;

int show_nick_popup = 0;
char nick_new[32] = "";
char nick_confirm[32] = "";
char nick_pass[32] = "";

int show_password_popup = 0;
char password_current[64] = "";
char password_new[64] = "";
char password_confirm[64] = "";
char password_message[128] = "";
int show_password_current = 0;
int show_password_new = 0;
int show_password_confirm = 0;

IncomingReq inbox[10];
int inbox_count = 0;
int is_inbox_open = 0;
SDL_Rect btn_inbox;
int inbox_scroll = 0;

int show_add_friend_popup = 0;
char input_friend_id[10] = "";
char friend_popup_msg[128] = "";

SDL_Rect btn_friend_add_id_rect;
SDL_Rect friend_win_rect;
SDL_Rect btn_friend_close_rect;

SDL_Rect blocked_win_rect;
SDL_Rect btn_blocked_close_rect;
SDL_Rect slider_afk;
SDL_Rect btn_disconnect_rect;
SDL_Rect btn_inbox_rect;

int cursor_pos = 0;

int unread_chat_count = 0;
int show_unread_counter = 1;
SDL_Rect btn_toggle_unread;

char gl_renderer_cache[128] = "";
char gl_vendor_cache[128] = "";
int gl_probe_done = 0;

// Debug overlay caching to avoid expensive text rendering every frame
SDL_Texture *debug_text_textures[14];
SDL_Rect debug_text_rects[14];
int debug_text_line_count = 0;
Uint32 debug_cache_last_update = 0;
int debug_box_w = 0, debug_box_h = 0;
int debug_box_x = 0;

#ifdef USE_VULKAN
char vk_device_name[128] = "";
int vk_probe_done = 0;
#endif

Uint32 last_input_tick = 0;
int afk_timeout_minutes = 2;
Uint32 last_color_packet_ms = 0;
int is_auto_afk = 0;

int settings_scroll_y = 0;
int settings_content_h = 0;
SDL_Rect settings_view_port;

SDL_Rect slider_r2, slider_g2, slider_b2;
int my_r2 = 255, my_g2 = 255, my_b2 = 255;

int saved_r = 255, saved_g = 255, saved_b = 255;

int selection_start = 0;
int selection_len = 0;
int is_dragging = 0;
SDL_Rect active_input_rect;

int show_nick_pass = 0;
SDL_Rect btn_show_nick_pass;

int show_contributors = 0;

float ui_scale = 1.0f;
float pending_ui_scale = 1.0f;
SDL_Rect slider_ui_scale;

float game_zoom = 1.0f;
float pending_game_zoom = 1.0f;
SDL_Rect slider_game_zoom;

int config_use_vulkan = 0;
int config_use_nvidia_gpu = 0;
SDL_Rect btn_toggle_vulkan;
SDL_Rect btn_toggle_nvidia_gpu;
SDL_Rect btn_cycle_language;

#if defined(__ANDROID__) || defined(__IPHONEOS__)
int keyboard_height = 0;
int chat_window_shift = 0;
int show_mobile_text_menu = 0;
SDL_Rect mobile_text_menu_rect;
int mobile_text_menu_x = 0;
int mobile_text_menu_y = 0;
Uint32 long_press_start_time = 0;
int long_press_active = 0;
int long_press_start_x = 0;
int long_press_start_y = 0;
#define CHAT_MIN_Y_POSITION 50  // Minimum Y position for chat window when shifted up
#define CHAT_DEFAULT_Y_OFFSET 240  // Default offset from bottom of screen for chat window
#endif

int show_documentation = 0;
SDL_Rect btn_contributors_rect;
SDL_Rect btn_documentation_rect;

SDL_Texture *cached_contributors_tex = NULL;
SDL_Texture *cached_documentation_tex = NULL;
SDL_Texture *cached_role_list_tex = NULL;
int cached_staff_count = -1; // Track when to invalidate cache
int contributors_tex_w = 0, contributors_tex_h = 0;
int documentation_tex_w = 0, documentation_tex_h = 0;
int contributors_scroll = 0;
int documentation_scroll = 0;

int show_role_list = 0;
int role_list_scroll = 0;
StaffEntry staff_list[50];
int staff_count = 0;
SDL_Rect btn_staff_list_rect;

int show_sanction_popup = 0;
int sanction_target_id = -1;
int sanction_mode = 0;
char input_sanction_reason[64] = "";
char input_ban_time[16] = "";

int show_my_warnings = 0;
WarningEntry my_warning_list[20];
int my_warning_count = 0;
int warnings_scroll = 0;

SDL_Rect btn_sanction_open;
SDL_Rect btn_my_warnings;

int active_slider = SLIDER_NONE;

// Track scroll gesture vs tap
int scroll_start_x = 0;
int scroll_start_y = 0;
int scroll_moved = 0;  // Flag to track if finger actually moved during scroll

// Track overlay touch for proper click handling
SDL_FingerID overlay_touch_id = -1;
int overlay_touch_start_x = 0;
int overlay_touch_start_y = 0;

int my_r = 255, my_g = 255, my_b = 255;

MapTrigger triggers[20];
int trigger_count = 0;
Uint32 last_map_switch_time = 0;
Uint32 last_move_time = 0;

// Inventory System
InventorySlot my_inventory[MAX_INVENTORY_SLOTS];
Item my_equipment[EQUIP_SLOT_COUNT];
int my_inventory_count = 0;
int show_inventory = 0;
SDL_Rect inventory_win = {0, 0, 400, 500};
SDL_Rect btn_inventory_toggle = {0, 0, 80, 30};
int inventory_scroll = 0;
int selected_inv_slot = -1;
int dragging_item_slot = -1;

// Equipment slot rectangles for click detection
SDL_Rect btn_equip_weapon = {0, 0, 0, 0};
SDL_Rect btn_equip_helmet = {0, 0, 0, 0};
SDL_Rect btn_equip_chest = {0, 0, 0, 0};
SDL_Rect btn_equip_legs = {0, 0, 0, 0};
SDL_Rect btn_equip_boots = {0, 0, 0, 0};
SDL_Rect btn_equip_accessory = {0, 0, 0, 0};

// Tooltip for items
char tooltip_text[64] = "";
int tooltip_x = 0;
int tooltip_y = 0;
int show_tooltip = 0;

// Ground items
GroundItem client_ground_items[MAX_GROUND_ITEMS];
int client_ground_item_count = 0;

SDL_Rect dpad_rect = {20, 0, 150, 150};
float vjoy_dx = 0, vjoy_dy = 0;
SDL_FingerID touch_id_dpad = -1;
SDL_FingerID scroll_touch_id = -1;
int scroll_window_id = -1; // Track which window is being scrolled: 0=settings, 1=docs, 2=contrib, etc.
int scroll_last_y = 0;
int joystick_active = 0;

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
        snprintf(buf, 64, get_string(STR_FPS), current_fps);
        int w, h; TTF_SizeText(font, buf, &w, &h); SDL_Rect bg = {10, y, w+4, h};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150); SDL_RenderFillRect(renderer, &bg);
        render_text(renderer, buf, 12, y, col_green, 0); y += 20;
    }
    if (show_coords) {
        float px=0, py=0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].active && local_players[i].id == local_player_id) { px=local_players[i].x; py=local_players[i].y; }
        snprintf(buf, 64, "XY: %.0f, %.0f", px, py);
        int w, h; TTF_SizeText(font, buf, &w, &h); SDL_Rect bg = {10, y, w+4, h};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150); SDL_RenderFillRect(renderer, &bg);
        render_text(renderer, buf, 12, y, col_white, 0); y += 20;
    }
    
    // Player stats (Gold, Level, XP) - Top left
    snprintf(buf, 64, "Gold: %d", my_gold);
    int w, h; TTF_SizeText(font, buf, &w, &h); 
    SDL_Rect gold_bg = {10, y, w+8, h+4};
    SDL_SetRenderDrawColor(renderer, 40, 30, 0, 200); SDL_RenderFillRect(renderer, &gold_bg);
    SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255); SDL_RenderDrawRect(renderer, &gold_bg);
    render_text(renderer, buf, 14, y+2, (SDL_Color){255, 215, 0, 255}, 0); 
    y += h + 6;
    
    snprintf(buf, 64, "Level: %d", my_level);
    TTF_SizeText(font, buf, &w, &h); 
    SDL_Rect lvl_bg = {10, y, w+8, h+4};
    SDL_SetRenderDrawColor(renderer, 20, 20, 40, 200); SDL_RenderFillRect(renderer, &lvl_bg);
    SDL_SetRenderDrawColor(renderer, 100, 200, 255, 255); SDL_RenderDrawRect(renderer, &lvl_bg);
    render_text(renderer, buf, 14, y+2, (SDL_Color){100, 200, 255, 255}, 0); 
    y += h + 6;
    
    snprintf(buf, 64, "XP: %d", my_xp);
    TTF_SizeText(font, buf, &w, &h); 
    SDL_Rect xp_bg = {10, y, w+8, h+4};
    SDL_SetRenderDrawColor(renderer, 20, 40, 20, 200); SDL_RenderFillRect(renderer, &xp_bg);
    SDL_SetRenderDrawColor(renderer, 100, 255, 100, 255); SDL_RenderDrawRect(renderer, &xp_bg);
    render_text(renderer, buf, 14, y+2, (SDL_Color){100, 255, 100, 255}, 0);

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
    
    // INVENTORY BUTTON (Next to inbox)
    btn_inventory_toggle = (SDL_Rect){screen_w - 100, 10, 40, 40};
    SDL_SetRenderDrawColor(renderer, 60, 50, 30, 255); SDL_RenderFillRect(renderer, &btn_inventory_toggle);
    SDL_SetRenderDrawColor(renderer, 200, 150, 0, 255); SDL_RenderDrawRect(renderer, &btn_inventory_toggle);
    render_text(renderer, "[I]", btn_inventory_toggle.x+8, btn_inventory_toggle.y+10, col_yellow, 0);
    
    // QUEST LOG BUTTON (Next to inventory)
    btn_toggle_quest_log = (SDL_Rect){screen_w - 150, 10, 40, 40};
    SDL_SetRenderDrawColor(renderer, 60, 50, 60, 255); SDL_RenderFillRect(renderer, &btn_toggle_quest_log);
    SDL_SetRenderDrawColor(renderer, 200, 150, 255, 255); SDL_RenderDrawRect(renderer, &btn_toggle_quest_log);
    render_text(renderer, "[Q]", btn_toggle_quest_log.x+8, btn_toggle_quest_log.y+10, (SDL_Color){200, 150, 255, 255}, 0);
    
    // Quest count badge
    if (my_quest_count > 0) {
        SDL_Rect badge = {btn_toggle_quest_log.x + 25, btn_toggle_quest_log.y - 5, 20, 20};
        SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255); SDL_RenderFillRect(renderer, &badge);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDrawRect(renderer, &badge);
        char num[4]; sprintf(num, "%d", my_quest_count);
        render_text(renderer, num, badge.x+6, badge.y+2, col_white, 0);
    }
}

void render_inbox(SDL_Renderer *renderer, int w, int h) {
    if (!is_inbox_open) return;
    
    SDL_Rect win = {w - 320, 60, 300, 300};
    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255); SDL_RenderFillRect(renderer, &win);
    SDL_SetRenderDrawColor(renderer, 100, 100, 255, 255); SDL_RenderDrawRect(renderer, &win);
    render_text(renderer, get_string(STR_PENDING_REQUESTS), win.x + 80, win.y + 10, col_yellow, 0);

    if (inbox_count == 0) {
        render_text(renderer, get_string(STR_NO_NEW_REQUESTS), win.x + 80, win.y + 40, col_white, 0);
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
            render_text(renderer, get_string(STR_YES), btn_acc.x+10, btn_acc.y+2, col_white, 0);

            // Deny
            SDL_Rect btn_deny = {row.x+220, row.y+25, 50, 20};
            SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_deny);
            render_text(renderer, get_string(STR_NO), btn_deny.x+15, btn_deny.y+2, col_white, 0);
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
    
    render_text(renderer, get_string(STR_ADD_FRIEND_BY_ID), pop.x+80, pop.y+10, col_green, 0);
    
    // --- NEW: Status Message ---
    // If message starts with "Error", render in Red, otherwise Yellow
    SDL_Color status_col = (strncmp(friend_popup_msg, "Error", 5) == 0) ? col_red : col_yellow;
    render_text(renderer, friend_popup_msg, pop.x+150, pop.y+35, status_col, 1); // 1 = Centered
    // ---------------------------
    
    SDL_Rect input_rect = {pop.x+50, pop.y+60, 200, 30};
    render_input_with_cursor(renderer, input_rect, input_friend_id, active_field == 25, 0);

    SDL_Rect btn_ok = {pop.x+50, pop.y+130, 80, 30};
    SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_ok);
    render_text(renderer, get_string(STR_ADD), btn_ok.x+25, btn_ok.y+5, col_white, 0);

    SDL_Rect btn_cancel = {pop.x+170, pop.y+130, 80, 30};
    SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_cancel);
    render_text(renderer, get_string(STR_CANCEL), btn_cancel.x+15, btn_cancel.y+5, col_white, 0);
}


void render_sanction_popup(SDL_Renderer *renderer, int w, int h) {
    if (!show_sanction_popup) return;

    SDL_Rect win = {w/2 - 150, h/2 - 150, 300, 300};
    SDL_SetRenderDrawColor(renderer, 40, 0, 0, 255); SDL_RenderFillRect(renderer, &win);
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); SDL_RenderDrawRect(renderer, &win);

    render_text(renderer, get_string(STR_SANCTION_PLAYER), win.x + 150, win.y + 10, col_red, 1);

    // Mode Toggle (Warn / Ban)
    SDL_Rect btn_warn = {win.x + 20, win.y + 40, 120, 30};
    SDL_Rect btn_ban = {win.x + 160, win.y + 40, 120, 30};
    
    SDL_SetRenderDrawColor(renderer, sanction_mode==0?200:50, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_warn);
    render_text(renderer, get_string(STR_WARN), btn_warn.x + 60, btn_warn.y + 5, col_white, 1);
    
    SDL_SetRenderDrawColor(renderer, sanction_mode==1?200:50, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_ban);
    render_text(renderer, get_string(STR_BAN), btn_ban.x + 60, btn_ban.y + 5, col_white, 1);

    int y = win.y + 90;
    
    // Reason Input
    render_text(renderer, get_string(STR_REASON), win.x + 20, y, col_white, 0);
    render_input_with_cursor(renderer, (SDL_Rect){win.x + 20, y+25, 260, 30}, input_sanction_reason, active_field == 30, 0);
    y += 70;

    // Time Input (Ban Only)
    if (sanction_mode == 1) {
        render_text(renderer, get_string(STR_TIME_FORMAT), win.x + 20, y, col_white, 0);
        render_input_with_cursor(renderer, (SDL_Rect){win.x + 20, y+25, 100, 30}, input_ban_time, active_field == 31, 0);
    } else {
        render_text(renderer, get_string(STR_THREE_WARNS_AUTO_BAN), win.x + 150, y+20, col_yellow, 1);
    }

    // Submit / Cancel
    SDL_Rect btn_submit = {win.x + 20, win.y + 240, 120, 30};
    SDL_Rect btn_cancel = {win.x + 160, win.y + 240, 120, 30};
    
    SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_submit);
    render_text(renderer, get_string(STR_EXECUTE), btn_submit.x + 60, btn_submit.y + 5, col_white, 1);
    
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_cancel);
    render_text(renderer, get_string(STR_CANCEL), btn_cancel.x + 60, btn_cancel.y + 5, col_white, 1);
}

void render_my_warnings(SDL_Renderer *renderer, int w, int h) {
    if (!show_my_warnings) return;

    SDL_Rect win = {w/2 - 200, h/2 - 200, 400, 400};
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); SDL_RenderFillRect(renderer, &win);
    SDL_SetRenderDrawColor(renderer, 255, 165, 0, 255); SDL_RenderDrawRect(renderer, &win);

    render_text(renderer, get_string(STR_MY_WARNINGS), win.x + 200, win.y + 10, col_yellow, 1);
    
    SDL_Rect btn_close = {win.x + 360, win.y + 5, 30, 30};
    SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_close);
    render_text(renderer, get_string(STR_X_CLOSE), btn_close.x + 10, btn_close.y + 5, col_white, 0);

    if (my_warning_count == 0) {
        render_text(renderer, get_string(STR_NO_WARNINGS), win.x + 200, win.y + 70, col_green, 1);
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

void render_inventory_slot(SDL_Renderer *renderer, InventorySlot *slot, SDL_Rect rect) {
    // Draw slot background
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawRect(renderer, &rect);
    
    if (slot && slot->item.item_id > 0) {
        // Draw item icon placeholder (colored square based on item type)
        SDL_Rect icon_rect = {rect.x + 5, rect.y + 5, rect.w - 10, rect.h - 25};
        
        // Color by item type
        switch (slot->item.type) {
            case ITEM_TYPE_CONSUMABLE: // Health potions, etc.
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                break;
            case ITEM_TYPE_WEAPON:
                SDL_SetRenderDrawColor(renderer, 200, 200, 0, 255);
                break;
            case ITEM_TYPE_ARMOR:
                SDL_SetRenderDrawColor(renderer, 100, 100, 200, 255);
                break;
            case ITEM_TYPE_ACCESSORY:
                SDL_SetRenderDrawColor(renderer, 200, 0, 200, 255);
                break;
            case ITEM_TYPE_MATERIAL:
                SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
                break;
            default:
                SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255);
        }
        SDL_RenderFillRect(renderer, &icon_rect);
        
        // Draw item name (first 3 chars)
        char short_name[4];
        strncpy(short_name, slot->item.name, 3);
        short_name[3] = '\0';
        render_text(renderer, short_name, icon_rect.x + icon_rect.w/2, icon_rect.y + 5, col_white, 1);
        
        // Draw quantity if > 1
        if (slot->item.quantity > 1) {
            char qty[8];
            snprintf(qty, 8, "x%d", slot->item.quantity);
            render_text(renderer, qty, rect.x + rect.w - 5, rect.y + rect.h - 15, col_white, 2);
        }
    }
}

void render_equipment_slot(SDL_Renderer *renderer, int slot, SDL_Rect rect) {
    // Draw slot background
    SDL_SetRenderDrawColor(renderer, 30, 30, 50, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, 150, 150, 0, 255);
    SDL_RenderDrawRect(renderer, &rect);
    
    // Draw slot label
    const char *labels[] = {"WPN", "HLM", "CHT", "LEG", "BOT", "ACC"};
    render_text(renderer, labels[slot], rect.x + rect.w/2, rect.y + 2, col_yellow, 1);
    
    if (my_equipment[slot].item_id > 0) {
        // Draw equipped item
        SDL_Rect icon_rect = {rect.x + 10, rect.y + 20, rect.w - 20, rect.h - 40};
        
        // Color by item type
        switch (my_equipment[slot].type) {
            case ITEM_TYPE_WEAPON:
                SDL_SetRenderDrawColor(renderer, 200, 200, 0, 255);
                break;
            case ITEM_TYPE_ARMOR:
                SDL_SetRenderDrawColor(renderer, 100, 100, 200, 255);
                break;
            case ITEM_TYPE_ACCESSORY:
                SDL_SetRenderDrawColor(renderer, 200, 0, 200, 255);
                break;
            default:
                SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255);
        }
        SDL_RenderFillRect(renderer, &icon_rect);
        
        // Draw item name (abbreviated)
        char short_name[4];
        strncpy(short_name, my_equipment[slot].name, 3);
        short_name[3] = '\0';
        render_text(renderer, short_name, icon_rect.x + icon_rect.w/2, icon_rect.y + 5, col_white, 1);
    }
}

void render_tooltip(SDL_Renderer *renderer) {
    if (!show_tooltip || strlen(tooltip_text) == 0) return;
    
    // Measure text size for background box
    int text_w = strlen(tooltip_text) * 7;
    int text_h = 20;
    int padding = 8;
    int box_w = text_w + padding * 2;
    int box_h = text_h + padding * 2;
    
    // Draw background
    SDL_Rect bg = {tooltip_x, tooltip_y, box_w, box_h};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 220);
    SDL_RenderFillRect(renderer, &bg);
    
    // Draw border
    SDL_SetRenderDrawColor(renderer, 200, 200, 0, 255);
    SDL_RenderDrawRect(renderer, &bg);
    
    // Draw text
    render_text(renderer, tooltip_text, tooltip_x + padding, tooltip_y + padding, col_white, 0);
}

void render_inventory(SDL_Renderer *renderer, int w, int h) {
    if (!show_inventory) return;

    // Get mouse position
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    // Convert mouse coordinates to UI space
    mx = (int)((mx * scale_x) / ui_scale);
    my = (int)((my * scale_y) / ui_scale);
    
    // Reset tooltip only if inventory is open (don't interfere with ground item tooltips)
    show_tooltip = 0;

    // Center the inventory window
    inventory_win.w = 450;
    inventory_win.h = 550;
    inventory_win.x = w/2 - inventory_win.w/2;
    inventory_win.y = h/2 - inventory_win.h/2;
    
    // Draw window background
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderFillRect(renderer, &inventory_win);
    SDL_SetRenderDrawColor(renderer, 200, 150, 0, 255);
    SDL_RenderDrawRect(renderer, &inventory_win);
    
    // Title
    render_text(renderer, get_string(STR_INVENTORY), inventory_win.x + inventory_win.w/2, inventory_win.y + 10, col_yellow, 1);
    
    // Close button
    SDL_Rect btn_close = {inventory_win.x + inventory_win.w - 35, inventory_win.y + 5, 30, 30};
    SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255);
    SDL_RenderFillRect(renderer, &btn_close);
    render_text(renderer, "X", btn_close.x + 8, btn_close.y + 5, col_white, 0);
    
    // Equipment slots (left side)
    int equip_x = inventory_win.x + 10;
    int equip_y = inventory_win.y + 50;
    int equip_slot_w = 80;
    int equip_slot_h = 70;
    int equip_spacing = 10;
    
    render_text(renderer, get_string(STR_EQUIPMENT), equip_x + 40, equip_y - 20, col_cyan, 1);
    
    for (int i = 0; i < EQUIP_SLOT_COUNT; i++) {
        SDL_Rect equip_rect = {
            equip_x,
            equip_y + i * (equip_slot_h + equip_spacing),
            equip_slot_w,
            equip_slot_h
        };
        render_equipment_slot(renderer, i, equip_rect);
        
        // Check for mouse hover to show tooltip
        SDL_Point mp = {mx, my};
        if (SDL_PointInRect(&mp, &equip_rect) && my_equipment[i].item_id > 0) {
            strncpy(tooltip_text, my_equipment[i].name, sizeof(tooltip_text) - 1);
            tooltip_x = mx + 10;
            tooltip_y = my - 30;
            show_tooltip = 1;
        }
        
        // Store equipment slot rects for click detection
        if (i == 0) btn_equip_weapon = equip_rect;
        else if (i == 1) btn_equip_helmet = equip_rect;
        else if (i == 2) btn_equip_chest = equip_rect;
        else if (i == 3) btn_equip_legs = equip_rect;
        else if (i == 4) btn_equip_boots = equip_rect;
        else if (i == 5) btn_equip_accessory = equip_rect;
    }
    
    // Inventory grid (right side)
    int inv_x = inventory_win.x + 110;
    int inv_y = inventory_win.y + 50;
    int slot_size = 60;
    int slots_per_row = 5;
    int spacing = 5;
    
    render_text(renderer, get_string(STR_ITEMS), inv_x + 150, inv_y - 20, col_cyan, 1);
    
    // Set up clipping for scrollable inventory area
    SDL_Rect clip_rect = {inv_x, inv_y, slots_per_row * (slot_size + spacing), inventory_win.h - 100};
    SDL_RenderSetClipRect(renderer, &clip_rect);
    
    int y_offset = -inventory_scroll;
    for (int i = 0; i < MAX_INVENTORY_SLOTS; i++) {
        int col = i % slots_per_row;
        int row = i / slots_per_row;
        
        SDL_Rect slot_rect = {
            inv_x + col * (slot_size + spacing),
            inv_y + row * (slot_size + spacing) + y_offset,
            slot_size,
            slot_size
        };
        
        // Only render if visible
        if (slot_rect.y + slot_rect.h > inv_y && slot_rect.y < inv_y + clip_rect.h) {
            if (i < my_inventory_count) {
                render_inventory_slot(renderer, &my_inventory[i], slot_rect);
                
                // Check for mouse hover to show tooltip
                SDL_Point mp = {mx, my};
                if (SDL_PointInRect(&mp, &slot_rect) && my_inventory[i].item.item_id > 0) {
                    strncpy(tooltip_text, my_inventory[i].item.name, sizeof(tooltip_text) - 1);
                    tooltip_x = mx + 10;
                    tooltip_y = my - 30;
                    show_tooltip = 1;
                }
            } else {
                render_inventory_slot(renderer, NULL, slot_rect);
            }
            
            // Highlight selected slot
            if (i == selected_inv_slot) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                SDL_RenderDrawRect(renderer, &slot_rect);
                SDL_Rect inner = {slot_rect.x + 1, slot_rect.y + 1, slot_rect.w - 2, slot_rect.h - 2};
                SDL_RenderDrawRect(renderer, &inner);
            }
        }
    }
    
    // Reset clipping
    SDL_RenderSetClipRect(renderer, NULL);
    
    // Action buttons at bottom
    SDL_Rect btn_use = {inventory_win.x + 110, inventory_win.y + inventory_win.h - 40, 80, 30};
    SDL_Rect btn_drop = {inventory_win.x + 200, inventory_win.y + inventory_win.h - 40, 80, 30};
    SDL_Rect btn_equip = {inventory_win.x + 290, inventory_win.y + inventory_win.h - 40, 90, 30};
    
    // Check if selected item is consumable
    int is_consumable = 0;
    if (selected_inv_slot >= 0 && selected_inv_slot < my_inventory_count) {
        is_consumable = (my_inventory[selected_inv_slot].item.type == ITEM_TYPE_CONSUMABLE);
    }
    
    // Use button - only active for consumables
    if (is_consumable) {
        SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); // Grayed out
    }
    SDL_RenderFillRect(renderer, &btn_use);
    render_text(renderer, get_string(STR_USE), btn_use.x + 30, btn_use.y + 7, is_consumable ? col_white : (SDL_Color){100, 100, 100, 255}, 0);
    
    SDL_SetRenderDrawColor(renderer, 100, 50, 0, 255);
    SDL_RenderFillRect(renderer, &btn_drop);
    render_text(renderer, get_string(STR_DROP), btn_drop.x + 25, btn_drop.y + 7, col_white, 0);
    
    SDL_SetRenderDrawColor(renderer, 0, 0, 150, 255);
    SDL_RenderFillRect(renderer, &btn_equip);
    render_text(renderer, get_string(STR_EQUIP), btn_equip.x + 25, btn_equip.y + 7, col_white, 0);
}

void render_dialogue(SDL_Renderer *renderer, int w, int h) {
    if (!show_dialogue) return;
    
    // Center dialogue window
    dialogue_window = (SDL_Rect){w/2 - 250, h/2 - 150, 500, 300};
    
    // Background
    SDL_SetRenderDrawColor(renderer, 40, 30, 20, 240);
    SDL_RenderFillRect(renderer, &dialogue_window);
    
    // Border
    SDL_SetRenderDrawColor(renderer, 200, 150, 100, 255);
    SDL_RenderDrawRect(renderer, &dialogue_window);
    
    // NPC Name header
    SDL_Rect header = {dialogue_window.x, dialogue_window.y, dialogue_window.w, 40};
    SDL_SetRenderDrawColor(renderer, 60, 40, 20, 255);
    SDL_RenderFillRect(renderer, &header);
    render_text(renderer, dialogue_npc_name, header.x + header.w/2, header.y + 10, (SDL_Color){255, 215, 0, 255}, 1);
    
    // Dialogue text (word wrapped)
    int text_x = dialogue_window.x + 20;
    int text_y = dialogue_window.y + 60;
    int max_width = dialogue_window.w - 40;
    
    // Simple word wrapping
    char word[64];
    char line[128] = "";
    int word_idx = 0;
    for (int i = 0; i <= strlen(dialogue_text); i++) {
        if (dialogue_text[i] == ' ' || dialogue_text[i] == '\0') {
            word[word_idx] = '\0';
            
            // Check if adding this word would exceed line width
            char test_line[128];
            snprintf(test_line, 128, "%s%s ", line, word);
            int tw, th;
            TTF_SizeText(font, test_line, &tw, &th);
            
            if (tw > max_width && strlen(line) > 0) {
                // Render current line and start new one
                render_text(renderer, line, text_x, text_y, col_white, 0);
                text_y += 20;
                strcpy(line, word);
                strcat(line, " ");
            } else {
                strcat(line, word);
                strcat(line, " ");
            }
            
            word_idx = 0;
        } else {
            word[word_idx++] = dialogue_text[i];
        }
    }
    
    // Render last line
    if (strlen(line) > 0) {
        render_text(renderer, line, text_x, text_y, col_white, 0);
    }
    
    // Close button
    SDL_Rect btn_close = {dialogue_window.x + dialogue_window.w - 80, dialogue_window.y + dialogue_window.h - 50, 60, 35};
    SDL_SetRenderDrawColor(renderer, 100, 50, 50, 255);
    SDL_RenderFillRect(renderer, &btn_close);
    SDL_SetRenderDrawColor(renderer, 200, 100, 100, 255);
    SDL_RenderDrawRect(renderer, &btn_close);
    render_text(renderer, "Close", btn_close.x + 10, btn_close.y + 8, col_white, 0);
}

void render_shop(SDL_Renderer *renderer, int w, int h) {
    if (!show_shop) return;
    
    // Center shop window
    shop_window = (SDL_Rect){w/2 - 300, h/2 - 250, 600, 500};
    
    // Background
    SDL_SetRenderDrawColor(renderer, 30, 25, 20, 240);
    SDL_RenderFillRect(renderer, &shop_window);
    
    // Border
    SDL_SetRenderDrawColor(renderer, 150, 120, 80, 255);
    SDL_RenderDrawRect(renderer, &shop_window);
    
    // Title
    char title[64];
    snprintf(title, 64, "%s - Shop", dialogue_npc_name);
    render_text(renderer, title, shop_window.x + shop_window.w/2, shop_window.y + 10, (SDL_Color){255, 215, 0, 255}, 1);
    
    // Tab buttons
    SDL_Rect tab_buy = {shop_window.x + 20, shop_window.y + 40, 100, 30};
    SDL_Rect tab_sell = {shop_window.x + 130, shop_window.y + 40, 100, 30};
    
    if (shop_tab == 0) {
        SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255);
        SDL_RenderFillRect(renderer, &tab_buy);
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_RenderFillRect(renderer, &tab_sell);
    } else {
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_RenderFillRect(renderer, &tab_buy);
        SDL_SetRenderDrawColor(renderer, 100, 100, 0, 255);
        SDL_RenderFillRect(renderer, &tab_sell);
    }
    
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer, &tab_buy);
    SDL_RenderDrawRect(renderer, &tab_sell);
    
    render_text(renderer, "Buy", tab_buy.x + 35, tab_buy.y + 7, col_white, 0);
    render_text(renderer, "Sell", tab_sell.x + 30, tab_sell.y + 7, col_white, 0);
    
    // Gold display
    char gold_str[32];
    snprintf(gold_str, 32, "Your Gold: %d", my_gold);
    render_text(renderer, gold_str, shop_window.x + shop_window.w - 150, shop_window.y + 48, (SDL_Color){255, 215, 0, 255}, 0);
    
    // Item list area
    SDL_Rect list_area = {shop_window.x + 20, shop_window.y + 80, shop_window.w - 40, shop_window.h - 140};
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 200);
    SDL_RenderFillRect(renderer, &list_area);
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawRect(renderer, &list_area);
    
    if (shop_tab == 0) {
        // Buy tab - show shop items
        int y_offset = 0;
        for (int i = 0; i < current_shop.item_count; i++) {
            if (i < shop_scroll) continue;
            if (y_offset > list_area.h - 40) break;
            
            SDL_Rect item_rect = {list_area.x + 10, list_area.y + 10 + y_offset, list_area.w - 20, 35};
            
            // Hover effect
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            SDL_Point scaled_mouse = {(int)(mx * scale_x / ui_scale), (int)(my * scale_y / ui_scale)};
            
            if (SDL_PointInRect(&scaled_mouse, &item_rect)) {
                SDL_SetRenderDrawColor(renderer, 60, 60, 40, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 40, 40, 30, 255);
            }
            SDL_RenderFillRect(renderer, &item_rect);
            SDL_SetRenderDrawColor(renderer, 80, 80, 60, 255);
            SDL_RenderDrawRect(renderer, &item_rect);
            
            // Item info
            char item_info[128];
            snprintf(item_info, 128, "%s - %d gold", 
                current_shop.items[i].name, current_shop.buy_prices[i]);
            render_text(renderer, item_info, item_rect.x + 10, item_rect.y + 10, col_white, 0);
            
            y_offset += 40;
        }
        
        if (current_shop.item_count == 0) {
            render_text(renderer, "No items available", list_area.x + list_area.w/2, list_area.y + list_area.h/2, (SDL_Color){150, 150, 150, 255}, 1);
        }
    } else {
        // Sell tab - show player inventory
        int y_offset = 0;
        int visible_item_count = 0;
        for (int i = 0; i < my_inventory_count; i++) {
            if (my_inventory[i].is_equipped) continue; // Don't show equipped items
            if (my_inventory[i].item.item_id <= 0) continue;
            
            if (visible_item_count < shop_scroll) {
                visible_item_count++;
                continue;
            }
            if (y_offset > list_area.h - 40) break;
            visible_item_count++;
            
            SDL_Rect item_rect = {list_area.x + 10, list_area.y + 10 + y_offset, list_area.w - 20, 35};
            
            // Hover effect
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            SDL_Point scaled_mouse = {(int)(mx * scale_x / ui_scale), (int)(my * scale_y / ui_scale)};
            
            if (SDL_PointInRect(&scaled_mouse, &item_rect)) {
                SDL_SetRenderDrawColor(renderer, 60, 60, 40, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 40, 40, 30, 255);
            }
            SDL_RenderFillRect(renderer, &item_rect);
            SDL_SetRenderDrawColor(renderer, 80, 80, 60, 255);
            SDL_RenderDrawRect(renderer, &item_rect);
            
            // Item info with sell price (half of buy price from shop)
            int sell_price = 0;
            for (int j = 0; j < current_shop.item_count; j++) {
                if (current_shop.items[j].item_id == my_inventory[i].item.item_id) {
                    sell_price = current_shop.sell_prices[j];
                    break;
                }
            }
            if (sell_price == 0) sell_price = 1; // Minimum 1 gold
            
            char item_info[128];
            snprintf(item_info, 128, "%s x%d - %d gold", 
                my_inventory[i].item.name, my_inventory[i].item.quantity, sell_price);
            render_text(renderer, item_info, item_rect.x + 10, item_rect.y + 10, col_white, 0);
            
            y_offset += 40;
        }
        
        if (my_inventory_count == 0) {
            render_text(renderer, "No items to sell", list_area.x + list_area.w/2, list_area.y + list_area.h/2, (SDL_Color){150, 150, 150, 255}, 1);
        }
    }
    
    // Close button
    SDL_Rect btn_close = {shop_window.x + shop_window.w - 100, shop_window.y + shop_window.h - 50, 80, 35};
    SDL_SetRenderDrawColor(renderer, 100, 50, 50, 255);
    SDL_RenderFillRect(renderer, &btn_close);
    SDL_SetRenderDrawColor(renderer, 200, 100, 100, 255);
    SDL_RenderDrawRect(renderer, &btn_close);
    render_text(renderer, "Close", btn_close.x + 20, btn_close.y + 8, col_white, 0);
}

void render_quest_log(SDL_Renderer *renderer, int w, int h) {
    if (!show_quest_log) return;
    
    // Quest log window
    quest_window = (SDL_Rect){w/2 - 300, h/2 - 250, 600, 500};
    
    // Background
    SDL_SetRenderDrawColor(renderer, 30, 20, 40, 240);
    SDL_RenderFillRect(renderer, &quest_window);
    
    // Border
    SDL_SetRenderDrawColor(renderer, 150, 100, 200, 255);
    SDL_RenderDrawRect(renderer, &quest_window);
    
    // Title
    render_text(renderer, "Quest Log", quest_window.x + quest_window.w/2, quest_window.y + 10, (SDL_Color){200, 150, 255, 255}, 1);
    
    // Quest list
    if (my_quest_count == 0) {
        render_text(renderer, "No active quests", quest_window.x + quest_window.w/2, quest_window.y + quest_window.h/2, (SDL_Color){150, 150, 150, 255}, 1);
    } else {
        int y = quest_window.y + 50;
        for (int i = 0; i < my_quest_count; i++) {
            Quest *q = &my_quests[i];
            
            // Quest name
            render_text(renderer, q->name, quest_window.x + 20, y, col_yellow, 0);
            y += 25;
            
            // Objectives
            for (int j = 0; j < q->objective_count; j++) {
                QuestObjective *obj = &q->objectives[j];
                char obj_text[128];
                
                const char *obj_type = "";
                if (obj->objective_type == 0) obj_type = "Kill";
                else if (obj->objective_type == 1) obj_type = "Collect";
                else if (obj->objective_type == 2) obj_type = "Visit";
                
                snprintf(obj_text, 128, "  %s %s: %d/%d", obj_type, obj->target_name, obj->current_count, obj->required_count);
                
                SDL_Color obj_color = obj->current_count >= obj->required_count ? (SDL_Color){0, 255, 0, 255} : col_white;
                render_text(renderer, obj_text, quest_window.x + 30, y, obj_color, 0);
                y += 20;
            }
            
            // Rewards
            char reward_text[128];
            snprintf(reward_text, 128, "  Rewards: %dg, %dxp", q->reward_gold, q->reward_xp);
            render_text(renderer, reward_text, quest_window.x + 30, y, (SDL_Color){255, 215, 0, 255}, 0);
            y += 20;
            
            // Check if quest is complete
            int all_complete = 1;
            for (int j = 0; j < q->objective_count; j++) {
                if (q->objectives[j].current_count < q->objectives[j].required_count) {
                    all_complete = 0;
                    break;
                }
            }
            
            // Complete button if ready
            if (all_complete) {
                SDL_Rect btn_complete = {quest_window.x + 30, y, 120, 30};
                SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255);
                SDL_RenderFillRect(renderer, &btn_complete);
                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
                SDL_RenderDrawRect(renderer, &btn_complete);
                render_text(renderer, "Complete!", btn_complete.x + 25, btn_complete.y + 7, col_white, 0);
            }
            
            y += 35;
        }
    }
    
    // Close button
    SDL_Rect btn_close = {quest_window.x + quest_window.w - 100, quest_window.y + quest_window.h - 50, 80, 35};
    SDL_SetRenderDrawColor(renderer, 100, 50, 50, 255);
    SDL_RenderFillRect(renderer, &btn_close);
    SDL_SetRenderDrawColor(renderer, 200, 100, 100, 255);
    SDL_RenderDrawRect(renderer, &btn_close);
    render_text(renderer, "Close", btn_close.x + 20, btn_close.y + 8, col_white, 0);
}

void render_quest_offer(SDL_Renderer *renderer, int w, int h) {
    if (!show_quest_offer) return;
    
    // Quest offer window
    SDL_Rect offer_window = {w/2 - 300, h/2 - 250, 600, 500};
    
    // Background
    SDL_SetRenderDrawColor(renderer, 40, 30, 20, 240);
    SDL_RenderFillRect(renderer, &offer_window);
    
    // Border
    SDL_SetRenderDrawColor(renderer, 200, 150, 100, 255);
    SDL_RenderDrawRect(renderer, &offer_window);
    
    // Title
    render_text(renderer, "Available Quests", offer_window.x + offer_window.w/2, offer_window.y + 10, col_yellow, 1);
    
    if (available_quest_count == 0) {
        render_text(renderer, "No quests available", offer_window.x + offer_window.w/2, offer_window.y + offer_window.h/2, (SDL_Color){150, 150, 150, 255}, 1);
    } else {
        int y = offer_window.y + 50;
        for (int i = 0; i < available_quest_count; i++) {
            Quest *q = &available_quests[i];
            
            // Quest name (clickable)
            SDL_Rect quest_rect = {offer_window.x + 20, y - 5, offer_window.w - 40, 25};
            
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            SDL_Point scaled_mouse = {(int)(mx * scale_x / ui_scale), (int)(my * scale_y / ui_scale)};
            
            if (SDL_PointInRect(&scaled_mouse, &quest_rect)) {
                SDL_SetRenderDrawColor(renderer, 80, 60, 40, 255);
                SDL_RenderFillRect(renderer, &quest_rect);
            }
            
            render_text(renderer, q->name, offer_window.x + 30, y, col_yellow, 0);
            y += 25;
            
            // Description
            render_text(renderer, q->description, offer_window.x + 30, y, (SDL_Color){200, 200, 200, 255}, 0);
            y += 20;
            
            // Level requirement
            char level_text[64];
            snprintf(level_text, 64, "  Level Required: %d", q->level_required);
            render_text(renderer, level_text, offer_window.x + 30, y, (SDL_Color){150, 150, 255, 255}, 0);
            y += 20;
            
            // Rewards
            char reward_text[128];
            snprintf(reward_text, 128, "  Rewards: %dg, %dxp", q->reward_gold, q->reward_xp);
            render_text(renderer, reward_text, offer_window.x + 30, y, (SDL_Color){255, 215, 0, 255}, 0);
            y += 30;
        }
    }
    
    // Close button
    SDL_Rect btn_close = {offer_window.x + offer_window.w - 100, offer_window.y + offer_window.h - 50, 80, 35};
    SDL_SetRenderDrawColor(renderer, 100, 50, 50, 255);
    SDL_RenderFillRect(renderer, &btn_close);
    SDL_SetRenderDrawColor(renderer, 200, 100, 100, 255);
    SDL_RenderDrawRect(renderer, &btn_close);
    render_text(renderer, "Close", btn_close.x + 20, btn_close.y + 8, col_white, 0);
}

void render_debug_overlay(SDL_Renderer *renderer, int screen_w) {
    if (!show_debug_info) {
        // Clean up cached textures when overlay is hidden
        for(int i = 0; i < debug_text_line_count; i++) {
            if (debug_text_textures[i]) {
                SDL_DestroyTexture(debug_text_textures[i]);
                debug_text_textures[i] = NULL;
            }
        }
        debug_cache_last_update = 0;
        debug_text_line_count = 0;
        return;
    }
    
    Uint32 now = SDL_GetTicks();
    // Only update debug textures every 500ms (2 times per second) to reduce overhead
    int should_update = (debug_text_line_count == 0) || (now - debug_cache_last_update > 500);
    
    if (should_update) {
        // Clean up old textures
        for(int i = 0; i < debug_text_line_count; i++) {
            if (debug_text_textures[i]) {
                SDL_DestroyTexture(debug_text_textures[i]);
                debug_text_textures[i] = NULL;
            }
        }
        
        // Get current values
        float px=0, py=0; 
        for(int i=0; i<MAX_CLIENTS; i++) 
            if(local_players[i].active && local_players[i].id == local_player_id) { 
                px=local_players[i].x; py=local_players[i].y; 
            }
        
        // Build debug text
        char lines[14][128]; 
        debug_text_line_count = 0;
        snprintf(lines[debug_text_line_count++], 128, get_string(STR_PING), current_ping);
        snprintf(lines[debug_text_line_count++], 128, "Server IP: %s", server_ip);
        snprintf(lines[debug_text_line_count++], 128, "Pos: %.1f, %.1f", px, py);
        
        // Rendering backend info
        #ifdef USE_VULKAN
        if (render_backend == RENDER_BACKEND_VULKAN) {
            snprintf(lines[debug_text_line_count++], 128, "Render: Vulkan");
        } else {
            snprintf(lines[debug_text_line_count++], 128, "Render: OpenGL");
        }
        #else
        snprintf(lines[debug_text_line_count++], 128, "Render: OpenGL");
        #endif
        
        SDL_RendererInfo info;
        SDL_GetRendererInfo(renderer, &info);
        const char *video_drv = SDL_GetCurrentVideoDriver();
        snprintf(lines[debug_text_line_count++], 128, "VideoDrv: %s", video_drv ? video_drv : "Unknown");
        
        // Show GPU info based on backend
        #ifdef USE_VULKAN
        if (render_backend == RENDER_BACKEND_VULKAN && vk_device_name[0]) {
            snprintf(lines[debug_text_line_count++], 128, "GPU: %s", vk_device_name);
        } else
        #endif
        {
            if (info.name) snprintf(lines[debug_text_line_count++], 128, "GPU: %s", info.name); 
            else snprintf(lines[debug_text_line_count++], 128, "GPU: Unknown");
        }
        
        // Always use cached GL info to avoid expensive glGetString() calls
        int is_gl_backend = (render_backend == RENDER_BACKEND_OPENGL);
        if (is_gl_backend) {
            if (gl_renderer_cache[0]) snprintf(lines[debug_text_line_count++], 128, "GL Renderer: %s", gl_renderer_cache);
            if (gl_vendor_cache[0]) snprintf(lines[debug_text_line_count++], 128, "GL Vendor: %s", gl_vendor_cache);
        }
        
        SDL_version compiled; SDL_VERSION(&compiled); 
        snprintf(lines[debug_text_line_count++], 128, "SDL: %d.%d.%d", compiled.major, compiled.minor, compiled.patch);
        #ifndef _WIN32
        struct utsname buffer; 
        if (uname(&buffer) == 0) snprintf(lines[debug_text_line_count++], 128, "OS: %s %s", buffer.sysname, buffer.release); 
        else snprintf(lines[debug_text_line_count++], 128, "OS: Unknown");
        #else
        snprintf(lines[debug_text_line_count++], 128, "OS: Windows");
        #endif
        char compiler_name[10] = "Unknown";
        #if defined(__clang__) 
            strcpy(compiler_name, "Clang");
        #elif defined(__GNUC__)
            strcpy(compiler_name, "GCC");
        #endif
        snprintf(lines[debug_text_line_count++], 128, "Compiler: %s %s", compiler_name, __VERSION__);
        
        // Pre-render all text lines to textures
        for(int i = 0; i < debug_text_line_count; i++) {
            SDL_Color color = col_white; 
            if (strncmp(lines[i], "Ping:", 5) == 0) color = col_green;
            if (strncmp(lines[i], "Render: Vulkan", 14) == 0) color = col_cyan;
            
            SDL_Surface *surface = TTF_RenderUTF8_Blended(font, lines[i], color);
            if (surface) {
                debug_text_textures[i] = SDL_CreateTextureFromSurface(renderer, surface);
                debug_text_rects[i].w = surface->w;
                debug_text_rects[i].h = surface->h;
                SDL_FreeSurface(surface);
            }
        }
        
        // Calculate box dimensions
        debug_box_w = 450;
        debug_box_h = (debug_text_line_count * 20) + 10;
        debug_box_x = screen_w - debug_box_w - 10;
        if (debug_box_x < 10) debug_box_x = 10;
        
        debug_cache_last_update = now;
    }
    
    // Render cached textures (fast)
    SDL_Rect dbg_box = {debug_box_x, 10, debug_box_w, debug_box_h};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150); 
    SDL_RenderFillRect(renderer, &dbg_box);
    
    int y = 15;
    for(int i = 0; i < debug_text_line_count; i++) {
        if (debug_text_textures[i]) {
            SDL_Rect dst = {dbg_box.x + 10, y, debug_text_rects[i].w, debug_text_rects[i].h};
            SDL_RenderCopy(renderer, debug_text_textures[i], NULL, &dst);
        }
        y += 20;
    }
}

void render_role_list(SDL_Renderer *renderer, int w, int h) {
    if (!show_role_list) {
        // Clean up cache when window is closed
        if (cached_role_list_tex) {
            SDL_DestroyTexture(cached_role_list_tex);
            cached_role_list_tex = NULL;
            cached_staff_count = -1;
        }
        return;
    }

    SDL_Rect win = {w/2 - 200, h/2 - 225, 400, 450};
    
    // Invalidate cache if staff count changed
    if (cached_staff_count != staff_count) {
        if (cached_role_list_tex) {
            SDL_DestroyTexture(cached_role_list_tex);
            cached_role_list_tex = NULL;
        }
        cached_staff_count = staff_count;
    }
    
    // Create cached texture if needed (performance optimization for mobile)
    if (!cached_role_list_tex) {
        // Create a target texture
        cached_role_list_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, 
                                                 SDL_TEXTUREACCESS_TARGET, 400, 450);
        if (cached_role_list_tex) {
            SDL_SetTextureBlendMode(cached_role_list_tex, SDL_BLENDMODE_BLEND);
            
            // Render to texture
            SDL_SetRenderTarget(renderer, cached_role_list_tex);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); 
            SDL_RenderClear(renderer);
            
            // Background
            SDL_Rect bg = {0, 0, 400, 450};
            SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255); 
            SDL_RenderFillRect(renderer, &bg);
            SDL_SetRenderDrawColor(renderer, 200, 200, 255, 255); 
            SDL_RenderDrawRect(renderer, &bg);

            // Title
            render_text(renderer, get_string(STR_SERVER_STAFF_LIST), 200, 15, col_cyan, 1);

            // Render staff entries
            int y = 50;
            
            if (staff_count == 0) {
                render_text(renderer, get_string(STR_LOADING), 200, y + 20, col_white, 1);
            }

            for (int i = 0; i < staff_count; i++) {
                char buffer[128];
                const char* role_name = get_role_name(staff_list[i].role);
                SDL_Color c = get_role_color(staff_list[i].role);

                // Format: [ROLE] Name (ID: X)
                snprintf(buffer, 128, "[%s] %s (ID: %d)", role_name, staff_list[i].name, staff_list[i].id);
                
                render_text(renderer, buffer, 20, y, c, 0);
                y += 25;
            }
            
            // Reset render target
            SDL_SetRenderTarget(renderer, NULL);
        }
    }
    
    // Draw the window with cached content
    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255); 
    SDL_RenderFillRect(renderer, &win);
    SDL_SetRenderDrawColor(renderer, 200, 200, 255, 255); 
    SDL_RenderDrawRect(renderer, &win);
    
    // Draw cached texture with scrolling support
    if (cached_role_list_tex) {
        // Setup clipping for scrollable content area
        SDL_Rect content_area = {win.x + 10, win.y + 50, win.w - 20, win.h - 60};
        SDL_RenderSetClipRect(renderer, &content_area);
        
        // Draw with scroll offset
        SDL_Rect src = {0, 0, 400, 450};
        SDL_Rect dst = {win.x, win.y - role_list_scroll, 400, 450};
        SDL_RenderCopy(renderer, cached_role_list_tex, &src, &dst);
        
        SDL_RenderSetClipRect(renderer, NULL);
    }
    
    // Draw close button on top (not cached since it needs to be interactive)
    SDL_Rect btn_close = {win.x + 360, win.y + 5, 30, 30};
    SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); 
    SDL_RenderFillRect(renderer, &btn_close);
    render_text(renderer, get_string(STR_X_CLOSE), btn_close.x + 10, btn_close.y + 5, col_white, 0);
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

#ifdef __ANDROID__
// JNI callback function to receive selected image data from Java
JNIEXPORT void JNICALL
Java_com_mmo_client_MainActivity_nativeOnImageSelected(JNIEnv* env, jobject obj, jbyteArray data, jint size) {
    if (!sock || sock <= 0) {
        printf("Error: Invalid socket, cannot upload avatar\n");
        return;
    }
    
    if (size > 0 && size <= MAX_AVATAR_SIZE) {
        // Get the byte array from Java
        jbyte* imageBytes = (*env)->GetByteArrayElements(env, data, NULL);
        if (imageBytes != NULL) {
            // Send avatar upload packet
            Packet pkt;
            memset(&pkt, 0, sizeof(Packet));
            pkt.type = PACKET_AVATAR_UPLOAD;
            pkt.image_size = size;
            
            // Send packet header first
            send_all(sock, &pkt, sizeof(Packet));
            // Then send image data
            send_all(sock, (uint8_t*)imageBytes, size);
            
            // Release the byte array
            (*env)->ReleaseByteArrayElements(env, data, imageBytes, JNI_ABORT);
            
            printf("Avatar uploaded: %d bytes\n", size);
        }
    } else {
        printf("Invalid avatar size: %d bytes (max: %d)\n", size, MAX_AVATAR_SIZE);
    }
}
#endif

#ifdef __IPHONEOS__
// iOS callback function to receive selected image data
static void ios_image_selected_callback(const uint8_t* data, size_t size) {
    if (!sock || sock <= 0) {
        printf("Error: Invalid socket, cannot upload avatar\n");
        return;
    }
    
    if (size > 0 && size <= MAX_AVATAR_SIZE) {
        // Send avatar upload packet
        Packet pkt;
        memset(&pkt, 0, sizeof(Packet));
        pkt.type = PACKET_AVATAR_UPLOAD;
        pkt.image_size = size;
        
        // Send packet header first
        send_all(sock, &pkt, sizeof(Packet));
        // Then send image data
        send_all(sock, (uint8_t*)data, size);
        
        printf("Avatar uploaded: %d bytes\n", (int)size);
    } else {
        printf("Invalid avatar size: %zu bytes (max: %d)\n", size, MAX_AVATAR_SIZE);
    }
}
#endif

// Platform-specific file picker for avatar selection
void open_file_picker_for_avatar() {
    #if defined(_WIN32)
    // Windows: Use native file dialog
    char filename[1024] = {0};
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrFilter = "Image Files\0*.PNG;*.JPG;*.JPEG;*.BMP\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (GetOpenFileNameA(&ofn) == TRUE) {
        // Load and send avatar
        FILE *f = fopen(filename, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);
            
            if (size > 0 && size <= MAX_AVATAR_SIZE) {
                uint8_t *data = malloc(size);
                if (data) {
                    fread(data, 1, size, f);
                    
                    // Send avatar upload packet
                    Packet pkt;
                    memset(&pkt, 0, sizeof(Packet));
                    pkt.type = PACKET_AVATAR_UPLOAD;
                    pkt.image_size = size;
                    
                    // Send packet header first
                    send_all(sock, &pkt, sizeof(Packet));
                    // Then send image data
                    send_all(sock, data, size);
                    
                    free(data);
                }
            }
            fclose(f);
        }
    }
    
    #elif defined(__APPLE__) && !defined(__IPHONEOS__)
    // macOS: Use Objective-C NSOpenPanel
    typedef void* (*NSOpenPanelFunc)();
    typedef void (*PanelSetAllowedFileTypesFunc)(void*, void*);
    typedef void (*PanelBeginFunc)(void*, void*);
    typedef long (*PanelRunModalFunc)(void*);
    typedef void* (*PanelURLFunc)(void*);
    typedef const char* (*URLPathFunc)(void*);
    
    void *panel = objc_msgSend((void*)objc_getClass("NSOpenPanel"), sel_registerName("openPanel"));
    if (panel) {
        // Set allowed file types
        void *fileTypes = objc_msgSend((void*)objc_getClass("NSArray"), sel_registerName("arrayWithObjects:"),
                                       objc_msgSend((void*)objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), "png"),
                                       objc_msgSend((void*)objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), "jpg"),
                                       objc_msgSend((void*)objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), "jpeg"),
                                       objc_msgSend((void*)objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), "bmp"),
                                       NULL);
        objc_msgSend(panel, sel_registerName("setAllowedFileTypes:"), fileTypes);
        
        // Run modal
        long result = (long)objc_msgSend(panel, sel_registerName("runModal"));
        if (result == 1) { // NSModalResponseOK
            void *url = objc_msgSend(panel, sel_registerName("URL"));
            if (url) {
                const char *path = (const char*)objc_msgSend(url, sel_registerName("fileSystemRepresentation"));
                if (path) {
                    // Load and send avatar (similar to Windows code)
                    FILE *f = fopen(path, "rb");
                    if (f) {
                        fseek(f, 0, SEEK_END);
                        long size = ftell(f);
                        fseek(f, 0, SEEK_SET);
                        
                        if (size > 0 && size <= MAX_AVATAR_SIZE) {
                            uint8_t *data = malloc(size);
                            if (data) {
                                fread(data, 1, size, f);
                                
                                Packet pkt;
                                memset(&pkt, 0, sizeof(Packet));
                                pkt.type = PACKET_AVATAR_UPLOAD;
                                pkt.image_size = size;
                                
                                send_all(sock, &pkt, sizeof(Packet));
                                send_all(sock, data, size);
                                
                                free(data);
                            }
                        }
                        fclose(f);
                    }
                }
            }
        }
    }
    
    #elif defined(__IPHONEOS__)
    // iOS: Use UIKit document picker
    ios_set_image_callback(ios_image_selected_callback);
    ios_open_file_picker();
    
    #elif defined(__ANDROID__)
    // Android: Call Java method via JNI to open file picker
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    
    if (env && activity) {
        // Get the MainActivity class (not the object class)
        jclass clazz = (*env)->FindClass(env, "com/mmo/client/MainActivity");
        if (clazz) {
            jmethodID method = (*env)->GetStaticMethodID(env, clazz, "openFilePicker", "()V");
            if (method) {
                (*env)->CallStaticVoidMethod(env, clazz, method);
                printf("Android file picker opened\n");
            } else {
                printf("Failed to find openFilePicker method\n");
            }
            (*env)->DeleteLocalRef(env, clazz);
        } else {
            printf("Failed to find MainActivity class\n");
        }
        (*env)->DeleteLocalRef(env, activity);
    } else {
        printf("Failed to get JNI environment or activity\n");
    }
    
    #else
    // Linux/other: Try zenity as fallback
    FILE *fp = popen("zenity --file-selection --file-filter='Images | *.png *.jpg *.jpeg *.bmp' 2>/dev/null", "r");
    if (fp) {
        char filename[1024];
        if (fgets(filename, sizeof(filename), fp)) {
            // Remove trailing newline
            filename[strcspn(filename, "\n")] = 0;
            
            FILE *f = fopen(filename, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);
                
                if (size > 0 && size <= MAX_AVATAR_SIZE) {
                    uint8_t *data = malloc(size);
                    if (data) {
                        fread(data, 1, size, f);
                        
                        Packet pkt;
                        memset(&pkt, 0, sizeof(Packet));
                        pkt.type = PACKET_AVATAR_UPLOAD;
                        pkt.image_size = size;
                        
                        send_all(sock, &pkt, sizeof(Packet));
                        send_all(sock, data, size);
                        
                        free(data);
                    }
                }
                fclose(f);
            }
        }
        pclose(fp);
    }
    #endif
}

void render_settings_menu(SDL_Renderer *renderer, int screen_w, int screen_h) {
    if (!is_settings_open) return;
    
    // 1. Setup Main Window (Fixed Size)
    int win_h = 600;
    settings_win = (SDL_Rect){screen_w/2 - 175, screen_h/2 - 300, 350, win_h}; 
    
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255); SDL_RenderFillRect(renderer, &settings_win);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &settings_win);
    render_text(renderer, get_string(STR_SETTINGS), settings_win.x + 175, settings_win.y + 10, col_white, 1);

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
    render_text(renderer, get_string(STR_SHOW_DEBUG_INFO), start_x + 30, y, col_white, 0); y += 40;

    btn_toggle_fps = (SDL_Rect){start_x, y, 20, 20};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &btn_toggle_fps);
    if (show_fps) { SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_Rect c={btn_toggle_fps.x+4,btn_toggle_fps.y+4,12,12}; SDL_RenderFillRect(renderer,&c); }
    render_text(renderer, get_string(STR_SHOW_FPS), start_x + 30, y, col_white, 0); y += 40;

    btn_toggle_coords = (SDL_Rect){start_x, y, 20, 20};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &btn_toggle_coords);
    if (show_coords) { SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_Rect c={btn_toggle_coords.x+4,btn_toggle_coords.y+4,12,12}; SDL_RenderFillRect(renderer,&c); }
    render_text(renderer, get_string(STR_SHOW_COORDINATES), start_x + 30, y, col_white, 0); y += 40;

    btn_toggle_unread = (SDL_Rect){start_x, y, 20, 20};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &btn_toggle_unread);
    if (show_unread_counter) { SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_Rect c={btn_toggle_unread.x+4,btn_toggle_unread.y+4,12,12}; SDL_RenderFillRect(renderer,&c); }
    render_text(renderer, get_string(STR_SHOW_UNREAD_COUNTER), start_x + 30, y, col_white, 0); y += 40;

    #ifndef __ANDROID__
    btn_toggle_vulkan = (SDL_Rect){start_x, y, 20, 20};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &btn_toggle_vulkan);
    if (config_use_vulkan) { 
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); 
        SDL_Rect c={btn_toggle_vulkan.x+4,btn_toggle_vulkan.y+4,12,12}; 
        SDL_RenderFillRect(renderer,&c); 
    }
    render_text(renderer, get_string(STR_USE_VULKAN), start_x + 30, y, col_white, 0); y += 40;
    #endif

    #if !defined(_WIN32) && !defined(__APPLE__) && !defined(__ANDROID__)
    btn_toggle_nvidia_gpu = (SDL_Rect){start_x, y, 20, 20};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderFillRect(renderer, &btn_toggle_nvidia_gpu);
    if (config_use_nvidia_gpu) { 
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); 
        SDL_Rect c={btn_toggle_nvidia_gpu.x+4,btn_toggle_nvidia_gpu.y+4,12,12}; 
        SDL_RenderFillRect(renderer,&c); 
    }
    render_text(renderer, get_string(STR_USE_NVIDIA_GPU), start_x + 30, y, col_white, 0); y += 40;
    #endif

    // -- Buttons --
    btn_view_blocked = (SDL_Rect){start_x, y, 300, 30};
    SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255); SDL_RenderFillRect(renderer, &btn_view_blocked);
    render_text(renderer, get_string(STR_MANAGE_BLOCKED_PLAYERS), btn_view_blocked.x + 150, btn_view_blocked.y + 5, col_white, 1);
    y += 40;

    char id_str[32]; snprintf(id_str, 32, get_string(STR_MY_ID), local_player_id);
    render_text(renderer, id_str, settings_win.x + 175, y, (SDL_Color){150, 150, 255, 255}, 1);
    y += 25;

    int my_status = 0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == local_player_id) my_status = local_players[i].status;
    btn_cycle_status = (SDL_Rect){start_x, y, 300, 30};
    SDL_SetRenderDrawColor(renderer, 50, 50, 100, 255); SDL_RenderFillRect(renderer, &btn_cycle_status);
    char status_str[64]; snprintf(status_str, 64, get_string(STR_STATUS), status_names[my_status]);
    render_text(renderer, status_str, btn_cycle_status.x + 150, btn_cycle_status.y + 5, get_status_color(my_status), 1);
    y += 40;

    SDL_Rect btn_nick = {start_x, y, 300, 30};
    SDL_SetRenderDrawColor(renderer, 100, 50, 150, 255); SDL_RenderFillRect(renderer, &btn_nick);
    render_text(renderer, get_string(STR_CHANGE_NICKNAME), btn_nick.x + 150, btn_nick.y + 5, col_white, 1);
    y += 40;

    btn_change_avatar = (SDL_Rect){start_x, y, 300, 30};
    SDL_SetRenderDrawColor(renderer, 50, 150, 100, 255); SDL_RenderFillRect(renderer, &btn_change_avatar);
    render_text(renderer, get_string(STR_CHANGE_AVATAR), btn_change_avatar.x + 150, btn_change_avatar.y + 5, col_white, 1);
    y += 40;

    btn_change_password = (SDL_Rect){start_x, y, 300, 30};
    SDL_SetRenderDrawColor(renderer, 150, 50, 100, 255); SDL_RenderFillRect(renderer, &btn_change_password);
    render_text(renderer, get_string(STR_CHANGE_PASSWORD), btn_change_password.x + 150, btn_change_password.y + 5, col_white, 1);
    y += 40;
  
    

    render_text(renderer, get_string(STR_NAME_COLOR_START), settings_win.x + 175, y, (SDL_Color){my_r, my_g, my_b, 255}, 1); 
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
    render_text(renderer, get_string(STR_NAME_COLOR_END), settings_win.x + 175, y, (SDL_Color){my_r2, my_g2, my_b2, 255}, 1); 
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
    render_text(renderer, get_string(STR_MUSIC_VOLUME), settings_win.x + 175, y, col_white, 1); 
    y += 25;
    slider_volume = (SDL_Rect){start_x + 30, y, 240, 15};
    render_fancy_slider(renderer, &slider_volume, music_volume/128.0f, (SDL_Color){0, 200, 255, 255}); 
    y += 50;

    // -- AFK --
    char afk_str[64]; snprintf(afk_str, 64, get_string(STR_AUTO_AFK), afk_timeout_minutes);
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
    char scale_str[64]; snprintf(scale_str, 64, get_string(STR_UI_SCALE), display_scale);
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
    char zoom_str[64]; snprintf(zoom_str, 64, get_string(STR_GAME_ZOOM), display_zoom);
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
    render_text(renderer, get_string(STR_DISCONNECT_LOGOUT), btn_disconnect_rect.x + 150, btn_disconnect_rect.y + 5, col_white, 1); 
    y += 40;

    // --- Docs / Staff / Credits Row ---
    int btn_w = 95; 
    
    // 1. Docs
    btn_documentation_rect = (SDL_Rect){start_x, y, btn_w, 30};
    SDL_SetRenderDrawColor(renderer, 0, 100, 150, 255); SDL_RenderFillRect(renderer, &btn_documentation_rect);
    render_text(renderer, get_string(STR_DOCS), btn_documentation_rect.x + 47, btn_documentation_rect.y + 5, col_white, 1);

    // 2. Staff
    btn_staff_list_rect = (SDL_Rect){start_x + 100, y, btn_w, 30};
    SDL_SetRenderDrawColor(renderer, 150, 100, 0, 255); SDL_RenderFillRect(renderer, &btn_staff_list_rect);
    render_text(renderer, get_string(STR_STAFF), btn_staff_list_rect.x + 47, btn_staff_list_rect.y + 5, col_white, 1);

    // 3. Credits
    btn_contributors_rect = (SDL_Rect){start_x + 200, y, btn_w, 30};
    SDL_SetRenderDrawColor(renderer, 0, 150, 100, 255); SDL_RenderFillRect(renderer, &btn_contributors_rect);
    render_text(renderer, get_string(STR_CREDITS), btn_contributors_rect.x + 47, btn_contributors_rect.y + 5, col_white, 1);
    
    y += 40; // Move down for next row

    // --- My Warnings Button ---
    btn_my_warnings = (SDL_Rect){start_x, y, 300, 30};
    SDL_SetRenderDrawColor(renderer, 200, 100, 0, 255); SDL_RenderFillRect(renderer, &btn_my_warnings);
    render_text(renderer, get_string(STR_VIEW_MY_WARNINGS), btn_my_warnings.x + 150, btn_my_warnings.y + 5, col_white, 1);

    y += 45; // Gap after button
    
    // --- Language Button ---
    btn_cycle_language = (SDL_Rect){start_x, y, 300, 30};
    SDL_SetRenderDrawColor(renderer, 100, 100, 200, 255); SDL_RenderFillRect(renderer, &btn_cycle_language);
    char lang_str[64]; snprintf(lang_str, 64, "%s: %s", get_string(STR_LANGUAGE), get_language_name(current_language));
    render_text(renderer, lang_str, btn_cycle_language.x + 150, btn_cycle_language.y + 5, col_white, 1);
    
    y += 45; // Gap after button

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
        
        render_text(renderer, get_string(STR_CHANGE_NICKNAME), pop.x+150, pop.y+10, col_yellow, 1);
        render_text(renderer, auth_message, pop.x+150, pop.y+35, col_red, 1);

        int py = pop.y + 60;
        render_text(renderer, get_string(STR_NEW_NAME), pop.x+20, py, col_white, 0);
        render_input_with_cursor(renderer, (SDL_Rect){pop.x+20, py+20, 260, 25}, nick_new, active_field==10, 0);

        py += 60;
        render_text(renderer, get_string(STR_TYPE_CONFIRM), pop.x+20, py, col_white, 0);
        render_input_with_cursor(renderer, (SDL_Rect){pop.x+20, py+20, 260, 25}, nick_confirm, active_field==11, 0);

        py += 60;
        render_text(renderer, get_string(STR_CURRENT_PASSWORD), pop.x+20, py, col_white, 0);
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
        render_text(renderer, get_string(STR_SHOW), btn_show_nick_pass.x + 20, btn_show_nick_pass.y - 2, col_white, 0);
        // -----------------------------------------

        SDL_Rect btn_submit = {pop.x+20, pop.y+240, 120, 30};
        SDL_Rect btn_cancel = {pop.x+160, pop.y+240, 120, 30};
        SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_submit);
        render_text(renderer, get_string(STR_SUBMIT), btn_submit.x+60, btn_submit.y+5, col_white, 1);
        SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_cancel);
        render_text(renderer, get_string(STR_CANCEL), btn_cancel.x+60, btn_cancel.y+5, col_white, 1);
    }
    
    // --- Password Change Popup ---
    if (show_password_popup) {
        SDL_Rect pop = {screen_w/2 - 175, screen_h/2 - 200, 350, 400};
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); SDL_RenderFillRect(renderer, &pop);
        SDL_SetRenderDrawColor(renderer, 200, 100, 0, 255); SDL_RenderDrawRect(renderer, &pop);
        
        render_text(renderer, get_string(STR_CHANGE_PASSWORD), pop.x+175, pop.y+10, col_yellow, 1);
        render_text(renderer, password_message, pop.x+175, pop.y+35, col_red, 1);

        int py = pop.y + 60;
        render_text(renderer, get_string(STR_CURRENT_PASSWORD), pop.x+20, py, col_white, 0);
        render_input_with_cursor(renderer, (SDL_Rect){pop.x+20, py+20, 240, 25}, password_current, active_field==FIELD_PASSWORD_CURRENT, !show_password_current);
        
        // Show checkbox for current password
        SDL_Rect btn_show_curr = {pop.x + 270, py + 25, 15, 15};
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); 
        SDL_RenderDrawRect(renderer, &btn_show_curr);
        if (show_password_current) {
            SDL_Rect inner = {btn_show_curr.x+3, btn_show_curr.y+3, 9, 9};
            SDL_RenderFillRect(renderer, &inner);
        }
        render_text(renderer, get_string(STR_SHOW), btn_show_curr.x + 20, btn_show_curr.y - 2, col_white, 0);

        py += 70;
        render_text(renderer, get_string(STR_NEW_PASSWORD), pop.x+20, py, col_white, 0);
        render_input_with_cursor(renderer, (SDL_Rect){pop.x+20, py+20, 240, 25}, password_new, active_field==FIELD_PASSWORD_NEW, !show_password_new);
        
        // Show checkbox for new password
        SDL_Rect btn_show_new = {pop.x + 270, py + 25, 15, 15};
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); 
        SDL_RenderDrawRect(renderer, &btn_show_new);
        if (show_password_new) {
            SDL_Rect inner = {btn_show_new.x+3, btn_show_new.y+3, 9, 9};
            SDL_RenderFillRect(renderer, &inner);
        }
        render_text(renderer, get_string(STR_SHOW), btn_show_new.x + 20, btn_show_new.y - 2, col_white, 0);

        py += 70;
        render_text(renderer, get_string(STR_CONFIRM_PASSWORD), pop.x+20, py, col_white, 0);
        render_input_with_cursor(renderer, (SDL_Rect){pop.x+20, py+20, 240, 25}, password_confirm, active_field==FIELD_PASSWORD_CONFIRM, !show_password_confirm);
        
        // Show checkbox for confirm password
        SDL_Rect btn_show_conf = {pop.x + 270, py + 25, 15, 15};
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); 
        SDL_RenderDrawRect(renderer, &btn_show_conf);
        if (show_password_confirm) {
            SDL_Rect inner = {btn_show_conf.x+3, btn_show_conf.y+3, 9, 9};
            SDL_RenderFillRect(renderer, &inner);
        }
        render_text(renderer, get_string(STR_SHOW), btn_show_conf.x + 20, btn_show_conf.y - 2, col_white, 0);

        SDL_Rect btn_submit = {pop.x+20, pop.y+340, 150, 30};
        SDL_Rect btn_cancel = {pop.x+180, pop.y+340, 150, 30};
        SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_submit);
        render_text(renderer, get_string(STR_CHANGE), btn_submit.x+75, btn_submit.y+5, col_white, 1);
        SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_cancel);
        render_text(renderer, get_string(STR_CANCEL), btn_cancel.x+75, btn_cancel.y+5, col_white, 1);
    }
}

void render_friend_list(SDL_Renderer *renderer, int w, int h) {
    if (!show_friend_list) return;

    int max_text_w = 200; 
    for(int i=0; i<friend_count; i++) {
        char temp_str[128];
        if (my_friends[i].is_online) snprintf(temp_str, 128, "%s (%s)", my_friends[i].username, get_string(STR_ONLINE));
        else snprintf(temp_str, 128, "%s (%s: %s)", my_friends[i].username, get_string(STR_LAST), my_friends[i].last_login);
        int fw, fh; TTF_SizeText(font, temp_str, &fw, &fh);
        if (fw > max_text_w) max_text_w = fw;
    }

    int win_w = max_text_w + 110; if (win_w < 300) win_w = 300; 
    friend_list_win = (SDL_Rect){w/2 - (win_w/2), h/2 - 200, win_w, 400};

    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); SDL_RenderFillRect(renderer, &friend_list_win);
    SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255); SDL_RenderDrawRect(renderer, &friend_list_win);
    
    int title_w, title_h; TTF_SizeText(font, get_string(STR_FRIENDS_LIST), &title_w, &title_h);
    render_text(renderer, get_string(STR_FRIENDS_LIST), friend_list_win.x + (win_w/2) - (title_w/2), friend_list_win.y + 10, col_green, 0);

    // --- CLOSE BUTTON GLOBAL UPDATE ---
    btn_friend_close_rect = (SDL_Rect){friend_list_win.x + win_w - 40, friend_list_win.y + 5, 30, 30};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_friend_close_rect);
    render_text(renderer, get_string(STR_X_CLOSE), btn_friend_close_rect.x + 10, btn_friend_close_rect.y + 5, col_white, 1);
    // ----------------------------------

    btn_friend_add_id_rect = (SDL_Rect){friend_list_win.x + 20, friend_list_win.y + 45, 100, 30};
    SDL_SetRenderDrawColor(renderer, 50, 50, 150, 255); SDL_RenderFillRect(renderer, &btn_friend_add_id_rect);
    render_text(renderer, get_string(STR_PLUS_ID), btn_friend_add_id_rect.x+30, btn_friend_add_id_rect.y+5, col_white, 0);

    // Set up clipping for scrollable area
    SDL_Rect clip_rect = {friend_list_win.x, friend_list_win.y + 80, friend_list_win.w, friend_list_win.h - 80};
    SDL_RenderSetClipRect(renderer, &clip_rect);
    
    int y_off = 85 - friend_list_scroll; // Apply scroll offset
    for(int i=0; i<friend_count; i++) {
        // Only render if within visible area
        if (y_off + 30 > 80 && y_off < friend_list_win.h) {
            char display[128];
            SDL_Color text_col = col_white;
            if (my_friends[i].is_online) { snprintf(display, 128, "%s (%s)", my_friends[i].username, get_string(STR_ONLINE)); text_col = col_green; } 
            else { snprintf(display, 128, "%s (%s: %s)", my_friends[i].username, get_string(STR_LAST), my_friends[i].last_login); text_col = (SDL_Color){150, 150, 150, 255}; }
            render_text(renderer, display, friend_list_win.x + 20, friend_list_win.y + y_off, text_col, 0);

            SDL_Rect btn_del = {friend_list_win.x + win_w - 50, friend_list_win.y + y_off, 40, 20};
            SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_del);
            render_text(renderer, get_string(STR_DEL), btn_del.x+8, btn_del.y+2, col_white, 0);
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
    char msg[64]; snprintf(msg, 64, get_string(STR_FRIEND_REQUEST_FROM), pending_friend_name);
    render_text(renderer, msg, popup_win.x + 150, popup_win.y + 20, col_yellow, 1);
    SDL_Rect btn_accept = {popup_win.x + 20, popup_win.y + 70, 120, 30}; SDL_Rect btn_deny = {popup_win.x + 160, popup_win.y + 70, 120, 30};
    SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_accept);
    render_text(renderer, get_string(STR_ACCEPT), btn_accept.x + 60, btn_accept.y + 5, col_white, 1);
    SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_deny);
    render_text(renderer, get_string(STR_DENY), btn_deny.x + 60, btn_deny.y + 5, col_white, 1);
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
    if (!is_friend) { SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255); SDL_RenderFillRect(renderer, &btn); render_text(renderer, get_string(STR_ADD_FRIEND), btn.x + (btn_w/2) - 40, btn.y + 5, col_white, 0); } 
    else { SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); SDL_RenderFillRect(renderer, &btn); render_text(renderer, get_string(STR_REMOVE), btn.x + (btn_w/2) - 30, btn.y + 5, col_white, 0); }
    btn_add_friend = btn;

    btn_send_pm = (SDL_Rect){profile_win.x + 20, start_y + 40, btn_w, 30};
    SDL_SetRenderDrawColor(renderer, 100, 0, 100, 255); SDL_RenderFillRect(renderer, &btn_send_pm);
    render_text(renderer, get_string(STR_MESSAGE), btn_send_pm.x + (btn_w/2) - 30, btn_send_pm.y + 5, col_white, 0);

    SDL_Rect btn_hide = {profile_win.x + 20, start_y + 80, btn_w, 30};
    int hidden = is_blocked(selected_player_id);
    if (hidden) { SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_hide); render_text(renderer, get_string(STR_UNHIDE), btn_hide.x + (btn_w/2) - 20, btn_hide.y + 5, col_white, 0); } 
    else { SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255); SDL_RenderFillRect(renderer, &btn_hide); render_text(renderer, get_string(STR_HIDE), btn_hide.x + (btn_w/2) - 20, btn_hide.y + 5, col_white, 0); }
    btn_hide_player_dyn = btn_hide;

    // --- NEW: Sanction Button (Admin Only) ---
    if (my_role >= ROLE_ADMIN) {
        btn_sanction_open = (SDL_Rect){profile_win.x + 20, start_y + 120, btn_w, 30};
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); SDL_RenderFillRect(renderer, &btn_sanction_open);
        render_text(renderer, get_string(STR_SANCTION), btn_sanction_open.x + (btn_w/2) - 35, btn_sanction_open.y + 5, col_white, 0);
    }
}

void render_auth_screen(SDL_Renderer *renderer) {
    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
    
    // Apply UI scaling to auth screen (same as game UI)
    SDL_RenderSetScale(renderer, ui_scale, ui_scale);
    
    // Adjust dimensions for scaled rendering
    float scaled_w = w / ui_scale;
    float scaled_h = h / ui_scale;
    
    auth_box = (SDL_Rect){scaled_w/2-200, scaled_h/2-200, 400, 400}; 
    
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); SDL_RenderFillRect(renderer, &auth_box);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); SDL_RenderDrawRect(renderer, &auth_box);
    
    render_text(renderer, get_string(STR_C_MMO_LOGIN), auth_box.x + 200, auth_box.y + 20, col_white, 1);
    render_text(renderer, auth_message, auth_box.x + 200, auth_box.y + 50, col_red, 1);

    int y_start = auth_box.y + 80;

    // 1. IP Field
    render_text(renderer, get_string(STR_SERVER_IP), auth_box.x + 20, y_start, col_white, 0);
    render_input_with_cursor(renderer, (SDL_Rect){auth_box.x + 130, y_start - 5, 200, 25}, input_ip, active_field == 2, 0);

    // 2. Port Field
    render_text(renderer, get_string(STR_PORT), auth_box.x + 20, y_start + 40, col_white, 0);
    render_input_with_cursor(renderer, (SDL_Rect){auth_box.x + 130, y_start + 35, 80, 25}, input_port, active_field == 3, 0);

    // 3. Username
    y_start += 90;
    render_text(renderer, get_string(STR_USERNAME), auth_box.x + 20, y_start, col_white, 0);
    render_input_with_cursor(renderer, (SDL_Rect){auth_box.x + 130, y_start - 5, 200, 25}, auth_username, active_field == 0, 0);

    // 4. Password
    render_text(renderer, get_string(STR_PASSWORD), auth_box.x + 20, y_start + 50, col_white, 0);
    render_input_with_cursor(renderer, (SDL_Rect){auth_box.x + 130, y_start + 45, 200, 25}, auth_password, active_field == 1, !show_password);
    
    btn_show_pass = (SDL_Rect){auth_box.x + 340, y_start + 50, 15, 15};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDrawRect(renderer, &btn_show_pass);
    if (show_password) { SDL_Rect inner = {btn_show_pass.x + 3, btn_show_pass.y + 3, btn_show_pass.w - 6, btn_show_pass.h - 6}; SDL_RenderFillRect(renderer, &inner); }
    render_text(renderer, get_string(STR_SHOW), btn_show_pass.x + 20, btn_show_pass.y, col_white, 0);

    // Buttons & Server/Profile Lists
    btn_login = (SDL_Rect){auth_box.x+20, auth_box.y+280, 160, 40};
    btn_register = (SDL_Rect){auth_box.x+220, auth_box.y+280, 160, 40};
    btn_open_servers = (SDL_Rect){auth_box.x+20, auth_box.y+340, 180, 30};
    btn_open_profiles = (SDL_Rect){auth_box.x+210, auth_box.y+340, 170, 30};

    SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &btn_login);
    render_text(renderer, get_string(STR_LOGIN), btn_login.x + 80, btn_login.y + 10, col_white, 1);
    SDL_SetRenderDrawColor(renderer, 0, 0, 150, 255); SDL_RenderFillRect(renderer, &btn_register);
    render_text(renderer, get_string(STR_REGISTER), btn_register.x + 80, btn_register.y + 10, col_white, 1);
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_open_servers);
    render_text(renderer, get_string(STR_SAVED_SERVERS), btn_open_servers.x + 90, btn_open_servers.y + 5, col_white, 1);
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_open_profiles);
    render_text(renderer, get_string(STR_SAVED_PROFILES), btn_open_profiles.x + 85, btn_open_profiles.y + 5, col_white, 1);

    if (show_server_list) {
        server_list_win = (SDL_Rect){scaled_w/2 - 125, scaled_h/2 - 200, 250, 400}; 
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255); SDL_RenderFillRect(renderer, &server_list_win);
        SDL_SetRenderDrawColor(renderer, 200, 200, 0, 255); SDL_RenderDrawRect(renderer, &server_list_win);
        render_text(renderer, get_string(STR_SELECT_SERVER), server_list_win.x + 125, server_list_win.y + 10, col_yellow, 1);
        
        // Close button
        btn_close_servers = (SDL_Rect){server_list_win.x + 210, server_list_win.y + 5, 30, 30};
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_close_servers);
        render_text(renderer, get_string(STR_X_CLOSE), btn_close_servers.x + 15, btn_close_servers.y + 5, col_white, 1);
        
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
        render_text(renderer, get_string(STR_SAVE_CURRENT_IP), btn_add_server.x + 105, btn_add_server.y + 5, col_white, 1);
    }
    
    if (show_profile_list) {
        profile_list_win = (SDL_Rect){scaled_w/2 - 125, scaled_h/2 - 200, 250, 400}; 
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255); SDL_RenderFillRect(renderer, &profile_list_win);
        SDL_SetRenderDrawColor(renderer, 0, 200, 200, 255); SDL_RenderDrawRect(renderer, &profile_list_win);
        render_text(renderer, get_string(STR_SELECT_PROFILE), profile_list_win.x + 125, profile_list_win.y + 10, col_cyan, 1);
        
        // Close button
        btn_close_profiles = (SDL_Rect){profile_list_win.x + 210, profile_list_win.y + 5, 30, 30};
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_close_profiles);
        render_text(renderer, get_string(STR_X_CLOSE), btn_close_profiles.x + 15, btn_close_profiles.y + 5, col_white, 1);
        
        int y_p = 50;
        for(int i=0; i<profile_count; i++) {
            SDL_Rect row = {profile_list_win.x + 10, profile_list_win.y + y_p, 230, 30};
            SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255); SDL_RenderFillRect(renderer, &row);
            render_text(renderer, profile_list[i].username, row.x + 5, row.y + 5, col_white, 0);
            y_p += 35;
        }
        btn_add_profile = (SDL_Rect){profile_list_win.x + 20, profile_list_win.y + 350, 210, 30};
        SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255); SDL_RenderFillRect(renderer, &btn_add_profile);
        render_text(renderer, get_string(STR_SAVE_CURRENT), btn_add_profile.x + 105, btn_add_profile.y + 5, col_white, 1);
    }
    
    #if defined(__ANDROID__) || defined(__IPHONEOS__)
    render_mobile_text_menu(renderer);
    #endif
    
    SDL_RenderPresent(renderer);
}

void render_blocked_list(SDL_Renderer *renderer, int w, int h) {
    if (!show_blocked_list) return;

    blocked_win_rect = (SDL_Rect){w/2 - 150, h/2 - 200, 300, 400}; // Use global
    
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); SDL_RenderFillRect(renderer, &blocked_win_rect);
    SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255); SDL_RenderDrawRect(renderer, &blocked_win_rect);
    
    render_text(renderer, get_string(STR_BLOCKED_PLAYERS), blocked_win_rect.x + 150, blocked_win_rect.y + 10, col_red, 1);

    // --- CLOSE BUTTON GLOBAL UPDATE ---
    btn_blocked_close_rect = (SDL_Rect){blocked_win_rect.x + 260, blocked_win_rect.y + 5, 30, 30};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_blocked_close_rect);
    render_text(renderer, get_string(STR_X_CLOSE), btn_blocked_close_rect.x + 15, btn_blocked_close_rect.y + 5, col_white, 1);
    // ----------------------------------

    if (blocked_count == 0) {
        render_text(renderer, get_string(STR_NO_HIDDEN_PLAYERS), blocked_win_rect.x + 150, blocked_win_rect.y + 100, col_white, 1);
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
            render_text(renderer, get_string(STR_UNHIDE), btn_unblock.x + 30, btn_unblock.y + 2, col_white, 1);
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
            render_text(renderer, get_string(STR_PROJECT_CONTRIBUTORS), 200, 15, col_cyan, 1);
            
            int y = 60;
            int center_x = 200;

            render_text(renderer, get_string(STR_YOU_FOR_PLAYING), center_x, y, (SDL_Color){200,200,200,255}, 1); y += 40;
            render_text(renderer, get_string(STR_MAIN_DEVELOPER), center_x, y, col_white, 1); y += 25;
            render_text(renderer, "HALt The Dragon", center_x, y, col_white, 1); y += 40;
            render_text(renderer, get_string(STR_AI_ASSISTANT), center_x, y, col_white, 1); y += 25;
            render_text(renderer, "Gemini", center_x, y, col_white, 1); y += 40;
            render_text(renderer, get_string(STR_MULTIPLAYER_TESTS), center_x, y, col_white, 1); y += 25;
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
    render_text(renderer, get_string(STR_X_CLOSE), btn_close.x + 10, btn_close.y + 5, col_white, 0);
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

    // Fixed size window: 450x500 (smaller for better fit on various screens)
    SDL_Rect win = {w/2 - 225, h/2 - 250, 450, 500};
    
    // Create cached texture if needed (performance optimization for mobile)
    if (!cached_documentation_tex) {
        // Create a target texture for the static content - use larger height for all content
        cached_documentation_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, 
                                                     SDL_TEXTUREACCESS_TARGET, 450, 1000);
        if (cached_documentation_tex) {
            SDL_SetTextureBlendMode(cached_documentation_tex, SDL_BLENDMODE_BLEND);
            
            // Render to texture
            SDL_SetRenderTarget(renderer, cached_documentation_tex);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0); 
            SDL_RenderClear(renderer);
            
            // Background (will be larger than visible area for scrolling)
            SDL_Rect bg = {0, 0, 450, 1000};
            SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); 
            SDL_RenderFillRect(renderer, &bg);
            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); 
            SDL_RenderDrawRect(renderer, &bg);

            // Title
            render_text(renderer, "Game Documentation", 225, 15, col_green, 1);

            // Render content starting at fixed position (scrolling handled later)
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

            // 3. Shortcuts & Editing
            render_text(renderer, "#Shortcuts & Editing#", start_x, y, col_yellow, 0); y += 30;
            render_text(renderer, "- Select: Shift + Arrows OR Mouse Drag", start_x, y, col_white, 0); y += 25;
            #if !defined(__ANDROID__) && !defined(__IPHONEOS__)
            render_text(renderer, "- Copy/Paste: Ctrl+C, Ctrl+V, Ctrl+X", start_x, y, col_white, 0); y += 25;
            #else
            render_text(renderer, "- Copy/Paste: Use context menu", start_x, y, col_white, 0); y += 25;
            #endif
            render_text(renderer, "- Select All: Ctrl+A", start_x, y, col_white, 0); y += 25;
            render_text(renderer, "- Cursor: Click to move, Arrows to nav", start_x, y, col_white, 0); y += 35;

            // 4. Chat Commands
            render_text(renderer, "#Chat Commands#", start_x, y, col_yellow, 0); y += 30;
            render_text(renderer, "Admin Commands (requires Admin role):", start_x, y, (SDL_Color){255,150,150,255}, 0); y += 25;
            render_text(renderer, "- /unban <ID>", start_x + 10, y, col_white, 0); y += 20;
            render_raw_text(renderer, "  Removes ban from player ID", start_x + 10, y, (SDL_Color){180,180,180,255}, 0); y += 25;
            render_text(renderer, "- /unwarn <ID>", start_x + 10, y, col_white, 0); y += 20;
            render_raw_text(renderer, "  Removes last warning from player ID", start_x + 10, y, (SDL_Color){180,180,180,255}, 0); y += 25;
            render_text(renderer, "- /role <ID> <LEVEL>", start_x + 10, y, col_white, 0); y += 20;
            render_raw_text(renderer, "  Sets role (0=Player, 1=Admin, 2=Dev, 3=Contrib, 4=VIP)", start_x + 10, y, (SDL_Color){180,180,180,255}, 0); y += 35;
            
            // Store content height for scrolling
            documentation_tex_w = 450;
            documentation_tex_h = y; // Actual content height
            
            // Reset render target
            SDL_SetRenderTarget(renderer, NULL);
        }
    }
    
    // Draw the window with cached content
    // Background and border are drawn fresh each frame for proper positioning
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); 
    SDL_RenderFillRect(renderer, &win);
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); 
    SDL_RenderDrawRect(renderer, &win);
    
    // Draw cached texture with scrolling support
    if (cached_documentation_tex) {
        // Setup clipping for scrollable content area
        SDL_Rect content_area = {win.x + 10, win.y + 50, win.w - 20, win.h - 60};
        SDL_RenderSetClipRect(renderer, &content_area);
        
        // Draw with scroll offset - use actual texture dimensions
        SDL_Rect src = {0, 0, documentation_tex_w, documentation_tex_h};
        SDL_Rect dst = {win.x + 10, win.y + 50 - documentation_scroll, documentation_tex_w, documentation_tex_h};
        SDL_RenderCopy(renderer, cached_documentation_tex, &src, &dst);
        
        SDL_RenderSetClipRect(renderer, NULL);
    }
    
    // Draw close button on top (not cached since it needs to be interactive)
    SDL_Rect btn_close = {win.x + win.w - 40, win.y + 5, 30, 30};
    SDL_SetRenderDrawColor(renderer, 150, 0, 0, 255); 
    SDL_RenderFillRect(renderer, &btn_close);
    render_text(renderer, get_string(STR_X_CLOSE), btn_close.x + 10, btn_close.y + 5, col_white, 0);
}


void render_mobile_controls(SDL_Renderer *renderer, int h) {
    #if defined(__IPHONEOS__) || defined(__ANDROID__)
    if (!joystick_active || touch_id_dpad == -1) return;
    
    // Draw Base (dpad_rect is in UI space, renderer will scale it)
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 100);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderFillRect(renderer, &dpad_rect);
    
    // Draw Knob (use UI space coordinates)
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

#if defined(__ANDROID__) || defined(__IPHONEOS__)
// Helper function to get the buffer and max length for the currently active field
static char* get_active_field_buffer(int *max_len_out) {
    if (is_chat_open) {
        if (max_len_out) *max_len_out = 60;
        return input_buffer;
    }
    
    if (max_len_out) *max_len_out = 31; // Default for most fields
    
    switch (active_field) {
        case FIELD_AUTH_USERNAME: return auth_username;
        case FIELD_AUTH_PASSWORD: return auth_password;
        case FIELD_IP: 
            if (max_len_out) *max_len_out = 63;
            return input_ip;
        case FIELD_PORT: 
            if (max_len_out) *max_len_out = 5;
            return input_port;
        case FIELD_NICK_NEW: return nick_new;
        case FIELD_NICK_CONFIRM: return nick_confirm;
        case FIELD_NICK_PASS: return nick_pass;
        case FIELD_PASSWORD_CURRENT:
        case FIELD_PASSWORD_NEW:
        case FIELD_PASSWORD_CONFIRM:
            if (max_len_out) *max_len_out = 63;
            if (active_field == FIELD_PASSWORD_CURRENT) return password_current;
            if (active_field == FIELD_PASSWORD_NEW) return password_new;
            return password_confirm;
        case FIELD_FRIEND_ID:
            if (max_len_out) *max_len_out = 8;
            return input_friend_id;
        case FIELD_SANCTION_REASON:
            if (max_len_out) *max_len_out = 63;
            return input_sanction_reason;
        case FIELD_BAN_TIME:
            if (max_len_out) *max_len_out = 15;
            return input_ban_time;
        default:
            return NULL;
    }
}

#if defined(__ANDROID__) || defined(__IPHONEOS__)
// Helper function to calculate chat window Y position with keyboard shift (mobile only)
static int get_chat_window_y(int scaled_h, float ui_scale_val) {
    int chat_y = scaled_h - CHAT_DEFAULT_Y_OFFSET;
    
    // Apply keyboard shift to avoid overlap
    if (chat_input_active && keyboard_height > 0) {
        int shift = keyboard_height / ui_scale_val;
        chat_y -= shift;
        if (chat_y < CHAT_MIN_Y_POSITION) chat_y = CHAT_MIN_Y_POSITION;
    }
    
    return chat_y;
}
#endif

void render_mobile_text_menu(SDL_Renderer *renderer) {
    if (!show_mobile_text_menu) return;
    
    // Menu dimensions
    int button_w = 65;
    int button_h = 40;
    int button_spacing = 5;
    
    // Position menu near selection or at tap location
    mobile_text_menu_rect = (SDL_Rect){mobile_text_menu_x, mobile_text_menu_y, MOBILE_TEXT_MENU_WIDTH, MOBILE_TEXT_MENU_HEIGHT};
    
    // Draw menu background
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 230);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderFillRect(renderer, &mobile_text_menu_rect);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer, &mobile_text_menu_rect);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    
    // Draw buttons: Select All | Cut | Copy | Paste | Clear
    int x = mobile_text_menu_rect.x + button_spacing;
    int y = mobile_text_menu_rect.y + 5;
    
    // Select All button
    SDL_Rect btn_select_all = {x, y, button_w, button_h};
    SDL_SetRenderDrawColor(renderer, 200, 150, 80, 255);
    SDL_RenderFillRect(renderer, &btn_select_all);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &btn_select_all);
    render_text(renderer, "All", btn_select_all.x + button_w/2, btn_select_all.y + 10, col_white, 1);
    
    x += button_w + button_spacing;
    
    // Cut button
    SDL_Rect btn_cut = {x, y, button_w, button_h};
    SDL_SetRenderDrawColor(renderer, 180, 80, 80, 255);
    SDL_RenderFillRect(renderer, &btn_cut);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &btn_cut);
    render_text(renderer, "Cut", btn_cut.x + button_w/2, btn_cut.y + 10, col_white, 1);
    
    x += button_w + button_spacing;
    
    // Copy button
    SDL_Rect btn_copy = {x, y, button_w, button_h};
    SDL_SetRenderDrawColor(renderer, 80, 180, 80, 255);
    SDL_RenderFillRect(renderer, &btn_copy);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &btn_copy);
    render_text(renderer, "Copy", btn_copy.x + button_w/2, btn_copy.y + 10, col_white, 1);
    
    x += button_w + button_spacing;
    
    // Paste button
    SDL_Rect btn_paste = {x, y, button_w, button_h};
    SDL_SetRenderDrawColor(renderer, 80, 80, 180, 255);
    SDL_RenderFillRect(renderer, &btn_paste);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &btn_paste);
    render_text(renderer, "Paste", btn_paste.x + button_w/2, btn_paste.y + 10, col_white, 1);
    
    x += button_w + button_spacing;
    
    // Clear button
    SDL_Rect btn_clear = {x, y, button_w, button_h};
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_RenderFillRect(renderer, &btn_clear);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &btn_clear);
    render_text(renderer, "Clear", btn_clear.x + button_w/2, btn_clear.y + 10, col_white, 1);
}
#endif

void render_game(SDL_Renderer *renderer) {
    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
    
    // Reset tooltip at start of frame
    show_tooltip = 0;
    
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
        // Batch blend mode changes to reduce state switches
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        for(int t=0; t<trigger_count; t++) {
            // Only draw triggers for the current map
            if (strcmp(triggers[t].src_map, current_map_file) == 0) {
                SDL_Rect r = triggers[t].rect;
                r.x -= cam_x; // Adjust for camera position
                r.y -= cam_y;
                
                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 100); // Semi-transparent Green
                SDL_RenderFillRect(renderer, &r);
            }
        }
        // Draw all outlines after fills
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        for(int t=0; t<trigger_count; t++) {
            if (strcmp(triggers[t].src_map, current_map_file) == 0) {
                SDL_Rect r = triggers[t].rect;
                r.x -= cam_x;
                r.y -= cam_y;
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
            
            // Draw typing indicator
            if (local_players[i].is_typing) {
                // Animated dots: cycle through 1, 2, 3 dots based on time
                int dot_count = ((now / 500) % 3) + 1;
                char typing_text[8] = "";
                for (int d = 0; d < dot_count; d++) {
                    strcat(typing_text, ".");
                }
                
                // Measure text width for centering
                int tw, th; 
                TTF_SizeText(font, typing_text, &tw, &th);
                
                // Semi-transparent background box
                SDL_Rect typing_bg = { (dst.x + 16) - (tw / 2) - 4, (dst.y - 50) - 2, tw + 8, th + 4 };
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 40, 40, 40, 180); 
                SDL_RenderFillRect(renderer, &typing_bg);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                
                // Render dots
                render_text(renderer, typing_text, dst.x+16, dst.y - 50, (SDL_Color){200, 200, 200, 255}, 1);
            }
            
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

    // 2.4. Draw NPCs
    // Get player position for proximity check
    float player_x = 0, player_y = 0;
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(local_players[i].active && local_players[i].id == local_player_id) {
            player_x = local_players[i].x + PLAYER_WIDTH/2;
            player_y = local_players[i].y + PLAYER_HEIGHT/2;
            break;
        }
    }
    
    for (int i = 0; i < client_npc_count; i++) {
        NPC *npc = &client_npcs[i];
        if (strcmp(npc->map_name, current_map_file) == 0) {
            // Only render NPCs within map boundaries
            if (npc->x < 0 || npc->x > map_w || npc->y < 0 || npc->y > map_h) continue;
            
            SDL_Rect dst = { (int)npc->x - cam_x, (int)npc->y - cam_y, PLAYER_WIDTH, PLAYER_HEIGHT };
            
            // Calculate distance from player to NPC center
            float npc_center_x = npc->x + PLAYER_WIDTH/2;
            float npc_center_y = npc->y + PLAYER_HEIGHT/2;
            float dx = player_x - npc_center_x;
            float dy = player_y - npc_center_y;
            float distance = sqrtf(dx*dx + dy*dy);
            
            // Draw interaction radius if player is nearby
            if (distance <= NPC_INTERACTION_RADIUS) {
                // Draw semi-transparent circle to show interaction range
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                
                // Draw filled circle using multiple rectangles (simple approach)
                int radius = NPC_INTERACTION_RADIUS;
                SDL_SetRenderDrawColor(renderer, 200, 200, 255, 40);
                for (int y = -radius; y <= radius; y++) {
                    for (int x = -radius; x <= radius; x++) {
                        if (x*x + y*y <= radius*radius) {
                            SDL_RenderDrawPoint(renderer, 
                                dst.x + PLAYER_WIDTH/2 + x, 
                                dst.y + PLAYER_HEIGHT/2 + y);
                        }
                    }
                }
                
                // Draw circle outline
                SDL_SetRenderDrawColor(renderer, 150, 150, 255, 180);
                int points = 32;
                for (int j = 0; j < points; j++) {
                    float angle1 = (j * 2.0f * 3.14159f) / points;
                    float angle2 = ((j+1) * 2.0f * 3.14159f) / points;
                    SDL_RenderDrawLine(renderer,
                        dst.x + PLAYER_WIDTH/2 + radius * cosf(angle1),
                        dst.y + PLAYER_HEIGHT/2 + radius * sinf(angle1),
                        dst.x + PLAYER_WIDTH/2 + radius * cosf(angle2),
                        dst.y + PLAYER_HEIGHT/2 + radius * sinf(angle2));
                }
                
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }
            
            // Color NPCs based on type: quest=yellow, merchant=green, generic=white
            if (npc->npc_type == NPC_TYPE_QUEST) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255); // Yellow for quest givers
            } else if (npc->npc_type == NPC_TYPE_MERCHANT) {
                SDL_SetRenderDrawColor(renderer, 0, 255, 100, 255); // Green for merchants
            } else {
                SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); // Gray for generic
            }
            SDL_RenderFillRect(renderer, &dst);
            
            // Draw border
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &dst);
            
            // Render NPC name
            render_text(renderer, npc->name, dst.x+16, dst.y - 18, col_white, 1);
        }
    }

    // 2.5. Draw Ground Items
    // Get mouse position for tooltip (before UI scaling)
    int mouse_x, mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    int world_mouse_x = (int)((mouse_x * scale_x) / game_zoom) + cam_x;
    int world_mouse_y = (int)((mouse_y * scale_y) / game_zoom) + cam_y;
    
    for (int i = 0; i < client_ground_item_count; i++) {
        GroundItem *item = &client_ground_items[i];
        if (strcmp(item->map_name, current_map_file) == 0) {
            SDL_Rect item_rect = {(int)item->x - cam_x, (int)item->y - cam_y, 24, 24};
            SDL_Rect world_item_rect = {(int)item->x, (int)item->y, 24, 24};
            
            // Check for mouse hover (in world coordinates)
            SDL_Point world_mp = {world_mouse_x, world_mouse_y};
            if (SDL_PointInRect(&world_mp, &world_item_rect)) {
                // Use item name from ground item structure (sent by server)
                if (strlen(item->name) > 0) {
                    strncpy(tooltip_text, item->name, sizeof(tooltip_text) - 1);
                    tooltip_text[sizeof(tooltip_text) - 1] = '\0';
                    tooltip_x = (int)((mouse_x * scale_x) / ui_scale) + 10;
                    tooltip_y = (int)((mouse_y * scale_y) / ui_scale) - 30;
                    show_tooltip = 1;
                    // Debug: verify tooltip is being set
                    // printf("Tooltip set: %s at %d,%d\n", tooltip_text, tooltip_x, tooltip_y);
                }
            }
            
            // Color by item type (get from item ID)
            // For now, use a simple color scheme
            if (item->item_id == 1) { // Health potion
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            } else if (item->item_id == 2) { // Sword
                SDL_SetRenderDrawColor(renderer, 200, 200, 0, 255);
            } else if (item->item_id >= 3 && item->item_id <= 5) { // Armor
                SDL_SetRenderDrawColor(renderer, 100, 100, 200, 255);
            } else if (item->item_id == 6) { // Accessory
                SDL_SetRenderDrawColor(renderer, 200, 0, 200, 255);
            } else { // Materials
                SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
            }
            
            SDL_RenderFillRect(renderer, &item_rect);
            
            // White border
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &item_rect);
            
            // Render a small "?" or quantity
            if (item->quantity > 1) {
                char qty[4];
                snprintf(qty, 4, "%d", item->quantity);
                render_text(renderer, qty, item_rect.x + 12, item_rect.y + 4, col_white, 1);
            } else {
                render_text(renderer, "?", item_rect.x + 12, item_rect.y + 4, col_white, 1);
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
    render_inventory(renderer, scaled_w, scaled_h);
    render_dialogue(renderer, scaled_w, scaled_h);
    render_shop(renderer, scaled_w, scaled_h);
    render_quest_log(renderer, scaled_w, scaled_h);
    render_quest_offer(renderer, scaled_w, scaled_h);
    render_debug_overlay(renderer, scaled_w);
    render_hud(renderer, scaled_w, scaled_h);
    render_mobile_controls(renderer, scaled_w);
    
    // 5. Draw HUD Buttons
    btn_chat_toggle = (SDL_Rect){10, scaled_h-40, 100, 30};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_chat_toggle);
    render_text(renderer, is_chat_open ? get_string(STR_CLOSE) : get_string(STR_CHAT), btn_chat_toggle.x+50, btn_chat_toggle.y+5, col_white, 1);
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
    render_text(renderer, get_string(STR_FRIENDS), btn_view_friends.x+50, btn_view_friends.y+5, col_white, 1);

    btn_settings_toggle = (SDL_Rect){230, scaled_h-40, 100, 30};
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); SDL_RenderFillRect(renderer, &btn_settings_toggle);
    render_text(renderer, get_string(STR_SETTINGS), btn_settings_toggle.x+50, btn_settings_toggle.y+5, col_white, 1);

 // 6. Draw Chat Overlay
    if(is_chat_open) {
        // Calculate chat window position with mobile keyboard shift
        int chat_y = scaled_h - 240;  // Default position
        #if defined(__ANDROID__) || defined(__IPHONEOS__)
        chat_y = get_chat_window_y(scaled_h, ui_scale);
        chat_window_shift = (scaled_h - CHAT_DEFAULT_Y_OFFSET) - chat_y;  // Calculate actual shift applied
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

        // Draw History with scroll offset
        // Calculate start position: show last CHAT_VISIBLE_LINES messages by default
        int total_content_height = CHAT_HISTORY * 15;
        int visible_height = CHAT_VISIBLE_LINES * 15;
        int default_scroll = total_content_height - visible_height; // Start scrolled to bottom
        
        // Apply user scroll offset (scroll up shows older messages)
        int effective_scroll = default_scroll - chat_scroll;
        if (effective_scroll < 0) effective_scroll = 0;
        if (effective_scroll > total_content_height - visible_height) effective_scroll = total_content_height - visible_height;
        
        int start_line = effective_scroll / 15;
        int offset_y = effective_scroll % 15;
        
        for(int i=0; i<CHAT_VISIBLE_LINES + 2; i++) { // +2 for partial lines at top/bottom
            int line_idx = start_line + i;
            if (line_idx >= 0 && line_idx < CHAT_HISTORY) {
                SDL_Color line_col = col_white;
                if (strncmp(chat_log[line_idx], "To [", 4) == 0 || strncmp(chat_log[line_idx], "From [", 6) == 0) line_col = col_magenta;
                render_text(renderer, chat_log[line_idx], 15, win.y+10+(i*15)-offset_y, line_col, 0);
            }
        }

        SDL_RenderSetClipRect(renderer, NULL);

        // --- DRAW INPUT AREA BACKGROUND (semi-transparent) ---
        SDL_Rect input_bg = {10, win.y + win.h - 30, 300, 30};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200); // Black with 200/255 opacity
        SDL_RenderFillRect(renderer, &input_bg);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

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
            #define MAX_TEXT_BUFFER_SIZE 256
            #define MAX_TEXT_LENGTH (MAX_TEXT_BUFFER_SIZE - 1)
            
            int start = selection_start;
            int len = selection_len;
            if (len < 0) { start += len; len = -len; }

            // Calculate offset X (Prefix + Text before selection)
            char text_before[MAX_TEXT_BUFFER_SIZE];
            int prefix_len = snprintf(text_before, MAX_TEXT_BUFFER_SIZE, "%s", prefix);
            // Handle truncation - snprintf returns length it would have written
            if (prefix_len >= MAX_TEXT_BUFFER_SIZE) prefix_len = MAX_TEXT_LENGTH;
            if (prefix_len < 0) prefix_len = 0;
            
            // Safely copy up to 'start' characters from input_buffer
            int copy_len = start;
            int buf_len = strlen(input_buffer);
            if (copy_len > buf_len) copy_len = buf_len;
            // Ensure we don't overflow the buffer
            if (prefix_len >= MAX_TEXT_LENGTH) {
                copy_len = 0;  // No room left after prefix
            } else if (prefix_len + copy_len >= MAX_TEXT_BUFFER_SIZE) {
                copy_len = MAX_TEXT_LENGTH - prefix_len;
            }
            if (copy_len < 0) copy_len = 0;
            
            if (copy_len > 0) {
                memcpy(text_before + prefix_len, input_buffer, copy_len);
            }
            text_before[prefix_len + copy_len] = '\0';
            
            int w_before = 0, h;
            TTF_SizeText(font, text_before, &w_before, &h);

            // Calculate width of selection
            char text_sel[MAX_TEXT_BUFFER_SIZE];
            int sel_copy_len = len;
            if (start >= buf_len) {
                sel_copy_len = 0;
            } else if (start + sel_copy_len > buf_len) {
                sel_copy_len = buf_len - start;
            }
            if (sel_copy_len > MAX_TEXT_LENGTH) sel_copy_len = MAX_TEXT_LENGTH;
            if (sel_copy_len < 0) sel_copy_len = 0;
            
            if (sel_copy_len > 0) {
                memcpy(text_sel, input_buffer + start, sel_copy_len);
            }
            text_sel[sel_copy_len] = '\0';
            
            int w_sel = 0;
            if (sel_copy_len > 0) TTF_SizeText(font, text_sel, &w_sel, &h);

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
            
            #undef MAX_TEXT_BUFFER_SIZE
            #undef MAX_TEXT_LENGTH
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
    
    // Render mobile text menu last so it appears on top of everything
    #if defined(__ANDROID__) || defined(__IPHONEOS__)
    render_mobile_text_menu(renderer);
    #endif
    
    // Render tooltip (must be after all UI elements but before scale restore)
    render_tooltip(renderer);
    
    // Restore original render scale
    SDL_RenderSetScale(renderer, old_scale_x, old_scale_y);
    
    SDL_RenderPresent(renderer);
}

// Inventory request functions
void request_item_pickup(void) {
    static Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PACKET_ITEM_PICKUP;
    send_packet(&pkt);
}

void request_item_drop(int slot) {
    static Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PACKET_ITEM_DROP;
    pkt.item_slot = slot;
    send_packet(&pkt);
}

void request_item_use(int slot) {
    static Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PACKET_ITEM_USE;
    pkt.item_slot = slot;
    send_packet(&pkt);
}

void request_item_equip(int slot, int equip_slot) {
    static Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PACKET_ITEM_EQUIP;
    pkt.item_slot = slot;
    pkt.equip_slot = equip_slot;
    send_packet(&pkt);
}

void request_item_unequip(int equip_slot) {
    static Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PACKET_ITEM_UNEQUIP;
    pkt.equip_slot = equip_slot;
    send_packet(&pkt);
}

void handle_game_click(int mx, int my, int cam_x, int cam_y, int w, int h) {

    // ============================================================
    // LAYER -2: QUEST WINDOWS (highest priority)
    // ============================================================
    
    if (show_quest_log) {
        SDL_Rect btn_close = {quest_window.x + quest_window.w - 100, quest_window.y + quest_window.h - 50, 80, 35};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_close)) {
            show_quest_log = 0;
            return;
        }
        
        // Check for Complete button clicks
        int y = quest_window.y + 50;
        for (int i = 0; i < my_quest_count; i++) {
            Quest *q = &my_quests[i];
            
            // Skip to button position (name + objectives + rewards)
            y += 25; // Quest name
            for (int j = 0; j < q->objective_count; j++) {
                y += 20; // Each objective
            }
            y += 20; // Rewards
            
            // Check if quest is complete
            int all_complete = 1;
            for (int j = 0; j < q->objective_count; j++) {
                if (q->objectives[j].current_count < q->objectives[j].required_count) {
                    all_complete = 0;
                    break;
                }
            }
            
            if (all_complete) {
                SDL_Rect btn_complete = {quest_window.x + 30, y, 120, 30};
                if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_complete)) {
                    // Complete quest
                    Packet pkt;
                    memset(&pkt, 0, sizeof(Packet));
                    pkt.type = PACKET_QUEST_COMPLETE;
                    pkt.quest_id = q->quest_id;
                    send_packet(&pkt);
                    printf("Completing quest %d\n", q->quest_id);
                    return;
                }
            }
            
            y += 35;
        }
        
        // Consume clicks inside quest log
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &quest_window)) {
            return;
        }
    }
    
    if (show_quest_offer) {
        SDL_Rect offer_window = {w/2 - 300, h/2 - 250, 600, 500};
        SDL_Rect btn_close = {offer_window.x + offer_window.w - 100, offer_window.y + offer_window.h - 50, 80, 35};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_close)) {
            show_quest_offer = 0;
            return;
        }
        
        // Quest clickable areas
        int y = offer_window.y + 50;
        for (int i = 0; i < available_quest_count; i++) {
            SDL_Rect quest_rect = {offer_window.x + 20, y - 5, offer_window.w - 40, 25};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &quest_rect)) {
                // Accept quest
                Packet pkt;
                memset(&pkt, 0, sizeof(Packet));
                pkt.type = PACKET_QUEST_ACCEPT;
                pkt.quest_id = available_quests[i].quest_id;
                pkt.npc_id = available_quests[i].npc_id;
                send_packet(&pkt);
                printf("Accepting quest %d\n", available_quests[i].quest_id);
                show_quest_offer = 0;
                return;
            }
            y += 95; // Name + description + level + rewards
        }
        
        // Consume clicks inside quest offer
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &offer_window)) {
            return;
        }
    }

    // ============================================================
    // LAYER -1: SHOP WINDOW (highest priority after dialogue)
    // ============================================================
    
    if (show_shop) {
        // Close button
        SDL_Rect btn_close = {shop_window.x + shop_window.w - 100, shop_window.y + shop_window.h - 50, 80, 35};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_close)) {
            show_shop = 0;
            return;
        }
        
        // Tab buttons
        SDL_Rect tab_buy = {shop_window.x + 20, shop_window.y + 40, 100, 30};
        SDL_Rect tab_sell = {shop_window.x + 130, shop_window.y + 40, 100, 30};
        
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &tab_buy)) {
            shop_tab = 0;
            shop_scroll = 0;
            return;
        }
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &tab_sell)) {
            shop_tab = 1;
            shop_scroll = 0;
            return;
        }
        
        // Item clicks
        SDL_Rect list_area = {shop_window.x + 20, shop_window.y + 80, shop_window.w - 40, shop_window.h - 140};
        
        if (shop_tab == 0) {
            // Buy tab - click on shop items
            int y_offset = 0;
            for (int i = 0; i < current_shop.item_count; i++) {
                if (i < shop_scroll) continue;
                if (y_offset > list_area.h - 40) break;
                
                SDL_Rect item_rect = {list_area.x + 10, list_area.y + 10 + y_offset, list_area.w - 20, 35};
                
                if (SDL_PointInRect(&(SDL_Point){mx, my}, &item_rect)) {
                    // Buy item
                    int price = current_shop.buy_prices[i];
                    if (my_gold >= price) {
                        Packet pkt;
                        memset(&pkt, 0, sizeof(Packet));
                        pkt.type = PACKET_SHOP_BUY;
                        pkt.npc_id = current_shop.npc_id;
                        pkt.buy_sell_item_id = current_shop.items[i].item_id;
                        pkt.buy_sell_quantity = 1;
                        send_packet(&pkt);
                        printf("Attempting to buy %s for %d gold\n", current_shop.items[i].name, price);
                    }
                    return;
                }
                
                y_offset += 40;
            }
        } else {
            // Sell tab - click on inventory items
            int y_offset = 0;
            int visible_item_count = 0;
            for (int i = 0; i < my_inventory_count; i++) {
                if (my_inventory[i].is_equipped) continue;
                if (my_inventory[i].item.item_id <= 0) continue;
                
                // Skip items before scroll position
                if (visible_item_count < shop_scroll) {
                    visible_item_count++;
                    continue;
                }
                if (y_offset > list_area.h - 40) break;
                visible_item_count++;
                
                SDL_Rect item_rect = {list_area.x + 10, list_area.y + 10 + y_offset, list_area.w - 20, 35};
                
                if (SDL_PointInRect(&(SDL_Point){mx, my}, &item_rect)) {
                    // Sell item
                    Packet pkt;
                    memset(&pkt, 0, sizeof(Packet));
                    pkt.type = PACKET_SHOP_SELL;
                    pkt.npc_id = current_shop.npc_id;
                    pkt.buy_sell_item_id = my_inventory[i].item.item_id;
                    pkt.buy_sell_quantity = 1;
                    pkt.item_slot = my_inventory[i].slot_index;
                    send_packet(&pkt);
                    printf("Attempting to sell %s (slot %d, item_id %d)\\n", 
                        my_inventory[i].item.name, my_inventory[i].slot_index, my_inventory[i].item.item_id);
                    return;
                }
                
                y_offset += 40;
            }
        }
        
        // If clicked inside shop window but not on interactive elements, consume the click
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &shop_window)) {
            return;
        }
    }

    // ============================================================
    // LAYER 0: DIALOGUE WINDOW (highest priority)
    // ============================================================
    
    if (show_dialogue) {
        SDL_Rect btn_close = {dialogue_window.x + dialogue_window.w - 80, dialogue_window.y + dialogue_window.h - 50, 60, 35};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_close)) {
            show_dialogue = 0;
            return;
        }
        // If clicked inside dialogue window but not on close, consume the click
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &dialogue_window)) {
            return;
        }
    }

    // ============================================================
    // LAYER 1: HUD BUTTONS
    // ============================================================
    
    SDL_Rect btn_inbox_check = {w - 50, 10, 40, 40};
    if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_inbox_check)) {
        is_inbox_open = !is_inbox_open;
        if (is_inbox_open) { show_friend_list = 0; show_add_friend_popup = 0; is_settings_open = 0; show_inventory = 0; }
        return;
    }
    
    SDL_Rect btn_inventory_check = {w - 100, 10, 40, 40};
    if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_inventory_check)) {
        show_inventory = !show_inventory;
        if (show_inventory) { is_inbox_open = 0; show_friend_list = 0; show_add_friend_popup = 0; is_settings_open = 0; show_quest_log = 0; }
        return;
    }
    
    SDL_Rect btn_quest_check = {w - 150, 10, 40, 40};
    if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_quest_check)) {
        show_quest_log = !show_quest_log;
        if (show_quest_log) { 
            is_inbox_open = 0; 
            show_friend_list = 0; 
            show_add_friend_popup = 0; 
            is_settings_open = 0; 
            show_inventory = 0;
            // Request quest list from server
            Packet pkt;
            memset(&pkt, 0, sizeof(Packet));
            pkt.type = PACKET_QUEST_LIST;
            send_packet(&pkt);
        }
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

    // 2B2. Inventory Window
    if (show_inventory) {
        SDL_Rect win = {w/2 - 225, h/2 - 275, 450, 550};
        
        // Close button
        SDL_Rect btn_close = {win.x + win.w - 35, win.y + 5, 30, 30};
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_close)) {
            show_inventory = 0;
            selected_inv_slot = -1;
            return;
        }
        
        // Check equipment slots
        int equip_x = win.x + 10;
        int equip_y = win.y + 50;
        int equip_slot_w = 80;
        int equip_slot_h = 70;
        int equip_spacing = 10;
        
        for (int i = 0; i < EQUIP_SLOT_COUNT; i++) {
            SDL_Rect equip_rect = {
                equip_x,
                equip_y + i * (equip_slot_h + equip_spacing),
                equip_slot_w,
                equip_slot_h
            };
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &equip_rect)) {
                // Clicked on equipment slot - unequip if item present
                if (my_equipment[i].item_id > 0) {
                    request_item_unequip(i);
                    printf("Unequipping item from slot %d\n", i);
                }
                return;
            }
        }
        
        // Check inventory slots
        int inv_x = win.x + 110;
        int inv_y = win.y + 50;
        int slot_size = 60;
        int slots_per_row = 5;
        int spacing = 5;
        int y_offset = -inventory_scroll;
        
        for (int i = 0; i < MAX_INVENTORY_SLOTS; i++) {
            int col = i % slots_per_row;
            int row = i / slots_per_row;
            
            SDL_Rect slot_rect = {
                inv_x + col * (slot_size + spacing),
                inv_y + row * (slot_size + spacing) + y_offset,
                slot_size,
                slot_size
            };
            
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &slot_rect)) {
                selected_inv_slot = i;
                printf("Selected inventory slot %d\n", i);
                return;
            }
        }
        
        // Check action buttons
        SDL_Rect btn_use = {win.x + 110, win.y + win.h - 40, 80, 30};
        SDL_Rect btn_drop = {win.x + 200, win.y + win.h - 40, 80, 30};
        SDL_Rect btn_equip = {win.x + 290, win.y + win.h - 40, 90, 30};
        
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_use)) {
            if (selected_inv_slot >= 0 && selected_inv_slot < my_inventory_count) {
                // Only allow using consumables
                if (my_inventory[selected_inv_slot].item.type == ITEM_TYPE_CONSUMABLE) {
                    request_item_use(selected_inv_slot);
                    printf("Using item in slot %d\n", selected_inv_slot);
                }
            }
            return;
        }
        
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_drop)) {
            if (selected_inv_slot >= 0 && selected_inv_slot < my_inventory_count) {
                request_item_drop(selected_inv_slot);
                printf("Dropping item in slot %d\n", selected_inv_slot);
            }
            return;
        }
        
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_equip)) {
            if (selected_inv_slot >= 0 && selected_inv_slot < my_inventory_count) {
                // Determine equipment slot based on item type and name
                int equip_slot = -1;
                int item_type = my_inventory[selected_inv_slot].item.type;
                const char *item_name = my_inventory[selected_inv_slot].item.name;
                
                if (item_type == ITEM_TYPE_WEAPON) {
                    equip_slot = EQUIP_SLOT_WEAPON;
                } else if (item_type == ITEM_TYPE_ARMOR) {
                    // Check item name to determine specific armor slot
                    if (strstr(item_name, "Helmet")) equip_slot = EQUIP_SLOT_HELMET;
                    else if (strstr(item_name, "Armor") || strstr(item_name, "Chest")) equip_slot = EQUIP_SLOT_CHEST;
                    else if (strstr(item_name, "Leg") || strstr(item_name, "Pants")) equip_slot = EQUIP_SLOT_LEGS;
                    else if (strstr(item_name, "Boot") || strstr(item_name, "Shoe")) equip_slot = EQUIP_SLOT_BOOTS;
                    else equip_slot = EQUIP_SLOT_CHEST; // Default to chest
                } else if (item_type == ITEM_TYPE_ACCESSORY) {
                    equip_slot = EQUIP_SLOT_ACCESSORY;
                }
                
                if (equip_slot >= 0) {
                    request_item_equip(selected_inv_slot, equip_slot);
                    printf("Equipping item in slot %d to equipment slot %d\n", selected_inv_slot, equip_slot);
                }
            }
            return;
        }
        
        return; // Don't process other clicks if inventory is open
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
            active_field = 25; SDL_StartTextInput(); active_input_rect = input;
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
        SDL_Rect win = {w/2 - 225, h/2 - 250, 450, 500};
        SDL_Rect btn_close = {win.x + win.w - 40, win.y + 5, 30, 30};
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
                send_packet(&pkt); strcpy(auth_message, get_string(STR_CONNECTED_LOGGING_IN)); return;
            }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){pop.x+160, pop.y+240, 120, 30})) { show_nick_popup = 0; active_field = -1; return; }
            return;
        }
        
        // Password Change Popup
        if (show_password_popup) {
            SDL_Rect pop = {w/2 - 175, h/2 - 200, 350, 400};
            int py = pop.y + 60;
            
            // Current password field
            SDL_Rect r_curr = {pop.x+20, py+20, 240, 25};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &r_curr)) {
                active_field = FIELD_PASSWORD_CURRENT; SDL_StartTextInput(); active_input_rect = r_curr;
                cursor_pos = get_cursor_pos_from_click(password_current, mx, r_curr.x);
                selection_start = cursor_pos; selection_len = 0; is_dragging = 1; return;
            }
            // Show checkbox for current
            SDL_Rect btn_show_curr = {pop.x + 270, py + 25, 15, 15};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_show_curr)) { show_password_current = !show_password_current; return; }
            
            py += 70;
            // New password field
            SDL_Rect r_new = {pop.x+20, py+20, 240, 25};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &r_new)) {
                active_field = FIELD_PASSWORD_NEW; SDL_StartTextInput(); active_input_rect = r_new;
                cursor_pos = get_cursor_pos_from_click(password_new, mx, r_new.x);
                selection_start = cursor_pos; selection_len = 0; is_dragging = 1; return;
            }
            // Show checkbox for new
            SDL_Rect btn_show_new = {pop.x + 270, py + 25, 15, 15};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_show_new)) { show_password_new = !show_password_new; return; }
            
            py += 70;
            // Confirm password field
            SDL_Rect r_conf = {pop.x+20, py+20, 240, 25};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &r_conf)) {
                active_field = FIELD_PASSWORD_CONFIRM; SDL_StartTextInput(); active_input_rect = r_conf;
                cursor_pos = get_cursor_pos_from_click(password_confirm, mx, r_conf.x);
                selection_start = cursor_pos; selection_len = 0; is_dragging = 1; return;
            }
            // Show checkbox for confirm
            SDL_Rect btn_show_conf = {pop.x + 270, py + 25, 15, 15};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_show_conf)) { show_password_confirm = !show_password_confirm; return; }
            
            // Submit button
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){pop.x+20, pop.y+340, 150, 30})) {
                // Validate locally first
                if (strlen(password_current) == 0) {
                    strcpy(password_message, get_string(STR_ERROR_CURRENT_PASSWORD_REQUIRED));
                } else if (strlen(password_new) < 8) {
                    strcpy(password_message, get_string(STR_ERROR_PASSWORD_TOO_SHORT_8));
                } else if (strcmp(password_new, password_confirm) != 0) {
                    strcpy(password_message, get_string(STR_ERROR_PASSWORDS_DONT_MATCH));
                } else {
                    // Send request to server
                    Packet pkt;
                    pkt.type = PACKET_CHANGE_PASSWORD_REQUEST;
                    strncpy(pkt.password, password_current, 63);
                    pkt.password[63] = '\0';
                    strncpy(pkt.username, password_new, 63);
                    pkt.username[63] = '\0';
                    strncpy(pkt.msg, password_confirm, 63);
                    pkt.msg[63] = '\0';
                    send_packet(&pkt);
                    strcpy(password_message, get_string(STR_PROCESSING));
                }
                return;
            }
            // Cancel button
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &(SDL_Rect){pop.x+180, pop.y+340, 150, 30})) {
                show_password_popup = 0; active_field = -1; return;
            }
            return;
        }

        // Main Settings Scrolled
        SDL_Rect s_win = {w/2 - 175, h/2 - 300, 350, 600}; 
        SDL_Rect viewport = {s_win.x + 10, s_win.y + 40, s_win.w - 20, s_win.h - 50};

        if (SDL_PointInRect(&(SDL_Point){mx, my}, &viewport)) {
            // Toggles
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_toggle_debug)) { 
                show_debug_info = !show_debug_info; 
                save_config(); return; 
            }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_toggle_fps)) { show_fps = !show_fps; save_config(); return; }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_toggle_coords)) { show_coords = !show_coords; save_config(); return; }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_toggle_unread)) { show_unread_counter = !show_unread_counter; save_config(); return; }
            #ifndef __ANDROID__
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_toggle_vulkan)) { config_use_vulkan = !config_use_vulkan; save_config(); return; }
            #endif
            #if !defined(_WIN32) && !defined(__APPLE__) && !defined(__ANDROID__)
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_toggle_nvidia_gpu)) { config_use_nvidia_gpu = !config_use_nvidia_gpu; save_config(); return; }
            #endif

            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_view_blocked)) { show_blocked_list = 1; return; } 
            
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_cycle_status)) {
                int my_status = 0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].id == local_player_id) my_status = local_players[i].status;
                my_status++; if (my_status > 4) my_status = 0; 
                Packet pkt; pkt.type = PACKET_STATUS_CHANGE; pkt.new_status = my_status; send_packet(&pkt); return;
            }

            SDL_Rect btn_nick = {btn_cycle_status.x, btn_cycle_status.y + 40, 300, 30};
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_nick)) {
                show_nick_popup = 1; nick_new[0] = 0; nick_confirm[0] = 0; nick_pass[0] = 0; strcpy(auth_message, get_string(STR_ENTER_CREDENTIALS)); return;
            }

            // Avatar change button (right after nickname button)
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_change_avatar)) {
                open_file_picker_for_avatar();
                return;
            }

            // Password change button
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_change_password)) {
                show_password_popup = 1;
                password_current[0] = 0; password_new[0] = 0; password_confirm[0] = 0;
                strcpy(password_message, "");
                return;
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
                strcpy(auth_message, get_string(STR_ENTER_CREDENTIALS)); return;
            }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_documentation_rect)) { show_documentation = 1; return; }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_contributors_rect)) { show_contributors = 1; return; }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_staff_list_rect)) { 
                show_role_list = 1; staff_count = 0; Packet req; req.type = PACKET_ROLE_LIST_REQUEST; send_packet(&req); return; 
            }
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_my_warnings)) {
                show_my_warnings = 1; my_warning_count = 0; Packet req; req.type = PACKET_WARNINGS_REQUEST; send_packet(&req); return;
            }
            
            // Language cycle button
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_cycle_language)) {
                current_language = (current_language + 1) % GAME_LANG_COUNT;
                save_config();
                return;
            }
        }
        
        // Close settings if clicked outside the window
        if (!SDL_PointInRect(&(SDL_Point){mx, my}, &s_win)) {
            is_settings_open = 0;
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

    // Check for player clicks in game world
    // mx, my are in UI coordinate space (divided by ui_scale)
    // We need to convert UI coordinates to game world coordinates:
    // 1. Multiply by ui_scale to get screen pixels
    // 2. Divide by game_zoom to account for zoom scaling
    // 3. Add camera offset to get world coordinates
    int game_mx = (int)((mx * ui_scale) / game_zoom) + cam_x;
    int game_my = (int)((my * ui_scale) / game_zoom) + cam_y;
    
    int clicked = 0;
    
    // Get player position for proximity check
    float player_x = 0, player_y = 0;
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(local_players[i].active && local_players[i].id == local_player_id) {
            player_x = local_players[i].x + PLAYER_WIDTH/2;
            player_y = local_players[i].y + PLAYER_HEIGHT/2;
            break;
        }
    }
    
    // Check NPCs first (higher priority)
    for (int i = 0; i < client_npc_count; i++) {
        if (strcmp(client_npcs[i].map_name, current_map_file) == 0) {
            SDL_Rect r = { (int)client_npcs[i].x, (int)client_npcs[i].y, PLAYER_WIDTH, PLAYER_HEIGHT };
            if (SDL_PointInRect(&(SDL_Point){game_mx, game_my}, &r)) {
                // Check if player is within interaction range
                float npc_center_x = client_npcs[i].x + PLAYER_WIDTH/2;
                float npc_center_y = client_npcs[i].y + PLAYER_HEIGHT/2;
                float dx = player_x - npc_center_x;
                float dy = player_y - npc_center_y;
                float distance = sqrtf(dx*dx + dy*dy);
                
                if (distance <= NPC_INTERACTION_RADIUS) {
                    selected_npc_id = client_npcs[i].npc_id;
                    selected_player_id = -1;
                    
                    // Send interaction packet to server
                    Packet pkt;
                    memset(&pkt, 0, sizeof(Packet));
                    pkt.type = PACKET_NPC_INTERACT;
                    pkt.npc_id = client_npcs[i].npc_id;
                    send_packet(&pkt);
                    
                    printf("Interacting with NPC: %s (ID: %d, Type: %d) - Distance: %.1f\n", 
                        client_npcs[i].name, client_npcs[i].npc_id, client_npcs[i].npc_type, distance);
                } else {
                    printf("Too far from NPC %s (Distance: %.1f, Required: %d)\n", 
                        client_npcs[i].name, distance, NPC_INTERACTION_RADIUS);
                }
                
                clicked = 1;
                return;
            }
        }
    }
    
    // Check players
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (local_players[i].active) {
            // Player rect in world coordinates (no camera offset)
            SDL_Rect r = { (int)local_players[i].x, (int)local_players[i].y, PLAYER_WIDTH, PLAYER_HEIGHT };
            if (SDL_PointInRect(&(SDL_Point){game_mx, game_my}, &r)) { 
                selected_player_id = local_players[i].id; 
                selected_npc_id = -1;
                clicked = 1; 
            }
        }
    }
    
    if (!clicked && !SDL_PointInRect(&(SDL_Point){mx, my}, &profile_win) && !SDL_PointInRect(&(SDL_Point){mx, my}, &btn_chat_toggle) && !SDL_PointInRect(&(SDL_Point){mx, my}, &btn_settings_toggle) && !SDL_PointInRect(&(SDL_Point){mx, my}, &btn_view_friends)) {
        selected_player_id = -1;
        selected_npc_id = -1;
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
        strcpy(auth_message, get_string(STR_CONNECTED_LOGGING_IN));
    } 
    else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_register)) {
        if (!is_connected) { if(!try_connect()) return; } 
        Packet pkt; pkt.type = PACKET_REGISTER_REQUEST; 
        strncpy(pkt.username, auth_username, 31); strncpy(pkt.password, auth_password, 31); 
        send_packet(&pkt);
        strcpy(auth_message, get_string(STR_CONNECTED_LOGGING_IN));
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
    
    // Add startup message to verify logging works
    ALOG("=== C MMO Client Starting ===\n");
    ALOG("Build info: %s %s\n", __DATE__, __TIME__);
    #ifdef __ANDROID__
    ALOG("Platform: Android\n");
    #endif
    
    // Load config early to get rendering backend preference and GPU settings
    // This must happen before any SDL initialization
    load_config();
    
    // Initialize auth message with localized text
    strcpy(auth_message, get_string(STR_ENTER_CREDENTIALS));
    
    // Parse command line arguments for rendering backend (overrides config)
    #ifdef USE_VULKAN
    ALOG("Vulkan support compiled in (USE_VULKAN defined)\n");
    ALOG("config_use_vulkan = %d, use_vulkan = %d\n", config_use_vulkan, use_vulkan);
    
    #ifdef __ANDROID__
    // Disable Vulkan on Android due to SDL2 limitations
    // SDL's Vulkan renderer backend is not functional on Android despite being compiled
    if (config_use_vulkan) {
        ALOG("Android: Vulkan is disabled on Android due to SDL2 limitations\n");
        config_use_vulkan = 0;
        use_vulkan = 0;
    }
    #else
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--vulkan") == 0 || strcmp(argv[i], "-vk") == 0) {
            use_vulkan = 1;
            render_backend = RENDER_BACKEND_VULKAN;
            backend_set_by_cmdline = 1;
            ALOG("Vulkan rendering backend requested via command line\n");
        }
    }
    
    // If not set by command line, use config setting
    if (!backend_set_by_cmdline && config_use_vulkan) {
        use_vulkan = 1;
        render_backend = RENDER_BACKEND_VULKAN;
        ALOG("Vulkan rendering backend enabled from config\n");
    }
    #endif
    
    // On Android, provide clear status message
    #ifdef __ANDROID__
    ALOG("Android: OpenGL ES renderer will be used (Vulkan disabled on Android)\n");
    #else
    if (use_vulkan) {
        ALOG("Vulkan rendering backend will be used\n");
    } else {
        ALOG("OpenGL renderer will be used\n");
    }
    #endif
    #else
    ALOG("Vulkan support not compiled (USE_VULKAN not defined)\n");
    #endif
    
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
    
    // Configure NVIDIA Prime GPU selection on Linux if enabled in config
    #if !defined(_WIN32) && !defined(__APPLE__) && !defined(__ANDROID__)
    if (config_use_nvidia_gpu) {
        if (setenv("__NV_PRIME_RENDER_OFFLOAD", "1", 1) != 0) {
            printf("Warning: Failed to set __NV_PRIME_RENDER_OFFLOAD environment variable\n");
        }
        if (setenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia", 1) != 0) {
            printf("Warning: Failed to set __GLX_VENDOR_LIBRARY_NAME environment variable\n");
        }
        printf("NVIDIA GPU selection enabled from config\n");
        // Request accelerated visual for proper GPU selection (works for both OpenGL and Vulkan)
        SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    }
    #endif
    
    #ifdef SDL_HINT_WINDOWS_RAWKEYBOARD
    SDL_SetHint(SDL_HINT_WINDOWS_RAWKEYBOARD, "1");
    #endif
    
    // Set SDL hint to prefer Vulkan renderer if requested (MUST be before SDL_Init)
    #ifdef USE_VULKAN
    ALOG("About to check use_vulkan flag, value=%d\n", use_vulkan);
    if (use_vulkan) {
        // Tell SDL to use Vulkan renderer backend
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "vulkan");
        ALOG("Setting SDL to use Vulkan renderer\n");
        
        // Explicitly disable VSync for Vulkan to prevent FPS drops and ping spikes
        // SDL's Vulkan renderer may have its own VSync behavior that needs to be disabled
        SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
        ALOG("Disabling VSync for Vulkan renderer\n");
        
        // Handle NVIDIA PRIME GPU selection for Vulkan on Linux
        #if !defined(_WIN32) && !defined(__APPLE__) && !defined(__ANDROID__)
        const char *nv_offload = getenv("__NV_PRIME_RENDER_OFFLOAD");
        const char *glx_vendor = getenv("__GLX_VENDOR_LIBRARY_NAME");
        if (nv_offload && glx_vendor && strcmp(nv_offload, "1") == 0 && strcmp(glx_vendor, "nvidia") == 0) {
            // When NVIDIA PRIME is requested, set Vulkan-specific environment variables
            // This ensures Vulkan uses the NVIDIA GPU instead of Intel iGPU
            setenv("__NV_PRIME_RENDER_OFFLOAD_PROVIDER", "NVIDIA-G0", 0);
            setenv("__VK_LAYER_NV_optimus", "NVIDIA_only", 0);
            printf("NVIDIA PRIME detected - configuring Vulkan to use NVIDIA GPU\n");
        }
        #endif
    } else {
        // Enable adaptive VSync for non-Vulkan backends
        SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
        ALOG("Vulkan not enabled, using default renderer with VSync\n");
    }
    #else
    // Enable adaptive VSync if available (reduces latency while preventing tearing)
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    ALOG("Vulkan support not compiled, using default renderer\n");
    #endif
    
    // On mobile, disable touch-to-mouse event synthesis to prevent double event handling
    // Without this, each touch generates BOTH a FINGERDOWN AND a MOUSEBUTTONDOWN event,
    // which causes issues like selection being cleared after menu button handling
    #if defined(__ANDROID__) || defined(__IPHONEOS__)
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    ALOG("Disabled touch-to-mouse event synthesis for mobile\n");
    #endif
    
    ALOG("About to call SDL_Init\n");
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
    
    // Load font at scaled size based on loaded config
    // ui_scale was loaded from config earlier via load_config()
    int scaled_font_size = calculate_scaled_font_size();
    
    font = TTF_OpenFont(FONT_PATH, scaled_font_size);
    if (!font) { 
        printf("Font missing: %s (tried size %d)\n", FONT_PATH, scaled_font_size); 
        return 1; 
    }
    
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
    
    // Note: SDL_WINDOW_VULKAN is NOT used here because we're using SDL_Renderer API
    // SDL_Renderer with Vulkan backend is controlled via SDL_HINT_RENDER_DRIVER hint (set above)
    // SDL_WINDOW_VULKAN is only for direct Vulkan usage without SDL_Renderer

    SDL_Window *window = SDL_CreateWindow("C MMO Client", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h, win_flags);
    if (!window) { ALOG("Window creation failed: %s\n", SDL_GetError()); return 1; }
    
    ALOG("SDL Window created successfully\n");
    
    // Create SDL_Renderer for game content rendering
    // SDL will use the hinted backend (Vulkan if requested) or auto-select best available
    SDL_Renderer *renderer = NULL;
    #if defined(__APPLE__) && !defined(__IPHONEOS__)
    // Small delay to allow macOS to properly initialize window compositing
    SDL_Delay(100);
    // Use software renderer for compatibility with older hardware on macOS
    ALOG("Creating software renderer for macOS\n");
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    #else
    // Use accelerated renderer without forcing VSync
    // VSync can be controlled via SDL_HINT_RENDER_VSYNC if desired
    // Not forcing VSync prevents frame drops when UI windows are open
    ALOG("Creating accelerated renderer\n");
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    #endif
    
    if (!renderer) { 
        ALOG("Renderer creation failed: %s\n", SDL_GetError()); 
        #ifdef USE_VULKAN
        if (use_vulkan) {
            ALOG("Vulkan renderer not available, falling back to OpenGL\n");
            // Clear the hint and try again with default renderer
            SDL_SetHint(SDL_HINT_RENDER_DRIVER, NULL);
            use_vulkan = 0;
            render_backend = RENDER_BACKEND_OPENGL;
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        }
        #endif
        if (!renderer) {
            ALOG("Failed to create any renderer\n");
            return 1;
        }
    }
    
    // Check which rendering backend SDL chose
    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(renderer, &info) == 0) {
        ALOG("SDL Renderer backend: %s\n", info.name);
        
        #ifdef USE_VULKAN
        // Verify backend matches user request
        int is_vulkan = (strstr(info.name, "vulkan") || strstr(info.name, "Vulkan")) ? 1 : 0;
        
        if (use_vulkan && !is_vulkan) {
            ALOG("Note: Vulkan requested but SDL chose %s - Will use custom Vulkan renderer\n", info.name);
            // Keep render_backend as VULKAN - we use custom Vulkan renderer, not SDL's
        } else if (is_vulkan) {
            ALOG("Vulkan rendering active through SDL\n");
            
            // Probe Vulkan device name if not already done
            if (!vk_probe_done) {
                // Get Vulkan instance and enumerate physical devices
                VkInstance instance = VK_NULL_HANDLE;
                
                // Get required extensions for Vulkan instance
                unsigned int ext_count = 0;
                if (SDL_Vulkan_GetInstanceExtensions(window, &ext_count, NULL)) {
                    const char **extensions = malloc(sizeof(char*) * ext_count);
                    if (extensions && SDL_Vulkan_GetInstanceExtensions(window, &ext_count, extensions)) {
                        // Create minimal Vulkan instance for probing
                        VkApplicationInfo app_info = {0};
                        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
                        app_info.pApplicationName = "MMO Client";
                        app_info.apiVersion = VK_API_VERSION_1_0;
                        
                        VkInstanceCreateInfo create_info = {0};
                        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
                        create_info.pApplicationInfo = &app_info;
                        create_info.enabledExtensionCount = ext_count;
                        create_info.ppEnabledExtensionNames = extensions;
                        
                        if (vkCreateInstance(&create_info, NULL, &instance) == VK_SUCCESS) {
                            // Enumerate physical devices
                            uint32_t device_count = 0;
                            vkEnumeratePhysicalDevices(instance, &device_count, NULL);
                            if (device_count > 0) {
                                VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * device_count);
                                if (devices) {
                                    vkEnumeratePhysicalDevices(instance, &device_count, devices);
                                    // Get properties of first device (or match based on NVIDIA if PRIME is set)
                                    VkPhysicalDeviceProperties props;
                                    int selected_device = 0;
                                    
                                    // If NVIDIA PRIME is set, try to find NVIDIA device
                                    #if !defined(_WIN32) && !defined(__APPLE__)
                                    const char *nv_offload = getenv("__NV_PRIME_RENDER_OFFLOAD");
                                    if (nv_offload && strcmp(nv_offload, "1") == 0) {
                                        for (uint32_t i = 0; i < device_count; i++) {
                                            vkGetPhysicalDeviceProperties(devices[i], &props);
                                            if (props.vendorID == 0x10DE) { // NVIDIA vendor ID
                                                selected_device = i;
                                                break;
                                            }
                                        }
                                    }
                                    #endif
                                    
                                    vkGetPhysicalDeviceProperties(devices[selected_device], &props);
                                    strncpy(vk_device_name, props.deviceName, sizeof(vk_device_name) - 1);
                                    vk_device_name[sizeof(vk_device_name) - 1] = '\0';
                                    printf("Vulkan device: %s\n", vk_device_name);
                                    free(devices);
                                }
                            }
                            vkDestroyInstance(instance, NULL);
                        }
                    }
                    free(extensions);
                }
                vk_probe_done = 1;
            }
        } else {
            render_backend = RENDER_BACKEND_OPENGL;
            printf("OpenGL/GLES rendering active through SDL\n");
        }
        #endif
    }
    
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
    // Config already loaded earlier to get backend preference
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
            else if(active_field==FIELD_PASSWORD_CURRENT) len=strlen(password_current);
            else if(active_field==FIELD_PASSWORD_NEW) len=strlen(password_new);
            else if(active_field==FIELD_PASSWORD_CONFIRM) len=strlen(password_confirm);
            else if(active_field==FIELD_FRIEND_ID) len=strlen(input_friend_id);
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
                
                // Check overlay windows first (higher priority)
                if (show_documentation) {
                    documentation_scroll -= scroll_amount;
                    if (documentation_scroll < 0) documentation_scroll = 0;
                    // Documentation window: 450x500, content height 650px, visible area 440px (500 - 60 for header)
                    int content_height = 650;
                    int visible_height = 440;
                    int max_scroll = content_height - visible_height;
                    if (max_scroll < 0) max_scroll = 0;
                    if (documentation_scroll > max_scroll) documentation_scroll = max_scroll;
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
                else if (show_role_list) {
                    role_list_scroll -= scroll_amount;
                    if (role_list_scroll < 0) role_list_scroll = 0;
                    // Max scroll based on staff count (25px per staff + 50px header)
                    int content_height = 50 + (staff_count * 25);
                    int visible_height = 450 - 60; // Window height minus header
                    int max_scroll = content_height - visible_height;
                    if (max_scroll < 0) max_scroll = 0;
                    if (role_list_scroll > max_scroll) role_list_scroll = max_scroll;
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
                else if (is_chat_open) {
                    chat_scroll += scroll_amount; // Note: inverted - scroll up increases scroll value to see older messages
                    if (chat_scroll < 0) chat_scroll = 0;
                    // Chat window: 100 messages * 15px = 1500px total, show last 10 (150px visible)
                    int total_content = CHAT_HISTORY * 15;
                    int visible_content = CHAT_VISIBLE_LINES * 15;
                    int max_scroll = total_content - visible_content;
                    if (max_scroll < 0) max_scroll = 0;
                    if (chat_scroll > max_scroll) chat_scroll = max_scroll;
                }
                else if (is_settings_open) {
                    settings_scroll_y -= scroll_amount; 
                    if (settings_scroll_y < 0) settings_scroll_y = 0;
                    
                    int max_scroll = settings_content_h - settings_view_port.h;
                    if (max_scroll < 0) max_scroll = 0;
                    if (settings_scroll_y > max_scroll) settings_scroll_y = max_scroll;
                }
                else if (show_inventory) {
                    inventory_scroll -= scroll_amount;
                    if (inventory_scroll < 0) inventory_scroll = 0;
                    // Calculate max scroll based on number of inventory rows
                    int slots_per_row = 5;
                    int rows = (MAX_INVENTORY_SLOTS + slots_per_row - 1) / slots_per_row;
                    int slot_size = 60;
                    int spacing = 5;
                    int content_height = rows * (slot_size + spacing);
                    int visible_height = 450; // Inventory window visible area
                    int max_scroll = content_height - visible_height;
                    if (max_scroll < 0) max_scroll = 0;
                    if (inventory_scroll > max_scroll) inventory_scroll = max_scroll;
                }
            }
            else if (event.type == SDL_FINGERDOWN) {
                int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
                // Convert touch coordinates to UI space
                int tx = (int)((event.tfinger.x * w) / ui_scale);
                int ty = (int)((event.tfinger.y * h) / ui_scale);

                #if defined(__ANDROID__) || defined(__IPHONEOS__)
                // Check if clicking on mobile text menu
                if (show_mobile_text_menu && SDL_PointInRect(&(SDL_Point){tx, ty}, &mobile_text_menu_rect)) {
                    // Handle menu button clicks
                    int button_w = 65;
                    int button_spacing = 5;
                    int x = mobile_text_menu_rect.x + button_spacing;
                    int y = mobile_text_menu_rect.y + 5;
                    int button_h = 40;
                    
                    SDL_Rect btn_select_all = {x, y, button_w, button_h};
                    SDL_Rect btn_cut = {x + button_w + button_spacing, y, button_w, button_h};
                    SDL_Rect btn_copy = {x + (button_w + button_spacing) * 2, y, button_w, button_h};
                    SDL_Rect btn_paste = {x + (button_w + button_spacing) * 3, y, button_w, button_h};
                    SDL_Rect btn_clear = {x + (button_w + button_spacing) * 4, y, button_w, button_h};
                    
                    if (SDL_PointInRect(&(SDL_Point){tx, ty}, &btn_select_all)) {
                        // Select All: Select all text in the active field
                        if (is_chat_open || active_field >= 0) {
                            char *buffer = get_active_field_buffer(NULL);
                            if (buffer) {
                                int len = strlen(buffer);
                                cursor_pos = len;
                                selection_start = 0;
                                selection_len = len;
                            }
                        }
                    } else if (SDL_PointInRect(&(SDL_Point){tx, ty}, &btn_cut)) {
                        // Cut: Copy and delete
                        if (selection_len != 0 && (is_chat_open || active_field >= 0)) {
                            char *buffer = get_active_field_buffer(NULL);
                            if (buffer) {
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
                                delete_selection(buffer);
                            }
                        }
                    } else if (SDL_PointInRect(&(SDL_Point){tx, ty}, &btn_copy)) {
                        // Copy
                        if (selection_len != 0 && (is_chat_open || active_field >= 0)) {
                            char *buffer = get_active_field_buffer(NULL);
                            if (buffer) {
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
                    } else if (SDL_PointInRect(&(SDL_Point){tx, ty}, &btn_paste)) {
                        // Paste
                        if (SDL_HasClipboardText() && (is_chat_open || active_field >= 0)) {
                            char *text = SDL_GetClipboardText();
                            if (text) {
                                int max_len = 0;
                                char *buffer = get_active_field_buffer(&max_len);
                                
                                if (buffer) {
                                    if (selection_len != 0) delete_selection(buffer);
                                    
                                    int add_len = strlen(text);
                                    if (strlen(buffer) + add_len <= max_len) {
                                        memmove(buffer + cursor_pos + add_len, buffer + cursor_pos, strlen(buffer) - cursor_pos + 1);
                                        memcpy(buffer + cursor_pos, text, add_len);
                                        cursor_pos += add_len;
                                    }
                                }
                                SDL_free(text);
                            }
                        }
                    } else if (SDL_PointInRect(&(SDL_Point){tx, ty}, &btn_clear)) {
                        // Clear selection or all text
                        if (is_chat_open || active_field >= 0) {
                            char *buffer = get_active_field_buffer(NULL);
                            if (buffer) {
                                if (selection_len != 0) {
                                    delete_selection(buffer);
                                } else {
                                    buffer[0] = 0;
                                    cursor_pos = 0;
                                    selection_start = 0;
                                    selection_len = 0;
                                }
                            }
                        }
                    }
                    show_mobile_text_menu = 0;
                    continue;
                }
                
                // Track if menu was visible before potentially closing it
                // This is used later to preserve selection when tapping on input while menu was visible
                int menu_was_visible = show_mobile_text_menu;
                
                // Close text menu if touching outside it
                if (show_mobile_text_menu && !SDL_PointInRect(&(SDL_Point){tx, ty}, &mobile_text_menu_rect)) {
                    show_mobile_text_menu = 0;
                }
                
                // Start long press detection for text fields
                if (is_chat_open || active_field >= 0) {
                    long_press_start_time = SDL_GetTicks();
                    long_press_active = 1;
                    long_press_start_x = tx;
                    long_press_start_y = ty;
                }
                #endif

                // Touch scrolling support for all scrollable windows
                // Check each window independently to see if touch is within its bounds
                // NOTE: Scroll detection doesn't block button clicks - both can happen
                // Actual scrolling happens on FINGERMOTION
                int scaled_w = w / ui_scale;
                int scaled_h = h / ui_scale;
                
                if (show_documentation) {
                    SDL_Rect win = {scaled_w/2 - 225, scaled_h/2 - 250, 450, 500};
                    SDL_Rect content_area = {win.x + 10, win.y + 50, win.w - 20, win.h - 60};
                    if (SDL_PointInRect(&(SDL_Point){tx, ty}, &content_area)) {
                        scroll_touch_id = event.tfinger.fingerId;
                        scroll_last_y = ty;
                        scroll_start_x = tx;
                        scroll_start_y = ty;
                        scroll_moved = 0;
                        scroll_window_id = 1; // Documentation
                    }
                }
                if (show_contributors) {
                    SDL_Rect win = {scaled_w/2 - 200, scaled_h/2 - 200, 400, 400};
                    SDL_Rect content_area = {win.x + 10, win.y + 50, win.w - 20, win.h - 60};
                    if (SDL_PointInRect(&(SDL_Point){tx, ty}, &content_area)) {
                        scroll_touch_id = event.tfinger.fingerId;
                        scroll_last_y = ty;
                        scroll_start_x = tx;
                        scroll_start_y = ty;
                        scroll_moved = 0;
                        scroll_window_id = 2; // Contributors
                    }
                }
                if (show_role_list) {
                    SDL_Rect win = {scaled_w/2 - 200, scaled_h/2 - 225, 400, 450};
                    SDL_Rect content_area = {win.x + 10, win.y + 50, win.w - 20, win.h - 60};
                    if (SDL_PointInRect(&(SDL_Point){tx, ty}, &content_area)) {
                        scroll_touch_id = event.tfinger.fingerId;
                        scroll_last_y = ty;
                        scroll_start_x = tx;
                        scroll_start_y = ty;
                        scroll_moved = 0;
                        scroll_window_id = 6; // Role list
                    }
                }
                if (show_friend_list) {
                    SDL_Rect win = {scaled_w/2 - 200, scaled_h/2 - 200, 400, 400};
                    SDL_Rect content_area = {win.x + 10, win.y + 80, win.w - 20, win.h - 85};
                    if (SDL_PointInRect(&(SDL_Point){tx, ty}, &content_area)) {
                        scroll_touch_id = event.tfinger.fingerId;
                        scroll_last_y = ty;
                        scroll_start_x = tx;
                        scroll_start_y = ty;
                        scroll_moved = 0;
                        scroll_window_id = 3; // Friend list
                    }
                }
                if (is_inbox_open) {
                    SDL_Rect win = {scaled_w/2 - 150, scaled_h/2 - 150, 300, 300};
                    SDL_Rect content_area = {win.x + 10, win.y + 35, win.w - 20, win.h - 40};
                    if (SDL_PointInRect(&(SDL_Point){tx, ty}, &content_area)) {
                        scroll_touch_id = event.tfinger.fingerId;
                        scroll_last_y = ty;
                        scroll_start_x = tx;
                        scroll_start_y = ty;
                        scroll_moved = 0;
                        scroll_window_id = 4; // Inbox
                    }
                }
                if (show_my_warnings) {
                    SDL_Rect win = {scaled_w/2 - 200, scaled_h/2 - 200, 400, 400};
                    SDL_Rect content_area = {win.x + 10, win.y + 45, win.w - 20, win.h - 50};
                    if (SDL_PointInRect(&(SDL_Point){tx, ty}, &content_area)) {
                        scroll_touch_id = event.tfinger.fingerId;
                        scroll_last_y = ty;
                        scroll_start_x = tx;
                        scroll_start_y = ty;
                        scroll_moved = 0;
                        scroll_window_id = 5; // Warnings
                    }
                }
                if (show_blocked_list) {
                    SDL_Rect win = {scaled_w/2 - 150, scaled_h/2 - 200, 300, 400};
                    SDL_Rect content_area = {win.x + 10, win.y + 45, win.w - 20, win.h - 50};
                    if (SDL_PointInRect(&(SDL_Point){tx, ty}, &content_area)) {
                        scroll_touch_id = event.tfinger.fingerId;
                        scroll_last_y = ty;
                        scroll_start_x = tx;
                        scroll_start_y = ty;
                        scroll_moved = 0;
                        scroll_window_id = 7; // Blocked list
                    }
                }
                // Don't allow settings scrolling if any popups/overlays are open over it
                // Also check for slider touches first - if touching a slider, don't enable scrolling
                if (is_settings_open && 
                    !show_nick_popup && !show_password_popup && 
                    !show_blocked_list && !show_add_friend_popup && 
                    !show_contributors && !show_documentation && 
                    !show_my_warnings && !show_role_list && 
                    SDL_PointInRect(&(SDL_Point){tx, ty}, &settings_view_port)) {
                    
                    // Check if this touch is on a slider - if so, don't enable scrolling
                    int is_slider_touch = 0;
                    SDL_Rect sliders[] = {slider_r, slider_g, slider_b, slider_r2, slider_g2, slider_b2, 
                                         slider_volume, slider_afk, slider_ui_scale, slider_game_zoom};
                    int margin = 15;
                    for (int i = 0; i < 10; i++) {
                        SDL_Rect touch_area = sliders[i];
                        touch_area.x -= margin; touch_area.y -= margin;
                        touch_area.w += margin*2; touch_area.h += margin*2;
                        if (SDL_PointInRect(&(SDL_Point){tx, ty}, &touch_area)) {
                            is_slider_touch = 1;
                            break;
                        }
                    }
                    
                    if (!is_slider_touch) {
                        scroll_touch_id = event.tfinger.fingerId;
                        scroll_last_y = ty;
                        scroll_start_x = tx;
                        scroll_start_y = ty;
                        scroll_moved = 0;
                        scroll_window_id = 0; // Settings
                    }
                }
                
                // Game-specific touch handling - process regardless of scroll detection
                if (client_state == STATE_GAME) {
                    // Use scaled height for UI coordinate calculations
                    int scaled_h = (int)(h / ui_scale);
                    int scaled_w = (int)(w / ui_scale);
                    
                    // PRIORITY 1: Check HUD buttons FIRST - they should always be accessible
                    if (SDL_PointInRect(&(SDL_Point){tx, ty}, &(SDL_Rect){10, scaled_h-40, 100, 30})) {
                        // Chat button
                        is_chat_open = !is_chat_open; 
                        if(is_chat_open) { 
                            unread_chat_count = 0; 
                            #if defined(__ANDROID__) || defined(__IPHONEOS__)
                            chat_input_active = 0;
                            #else
                            chat_input_active = 1;
                            SDL_StartTextInput();
                            #endif
                            // Send typing started
                            Packet typing_pkt;
                            memset(&typing_pkt, 0, sizeof(Packet));
                            typing_pkt.type = PACKET_TYPING;
                            typing_pkt.player_id = 1;
                            send_packet(&typing_pkt);
                            is_typing = 1;
                            last_typing_update = SDL_GetTicks();
                        } else { 
                            chat_input_active = 0; 
                            SDL_StopTextInput();
                            #if defined(__ANDROID__) || defined(__IPHONEOS__)
                            keyboard_height = 0;
                            #endif
                            // Send typing stopped
                            Packet typing_pkt;
                            memset(&typing_pkt, 0, sizeof(Packet));
                            typing_pkt.type = PACKET_TYPING;
                            typing_pkt.player_id = 0;
                            send_packet(&typing_pkt);
                            is_typing = 0;
                        }
                    }
                    else if (SDL_PointInRect(&(SDL_Point){tx, ty}, &(SDL_Rect){120, scaled_h-40, 100, 30})) {
                        // Friends button
                        show_friend_list = !show_friend_list;
                    }
                    else if (SDL_PointInRect(&(SDL_Point){tx, ty}, &(SDL_Rect){230, scaled_h-40, 100, 30})) {
                        // Settings button
                        is_settings_open = !is_settings_open;
                        if (!is_settings_open) { 
                            show_nick_popup = 0; 
                            show_password_popup = 0; 
                            show_blocked_list = 0; 
                            active_field = -1; 
                        } else {
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
                    else if (SDL_PointInRect(&(SDL_Point){tx, ty}, &(SDL_Rect){scaled_w - 50, 10, 40, 40})) {
                        // Inbox button
                        is_inbox_open = !is_inbox_open;
                        if (is_inbox_open) { 
                            show_friend_list = 0; 
                            show_add_friend_popup = 0; 
                            is_settings_open = 0; 
                            show_inventory = 0;
                        }
                    }
                    else if (SDL_PointInRect(&(SDL_Point){tx, ty}, &(SDL_Rect){scaled_w - 100, 10, 40, 40})) {
                        // Inventory button
                        show_inventory = !show_inventory;
                        if (show_inventory) { 
                            is_inbox_open = 0; 
                            show_friend_list = 0; 
                            show_add_friend_popup = 0; 
                            is_settings_open = 0; 
                        }
                    }
                    else if (is_chat_open) {
                        // Calculate chat window position with keyboard shift (same as rendering)
                        #if defined(__ANDROID__) || defined(__IPHONEOS__)
                        int chat_y = get_chat_window_y(scaled_h, ui_scale);
                        #else
                        int chat_y = scaled_h - 240;  // Fixed position on non-mobile platforms
                        #endif
                        
                        SDL_Rect chat_win = (SDL_Rect){10, chat_y, 300, 190};
                        SDL_Rect chat_input = (SDL_Rect){15, chat_win.y + chat_win.h - 24, 270, 24};
                        SDL_Rect chat_history = (SDL_Rect){chat_win.x, chat_win.y, chat_win.w, chat_win.h - 30}; // History area (exclude input)
                        
                        if (SDL_PointInRect(&(SDL_Point){tx, ty}, &chat_input)) {
                            #if defined(__ANDROID__) || defined(__IPHONEOS__)
                            // Track if the input was already active (for text operations)
                            int was_already_active = chat_input_active;
                            // Preserve selection if text menu was visible before this tap
                            // (menu_was_visible is captured before closing the menu at touch start)
                            int preserve_selection = menu_was_visible && selection_len != 0;
                            #else
                            int preserve_selection = 0;
                            #endif
                            
                            chat_input_active = 1;
                            SDL_StartTextInput();
                            
                            // Set up cursor position and text selection for dragging
                            int prefix_w = 0, ph;
                            char prefix[64];
                            if (chat_target_id != -1) {
                                char *name = "Unknown";
                                for(int i=0; i<MAX_CLIENTS; i++) {
                                    if(local_players[i].id == chat_target_id) name = local_players[i].username;
                                }
                                snprintf(prefix, 64, "To %s: ", name);
                            } else {
                                strcpy(prefix, "> ");
                            }
                            TTF_SizeText(font, prefix, &prefix_w, &ph);
                            active_input_rect = chat_input;
                            active_input_rect.x += (5 + prefix_w);
                            
                            // Only update cursor and selection if not preserving current selection
                            if (!preserve_selection) {
                                cursor_pos = get_cursor_pos_from_click(input_buffer, tx, active_input_rect.x);
                                selection_start = cursor_pos;
                                selection_len = 0;
                            }
                            is_dragging = 1;
                            
                            #if defined(__ANDROID__) || defined(__IPHONEOS__)
                            // Only trigger keyboard shift on initial activation, not when doing text operations
                            // If text menu is visible, don't shift - user is doing text formatting
                            // If input was already active, don't re-shift - user is selecting text
                            if (!show_mobile_text_menu && !was_already_active) {
                                // Estimate keyboard height as 40% of screen height on mobile
                                keyboard_height = h * 0.4;
                            }
                            #endif
                        } else if (SDL_PointInRect(&(SDL_Point){tx, ty}, &chat_history)) {
                            // Touch in chat history area - enable scrolling
                            scroll_touch_id = event.tfinger.fingerId;
                            scroll_last_y = ty;
                            scroll_window_id = 8; // Chat
                        } else if (!SDL_PointInRect(&(SDL_Point){tx, ty}, &chat_win)) {
                            chat_input_active = 0;
                            if (active_field < 0) SDL_StopTextInput();
                            #if defined(__ANDROID__) || defined(__IPHONEOS__)
                            keyboard_height = 0;
                            #endif
                        }
                    }
                    // Check if touching inside any overlay/popup window - prevents joystick creation
                    // Don't call handle_game_click here - wait for FINGERUP to avoid double-clicks
                    else if (show_friend_list || is_inbox_open || is_settings_open || show_sanction_popup || 
                             show_my_warnings || show_add_friend_popup || show_blocked_list ||
                             show_contributors || show_documentation || show_role_list || show_inventory) {
                        // Define overlay window rectangles (must match rendering positions)
                        int touching_overlay_window = 0;
                        
                        if (show_friend_list) {
                            SDL_Rect win = {scaled_w/2 - 200, scaled_h/2 - 200, 400, 400};
                            if (SDL_PointInRect(&(SDL_Point){tx, ty}, &win)) touching_overlay_window = 1;
                        }
                        if (is_inbox_open) {
                            SDL_Rect win = {scaled_w - 320, 60, 300, 300};
                            if (SDL_PointInRect(&(SDL_Point){tx, ty}, &win)) touching_overlay_window = 1;
                        }
                        if (is_settings_open) {
                            SDL_Rect win = {scaled_w/2 - 175, scaled_h/2 - 300, 350, 600};
                            if (SDL_PointInRect(&(SDL_Point){tx, ty}, &win)) touching_overlay_window = 1;
                            // Check settings sub-popups
                            if (show_nick_popup) {
                                SDL_Rect pop = {scaled_w/2 - 150, scaled_h/2 - 150, 300, 300};
                                if (SDL_PointInRect(&(SDL_Point){tx, ty}, &pop)) touching_overlay_window = 1;
                            }
                            if (show_password_popup) {
                                SDL_Rect pop = {scaled_w/2 - 175, scaled_h/2 - 200, 350, 400};
                                if (SDL_PointInRect(&(SDL_Point){tx, ty}, &pop)) touching_overlay_window = 1;
                            }
                        }
                        if (show_sanction_popup) {
                            SDL_Rect win = {scaled_w/2 - 150, scaled_h/2 - 150, 300, 300};
                            if (SDL_PointInRect(&(SDL_Point){tx, ty}, &win)) touching_overlay_window = 1;
                        }
                        if (show_my_warnings) {
                            SDL_Rect win = {scaled_w/2 - 200, scaled_h/2 - 200, 400, 400};
                            if (SDL_PointInRect(&(SDL_Point){tx, ty}, &win)) touching_overlay_window = 1;
                        }
                        if (show_add_friend_popup) {
                            SDL_Rect pop = {scaled_w/2 - 150, scaled_h/2 - 100, 300, 200};
                            if (SDL_PointInRect(&(SDL_Point){tx, ty}, &pop)) touching_overlay_window = 1;
                        }
                        if (show_blocked_list) {
                            SDL_Rect win = {scaled_w/2 - 150, scaled_h/2 - 200, 300, 400};
                            if (SDL_PointInRect(&(SDL_Point){tx, ty}, &win)) touching_overlay_window = 1;
                        }
                        if (show_contributors) {
                            SDL_Rect win = {scaled_w/2 - 200, scaled_h/2 - 225, 400, 450};
                            if (SDL_PointInRect(&(SDL_Point){tx, ty}, &win)) touching_overlay_window = 1;
                        }
                        if (show_documentation) {
                            SDL_Rect win = {scaled_w/2 - 225, scaled_h/2 - 250, 450, 500};
                            if (SDL_PointInRect(&(SDL_Point){tx, ty}, &win)) touching_overlay_window = 1;
                        }
                        if (show_role_list) {
                            SDL_Rect win = {scaled_w/2 - 200, scaled_h/2 - 225, 400, 450};
                            if (SDL_PointInRect(&(SDL_Point){tx, ty}, &win)) touching_overlay_window = 1;
                        }
                        if (show_inventory) {
                            SDL_Rect win = {scaled_w/2 - 225, scaled_h/2 - 275, 450, 550};
                            if (SDL_PointInRect(&(SDL_Point){tx, ty}, &win)) {
                                touching_overlay_window = 1;
                                // Setup inventory scrolling
                                scroll_touch_id = event.tfinger.fingerId;
                                scroll_last_y = ty;
                                scroll_start_x = tx;
                                scroll_start_y = ty;
                                scroll_moved = 0;
                                scroll_window_id = 9; // Inventory
                            }
                        }
                        
                        if (touching_overlay_window) {
                            // Touch is inside an overlay window - track it for FINGERUP handling
                            // But first check if it's on a slider in settings (and no popups are blocking)
                            int is_slider_touch = 0;
                            if (is_settings_open && !show_nick_popup && !show_password_popup && 
                                !show_blocked_list && !show_add_friend_popup && 
                                !show_contributors && !show_documentation && 
                                !show_my_warnings && !show_role_list) {
                                // Check if touching any slider (with expanded hit area)
                                SDL_Rect sliders[] = {slider_r, slider_g, slider_b, slider_r2, slider_g2, slider_b2, 
                                                     slider_volume, slider_afk, slider_ui_scale, slider_game_zoom};
                                int slider_ids[] = {SLIDER_R, SLIDER_G, SLIDER_B, SLIDER_R2, SLIDER_G2, SLIDER_B2,
                                                   SLIDER_VOL, SLIDER_AFK, SLIDER_UI_SCALE, SLIDER_GAME_ZOOM};
                                int margin = 15;
                                for (int i = 0; i < 10; i++) {
                                    SDL_Rect touch_area = sliders[i];
                                    touch_area.x -= margin; touch_area.y -= margin;
                                    touch_area.w += margin*2; touch_area.h += margin*2;
                                    if (SDL_PointInRect(&(SDL_Point){tx, ty}, &touch_area)) {
                                        active_slider = slider_ids[i];
                                        overlay_touch_id = event.tfinger.fingerId; // Track this finger for slider
                                        process_slider_drag(tx);
                                        is_slider_touch = 1;
                                        break;
                                    }
                                }
                            }
                            
                            if (!is_slider_touch) {
                                overlay_touch_id = event.tfinger.fingerId;
                                overlay_touch_start_x = tx;
                                overlay_touch_start_y = ty;
                            }
                        } else if (touch_id_dpad == -1) {
                            // Touch is outside overlay windows - allow joystick creation
                            dpad_rect = (SDL_Rect){tx - 75, ty - 75, 150, 150};
                            touch_id_dpad = event.tfinger.fingerId;
                            vjoy_dx = 0; vjoy_dy = 0;
                            joystick_active = 1;
                        } else {
                            // Already have joystick - will handle click on FINGERUP
                        }
                    }
                    else if (touch_id_dpad == -1) {
                        // Create joystick only in game state (and only if not touching UI buttons)
                        // dpad_rect is in UI space (renderer will apply ui_scale)
                        dpad_rect = (SDL_Rect){tx - 75, ty - 75, 150, 150};
                        touch_id_dpad = event.tfinger.fingerId;
                        vjoy_dx = 0; vjoy_dy = 0;
                        joystick_active = 1;
                    } else {
                        // Treat other touches as Mouse Clicks for UI (use scaled coordinates)
                        handle_game_click(tx, ty, 0, 0, scaled_w, scaled_h);
                    }
                }
            }
            else if (event.type == SDL_FINGERMOTION) {
                #if defined(__ANDROID__) || defined(__IPHONEOS__)
                // Cancel long press if finger moves too much (drag detection)
                if (long_press_active) {
                    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
                    int tx = (int)((event.tfinger.x * w) / ui_scale);
                    int ty = (int)((event.tfinger.y * h) / ui_scale);
                    int dx = abs(tx - long_press_start_x);
                    int dy = abs(ty - long_press_start_y);
                    // Cancel if moved more than 10 pixels in any direction
                    if (dx > 10 || dy > 10) {
                        long_press_active = 0;
                    }
                }
                #endif
                
                if (event.tfinger.fingerId == scroll_touch_id) {
                    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
                    int ty = (int)((event.tfinger.y * h) / ui_scale);
                    int delta = scroll_last_y - ty;
                    
                    // Mark that scrolling actually happened (finger moved)
                    if (abs(delta) > 3) scroll_moved = 1;
                    
                    // Apply scrolling only to the window that was touched
                    if (scroll_window_id == 0 && is_settings_open) {
                        // Settings
                        settings_scroll_y += delta;
                        if (settings_scroll_y < 0) settings_scroll_y = 0;
                        int max_scroll = settings_content_h - settings_view_port.h; 
                        if (max_scroll < 0) max_scroll = 0;
                        if (settings_scroll_y > max_scroll) settings_scroll_y = max_scroll;
                        printf("[FINGERMOTION] Settings scroll: delta=%d scroll_y=%d\n", delta, settings_scroll_y);
                    }
                    else if (scroll_window_id == 1 && show_documentation) {
                        // Documentation
                        documentation_scroll += delta;
                        if (documentation_scroll < 0) documentation_scroll = 0;
                        int content_height = 650;
                        int visible_height = 440;
                        int max_scroll = content_height - visible_height;
                        if (max_scroll < 0) max_scroll = 0;
                        if (documentation_scroll > max_scroll) documentation_scroll = max_scroll;
                        printf("[FINGERMOTION] Documentation scroll: delta=%d scroll=%d\n", delta, documentation_scroll);
                    }
                    else if (scroll_window_id == 2 && show_contributors) {
                        // Contributors
                        contributors_scroll += delta;
                        if (contributors_scroll < 0) contributors_scroll = 0;
                        int content_height = 450;
                        int visible_height = 400;
                        int max_scroll = content_height - visible_height;
                        if (max_scroll < 0) max_scroll = 0;
                        if (contributors_scroll > max_scroll) contributors_scroll = max_scroll;
                        printf("[FINGERMOTION] Contributors scroll: delta=%d scroll=%d\n", delta, contributors_scroll);
                    }
                    else if (scroll_window_id == 3 && show_friend_list) {
                        // Friend list
                        friend_list_scroll += delta;
                        if (friend_list_scroll < 0) friend_list_scroll = 0;
                        int content_height = 85 + (friend_count * 30);
                        int visible_height = 400 - 80;
                        int max_scroll = content_height - visible_height;
                        if (max_scroll < 0) max_scroll = 0;
                        if (friend_list_scroll > max_scroll) friend_list_scroll = max_scroll;
                        printf("[FINGERMOTION] Friend list scroll: delta=%d scroll=%d\n", delta, friend_list_scroll);
                    }
                    else if (scroll_window_id == 4 && is_inbox_open) {
                        // Inbox
                        inbox_scroll += delta;
                        if (inbox_scroll < 0) inbox_scroll = 0;
                        int content_height = 40 + (inbox_count * 55);
                        int visible_height = 300 - 35;
                        int max_scroll = content_height - visible_height;
                        if (max_scroll < 0) max_scroll = 0;
                        if (inbox_scroll > max_scroll) inbox_scroll = max_scroll;
                        printf("[FINGERMOTION] Inbox scroll: delta=%d scroll=%d\n", delta, inbox_scroll);
                    }
                    else if (scroll_window_id == 5 && show_my_warnings) {
                        // Warnings
                        warnings_scroll += delta;
                        if (warnings_scroll < 0) warnings_scroll = 0;
                        int content_height = 50 + (my_warning_count * 45);
                        int visible_height = 400 - 45;
                        int max_scroll = content_height - visible_height;
                        if (max_scroll < 0) max_scroll = 0;
                        if (warnings_scroll > max_scroll) warnings_scroll = max_scroll;
                        printf("[FINGERMOTION] Warnings scroll: delta=%d scroll=%d\n", delta, warnings_scroll);
                    }
                    else if (scroll_window_id == 6 && show_role_list) {
                        // Role list
                        role_list_scroll += delta;
                        if (role_list_scroll < 0) role_list_scroll = 0;
                        int content_height = 50 + (staff_count * 25);
                        int visible_height = 450 - 60;
                        int max_scroll = content_height - visible_height;
                        if (max_scroll < 0) max_scroll = 0;
                        if (role_list_scroll > max_scroll) role_list_scroll = max_scroll;
                        printf("[FINGERMOTION] Staff list scroll: delta=%d scroll=%d\n", delta, role_list_scroll);
                    }
                    else if (scroll_window_id == 7 && show_blocked_list) {
                        // Blocked list
                        blocked_scroll += delta;
                        if (blocked_scroll < 0) blocked_scroll = 0;
                        int content_height = 50 + (blocked_count * 35);
                        int visible_height = 400 - 45;
                        int max_scroll = content_height - visible_height;
                        if (max_scroll < 0) max_scroll = 0;
                        if (blocked_scroll > max_scroll) blocked_scroll = max_scroll;
                        printf("[FINGERMOTION] Blocked list scroll: delta=%d scroll=%d\n", delta, blocked_scroll);
                    }
                    else if (scroll_window_id == 8 && is_chat_open) {
                        // Chat
                        chat_scroll -= delta; // Touch drag down = see newer, drag up = see older
                        if (chat_scroll < 0) chat_scroll = 0;
                        int total_content = CHAT_HISTORY * 15;
                        int visible_content = CHAT_VISIBLE_LINES * 15;
                        int max_scroll = total_content - visible_content;
                        if (max_scroll < 0) max_scroll = 0;
                        if (chat_scroll > max_scroll) chat_scroll = max_scroll;
                        printf("[FINGERMOTION] Chat scroll: delta=%d scroll=%d\n", delta, chat_scroll);
                    }
                    else if (scroll_window_id == 9 && show_inventory) {
                        // Inventory
                        inventory_scroll += delta;
                        if (inventory_scroll < 0) inventory_scroll = 0;
                        int slots_per_row = 5;
                        int rows = (MAX_INVENTORY_SLOTS + slots_per_row - 1) / slots_per_row;
                        int slot_size = 60;
                        int spacing = 5;
                        int content_height = rows * (slot_size + spacing);
                        int visible_height = 450;
                        int max_scroll = content_height - visible_height;
                        if (max_scroll < 0) max_scroll = 0;
                        if (inventory_scroll > max_scroll) inventory_scroll = max_scroll;
                        printf("[FINGERMOTION] Inventory scroll: delta=%d scroll=%d\n", delta, inventory_scroll);
                    }
                    
                    scroll_last_y = ty;
                } else if (event.tfinger.fingerId == touch_id_dpad) {
                    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
                    // Convert touch to UI space (same as tx/ty calculation)
                    int touch_x = (int)((event.tfinger.x * w) / ui_scale);
                    int touch_y = (int)((event.tfinger.y * h) / ui_scale);
                    // dpad_rect is in UI space, so calculate offset in UI space
                    float cx = dpad_rect.x + dpad_rect.w/2;
                    float cy = dpad_rect.y + dpad_rect.h/2;
                    vjoy_dx = (touch_x - cx) / 60.0f;
                    vjoy_dy = (touch_y - cy) / 60.0f;
                    // Clamp
                    if(vjoy_dx > 1.0f) vjoy_dx = 1.0f; if(vjoy_dx < -1.0f) vjoy_dx = -1.0f;
                    if(vjoy_dy > 1.0f) vjoy_dy = 1.0f; if(vjoy_dy < -1.0f) vjoy_dy = -1.0f;
                } 
                #if defined(__ANDROID__) || defined(__IPHONEOS__)
                // Handle text selection dragging on mobile
                else if (is_dragging && (is_chat_open || active_field >= 0)) {
                    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
                    int tx = (int)((event.tfinger.x * w) / ui_scale);
                    
                    // Disable long press if finger moved
                    if (long_press_active) {
                        int dx = tx - long_press_start_x;
                        int dy = (int)((event.tfinger.y * h) / ui_scale) - long_press_start_y;
                        if (abs(dx) > 10 || abs(dy) > 10) {
                            long_press_active = 0;
                        }
                    }
                    
                    // Update text selection during drag
                    char *target = get_active_field_buffer_for_touch(NULL);
                    if (target) {
                        cursor_pos = get_cursor_pos_from_click(target, tx, active_input_rect.x);
                        selection_len = cursor_pos - selection_start;
                    }
                }
                // Handle slider dragging with touch (check if this finger is the overlay touch)
                else if (active_slider != SLIDER_NONE && event.tfinger.fingerId == overlay_touch_id) {
                    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
                    int tx = (int)((event.tfinger.x * w) / ui_scale);
                    process_slider_drag(tx);
                }
                #endif
                else {
                    // Unknown finger motion - ignore
                }
            }
            else if (event.type == SDL_FINGERUP) {
                #if defined(__ANDROID__) || defined(__IPHONEOS__)
                int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
                int tx = (int)((event.tfinger.x * w) / ui_scale);
                int ty = (int)((event.tfinger.y * h) / ui_scale);
                int scaled_w = (int)(w / ui_scale);
                int scaled_h = (int)(h / ui_scale);
                // Check for long press and show menu
                if (long_press_active) {
                    Uint32 press_duration = SDL_GetTicks() - long_press_start_time;
                    if (press_duration >= LONG_PRESS_DURATION && (is_chat_open || active_field >= 0)) {
                        position_mobile_text_menu(tx, ty, scaled_w, scaled_h, ui_scale);
                        show_mobile_text_menu = 1;
                    }
                    long_press_active = 0;
                }
                
                // Show text menu if text was selected via drag (for touch devices)
                // This will show the menu immediately after completing a selection drag
                if (is_dragging && (is_chat_open || active_field >= 0)) {
                    // Show menu if any text was selected during the drag
                    if (selection_len != 0) {
                        position_mobile_text_menu(tx, ty, scaled_w, scaled_h, ui_scale);
                        show_mobile_text_menu = 1;
                    }
                }
                
                // End dragging on finger up
                if (is_dragging) is_dragging = 0;
                #endif
                
                if (event.tfinger.fingerId == scroll_touch_id) {
                    // If finger didn't move much, treat as a tap/click instead of scroll
                    if (!scroll_moved && client_state == STATE_GAME) {
                        int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
                        int tap_x = scroll_start_x;
                        int tap_y = scroll_start_y;
                        int scaled_w = (int)(w / ui_scale);
                        int scaled_h = (int)(h / ui_scale);
                        
                        // Calculate camera position (same as mouse click handler)
                        float px=0, py=0; 
                        for(int i=0; i<MAX_CLIENTS; i++) {
                            if(local_players[i].active && local_players[i].id == local_player_id) { 
                                px=local_players[i].x; 
                                py=local_players[i].y; 
                            }
                        }
                        float zoomed_w = screen_w / game_zoom;
                        float zoomed_h = screen_h / game_zoom;
                        int cam_x = (int)px - (zoomed_w/2) + 16; 
                        int cam_y = (int)py - (zoomed_h/2) + 16;
                        // Clamp camera to map boundaries
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
                        
                        handle_game_click(tap_x, tap_y, cam_x, cam_y, scaled_w, scaled_h);
                    }
                    scroll_touch_id = -1;
                    scroll_window_id = -1;
                    scroll_last_y = 0;
                    scroll_moved = 0;
                } else if (event.tfinger.fingerId == overlay_touch_id) {
                    // Handle overlay touch that wasn't a scroll or slider
                    // Only process click if finger is released near where it started (within 50 pixels)
                    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
                    int tx = (int)((event.tfinger.x * w) / ui_scale);
                    int ty = (int)((event.tfinger.y * h) / ui_scale);
                    int dx = abs(tx - overlay_touch_start_x);
                    int dy = abs(ty - overlay_touch_start_y);
                    if (client_state == STATE_GAME && active_slider == SLIDER_NONE && dx < 50 && dy < 50) {
                        int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
                        int scaled_w = (int)(w / ui_scale);
                        int scaled_h = (int)(h / ui_scale);
                        
                        // Calculate camera position (same as mouse click handler)
                        float px=0, py=0; 
                        for(int i=0; i<MAX_CLIENTS; i++) {
                            if(local_players[i].active && local_players[i].id == local_player_id) { 
                                px=local_players[i].x; 
                                py=local_players[i].y; 
                            }
                        }
                        float zoomed_w = screen_w / game_zoom;
                        float zoomed_h = screen_h / game_zoom;
                        int cam_x = (int)px - (zoomed_w/2) + 16; 
                        int cam_y = (int)py - (zoomed_h/2) + 16;
                        // Clamp camera to map boundaries
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
                        
                        handle_game_click(overlay_touch_start_x, overlay_touch_start_y, cam_x, cam_y, scaled_w, scaled_h);
                    }
                    overlay_touch_id = -1;
                } else if (event.tfinger.fingerId == touch_id_dpad) {
                    // Check if this was a quick tap (not a joystick drag)
                    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
                    int tx = (int)((event.tfinger.x * w) / ui_scale);
                    int ty = (int)((event.tfinger.y * h) / ui_scale);
                    int scaled_w = (int)(w / ui_scale);
                    int scaled_h = (int)(h / ui_scale);
                    
                    // If joystick wasn't really used (minimal movement), treat as a tap
                    if (abs(vjoy_dx) < 0.1f && abs(vjoy_dy) < 0.1f && client_state == STATE_GAME) {
                        // Calculate camera position
                        float px=0, py=0; 
                        for(int i=0; i<MAX_CLIENTS; i++) {
                            if(local_players[i].active && local_players[i].id == local_player_id) { 
                                px=local_players[i].x; 
                                py=local_players[i].y; 
                            }
                        }
                        float zoomed_w = screen_w / game_zoom;
                        float zoomed_h = screen_h / game_zoom;
                        int cam_x = (int)px - (zoomed_w/2) + 16; 
                        int cam_y = (int)py - (zoomed_h/2) + 16;
                        // Clamp camera to map boundaries
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
                        
                        handle_game_click(tx, ty, cam_x, cam_y, scaled_w, scaled_h);
                    }
                    
                    touch_id_dpad = -1;
                    vjoy_dx = 0; vjoy_dy = 0;
                    joystick_active = 0;
                }
                
                // Apply pending slider changes on finger up (same as mouse up)
                if (active_slider == SLIDER_UI_SCALE && pending_ui_scale != ui_scale) {
                    ui_scale = pending_ui_scale;
                    reload_font_for_ui_scale();
                    save_config();
                }
                if (active_slider == SLIDER_GAME_ZOOM && pending_game_zoom != game_zoom) {
                    game_zoom = pending_game_zoom;
                    save_config();
                }
                active_slider = SLIDER_NONE;
            }
            
            // --- Mouse Drag Handling ---
            else if (event.type == SDL_MOUSEBUTTONUP) {
                is_dragging = 0;
                // Apply pending UI scale change on release
                if (active_slider == SLIDER_UI_SCALE && pending_ui_scale != ui_scale) {
                    ui_scale = pending_ui_scale;
                    reload_font_for_ui_scale(); // Reload font at new scale to prevent distortion
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
                    else if (active_field == 20) target = password_current;
                    else if (active_field == 21) target = password_new;
                    else if (active_field == 22) target = password_confirm;
                    else if (active_field == 25) target = input_friend_id;
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
                    // Convert window coordinates to UI coordinate space
                    int mx = (int)((event.button.x * scale_x) / ui_scale);
                    int my = (int)((event.button.y * scale_y) / ui_scale);
                    handle_auth_click(mx, my);
                }
                // Handle touch input on mobile devices
                else if(event.type == SDL_FINGERDOWN) {
                    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
                    // Convert touch coordinates to UI space
                    int tx = (int)((event.tfinger.x * w) / ui_scale);
                    int ty = (int)((event.tfinger.y * h) / ui_scale);
                    printf("[AUTH FINGERDOWN] tx=%d ty=%d\n", tx, ty);
                    handle_auth_click(tx, ty);
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
                    #if defined(__ANDROID__) || defined(__IPHONEOS__)
                    // On mobile, don't reset selection if text menu is visible - user may be clicking menu buttons
                    if (!show_mobile_text_menu) {
                        active_field = -1; selection_len = 0; selection_start = 0;
                    }
                    #else
                    active_field = -1; selection_len = 0; selection_start = 0;
                    #endif
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
                            // Send typing started
                            Packet typing_pkt;
                            memset(&typing_pkt, 0, sizeof(Packet));
                            typing_pkt.type = PACKET_TYPING;
                            typing_pkt.player_id = 1;
                            send_packet(&typing_pkt);
                            is_typing = 1;
                            last_typing_update = SDL_GetTicks();
                        } 
                        else { 
                            chat_input_active = 0; 
                            SDL_StopTextInput();
                            #if defined(__ANDROID__) || defined(__IPHONEOS__)
                            keyboard_height = 0;
                            #endif
                            // Send typing stopped
                            Packet typing_pkt;
                            memset(&typing_pkt, 0, sizeof(Packet));
                            typing_pkt.type = PACKET_TYPING;
                            typing_pkt.player_id = 0;
                            send_packet(&typing_pkt);
                            is_typing = 0;
                        }
                     } 
                     else if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn_settings_toggle)) {
                        is_settings_open = !is_settings_open;
                        if (!is_settings_open) { show_nick_popup = 0; show_password_popup = 0; show_blocked_list = 0; active_field = -1; }
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
                         // Calculate chat window position with keyboard shift (same as rendering)
                         int scaled_h = (int)(screen_h / ui_scale);
                         #if defined(__ANDROID__) || defined(__IPHONEOS__)
                         int chat_y = get_chat_window_y(scaled_h, ui_scale);
                         #else
                         int chat_y = scaled_h - 240;  // Fixed position on desktop (no keyboard shift)
                         #endif
                         
                         SDL_Rect chat_win = (SDL_Rect){10, chat_y, 300, 190};
                         SDL_Rect chat_input = (SDL_Rect){15, chat_win.y + chat_win.h - 24, 270, 24};
                         if (SDL_PointInRect(&(SDL_Point){mx, my}, &chat_input)) {
                             #if defined(__ANDROID__) || defined(__IPHONEOS__)
                             // Track if the input was already active (for text operations)
                             int was_already_active = chat_input_active;
                             // Preserve selection if text menu is visible
                             int preserve_selection = show_mobile_text_menu && selection_len != 0;
                             #else
                             int preserve_selection = 0;
                             #endif
                             
                             chat_input_active = 1;
                             SDL_StartTextInput();
                             
                             // Set up cursor position and text selection for dragging
                             int prefix_w = 0, ph;
                             char prefix[64];
                             if (chat_target_id != -1) {
                                 char *name = "Unknown";
                                 for(int i=0; i<MAX_CLIENTS; i++) {
                                     if(local_players[i].id == chat_target_id) name = local_players[i].username;
                                 }
                                 snprintf(prefix, 64, "To %s: ", name);
                             } else {
                                 strcpy(prefix, "> ");
                             }
                             TTF_SizeText(font, prefix, &prefix_w, &ph);
                             active_input_rect = chat_input;
                             active_input_rect.x += (5 + prefix_w);
                             
                             // Only update cursor and selection if not preserving current selection
                             if (!preserve_selection) {
                                 cursor_pos = get_cursor_pos_from_click(input_buffer, mx, active_input_rect.x);
                                 selection_start = cursor_pos;
                                 selection_len = 0;
                             }
                             is_dragging = 1;
                             
                             #if defined(__ANDROID__) || defined(__IPHONEOS__)
                             // Only trigger keyboard shift on initial activation, not when doing text operations
                             // If text menu is visible, don't shift - user is doing text formatting
                             // If input was already active, don't re-shift - user is selecting text
                             if (!show_mobile_text_menu && !was_already_active) {
                                 // Estimate keyboard height as 40% of screen height on mobile
                                 keyboard_height = screen_h * 0.4;
                             }
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
                         // Calculate camera position accounting for game zoom
                         float px=0, py=0; for(int i=0; i<MAX_CLIENTS; i++) if(local_players[i].active && local_players[i].id == local_player_id) { px=local_players[i].x; py=local_players[i].y; }
                         // Camera calculation must match render_game() - use zoomed dimensions
                         float zoomed_w = screen_w / game_zoom;
                         float zoomed_h = screen_h / game_zoom;
                         int cam_x = (int)px - (zoomed_w/2) + 16; 
                         int cam_y = (int)py - (zoomed_h/2) + 16;
                         // Clamp camera to map boundaries
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
                            // Send typing stopped after sending message
                            Packet typing_pkt;
                            memset(&typing_pkt, 0, sizeof(Packet));
                            typing_pkt.type = PACKET_TYPING;
                            typing_pkt.player_id = 0;
                            send_packet(&typing_pkt);
                            is_typing = 0;
                        }
                        else if(event.key.keysym.sym == SDLK_ESCAPE) { 
                            is_chat_open = 0; 
                            chat_target_id = -1;
                            chat_input_active = 0;
                            SDL_StopTextInput();
                            #if defined(__ANDROID__) || defined(__IPHONEOS__)
                            keyboard_height = 0;
                            #endif
                            // Send typing stopped when canceling
                            Packet typing_pkt;
                            memset(&typing_pkt, 0, sizeof(Packet));
                            typing_pkt.type = PACKET_TYPING;
                            typing_pkt.player_id = 0;
                            send_packet(&typing_pkt);
                            is_typing = 0;
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
                else if (is_settings_open && show_password_popup) {
                    char *target = NULL;
                    if(active_field == 20) target = password_current;
                    else if(active_field == 21) target = password_new;
                    else if(active_field == 22) target = password_confirm;
                    if (target) handle_text_edit(target, 63, &event);
                }
                else if (show_add_friend_popup) {
                    if (active_field == 25) handle_text_edit(input_friend_id, 8, &event);
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
            if (!is_chat_open && !show_nick_popup && !show_password_popup && !show_add_friend_popup && !show_sanction_popup && pending_friend_req_id == -1) {
                
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
                                    
                                    // Check for ground item pickup
                                    SDL_Rect player_rect = {(int)local_players[i].x, (int)local_players[i].y, 32, 32};
                                    for (int g = 0; g < client_ground_item_count; g++) {
                                        if (strcmp(client_ground_items[g].map_name, current_map_file) == 0) {
                                            SDL_Rect item_rect = {(int)client_ground_items[g].x, (int)client_ground_items[g].y, 24, 24};
                                            if (SDL_HasIntersection(&player_rect, &item_rect)) {
                                                // Pick up the item!
                                                Packet pickup_pkt;
                                                memset(&pickup_pkt, 0, sizeof(Packet));
                                                pickup_pkt.type = PACKET_ITEM_PICKUP;
                                                pickup_pkt.item_slot = g; // Send the ground item index
                                                send_packet(&pickup_pkt);
                                                printf("Picking up item %d at %.1f,%.1f\n", client_ground_items[g].item_id, 
                                                       client_ground_items[g].x, client_ground_items[g].y);
                                            }
                                        }
                                    }
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
                int recv_result = recv_total(sock, &pkt, sizeof(Packet));
                if (recv_result > 0) {
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
                            
                            // Send telemetry data
                            send_telemetry();
                        }
                        else if (pkt.status == AUTH_REGISTER_SUCCESS) strcpy(auth_message, get_string(STR_SUCCESS_REGISTERED));
                        else if (pkt.msg[0] != '\0') strcpy(auth_message, pkt.msg);
                        else strcpy(auth_message, get_string(STR_ERROR_GENERIC));
                    }
                    if (pkt.type == PACKET_UPDATE) {
                        memcpy(local_players, pkt.players, sizeof(local_players));
                        for(int i=0; i<MAX_CLIENTS; i++) {
                            if (local_players[i].id == local_player_id) {
                                // Update player stats from server
                                my_gold = local_players[i].gold;
                                my_xp = local_players[i].xp;
                                my_level = local_players[i].level;
                                
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
                    if (pkt.type == PACKET_DIALOGUE) {
                        // Display dialogue from NPC
                        strncpy(dialogue_text, pkt.dialogue.text, 255);
                        dialogue_text[255] = '\0';
                        
                        // Find NPC name from NPC ID
                        for (int i = 0; i < client_npc_count; i++) {
                            if (client_npcs[i].npc_id == pkt.dialogue.npc_id) {
                                strncpy(dialogue_npc_name, client_npcs[i].name, 31);
                                dialogue_npc_name[31] = '\0';
                                break;
                            }
                        }
                        
                        show_dialogue = 1;
                        printf("Received dialogue from NPC %d: %s\n", pkt.dialogue.npc_id, dialogue_text);
                    }
                    if (pkt.type == PACKET_SHOP_OPEN) {
                        // Open shop window with items
                        current_shop = pkt.shop_data;
                        
                        // Find NPC name from NPC ID
                        for (int i = 0; i < client_npc_count; i++) {
                            if (client_npcs[i].npc_id == pkt.shop_data.npc_id) {
                                strncpy(dialogue_npc_name, client_npcs[i].name, 31);
                                dialogue_npc_name[31] = '\0';
                                break;
                            }
                        }
                        
                        show_shop = 1;
                        shop_tab = 0;
                        shop_scroll = 0;
                        printf("Opened shop with %d items from NPC %d\n", current_shop.item_count, pkt.shop_data.npc_id);
                    }
                    if (pkt.type == PACKET_QUEST_LIST) {
                        // Check if this is quest offers (npc_id set) or player's active quests (npc_id = 0)
                        if (pkt.npc_id > 0) {
                            // Quest offers from NPC
                            available_quest_count = pkt.quest_count;
                            memcpy(available_quests, pkt.quests, sizeof(Quest) * pkt.quest_count);
                            show_quest_offer = 1;
                            quest_offer_scroll = 0;
                            printf("Received %d quest offers from NPC %d\n", available_quest_count, pkt.npc_id);
                        } else {
                            // Player's active quests
                            my_quest_count = pkt.quest_count;
                            memcpy(my_quests, pkt.quests, sizeof(Quest) * pkt.quest_count);
                            printf("Received %d active quests\n", my_quest_count);
                        }
                    }
                    if (pkt.type == PACKET_QUEST_PROGRESS) {
                        // Update quest progress
                        if (pkt.quest_count > 0) {
                            Quest *updated_quest = &pkt.quests[0];
                            for (int i = 0; i < my_quest_count; i++) {
                                if (my_quests[i].quest_id == updated_quest->quest_id) {
                                    my_quests[i] = *updated_quest;
                                    printf("Quest %d progress updated\n", updated_quest->quest_id);
                                    break;
                                }
                            }
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
                    if (pkt.type == PACKET_CHANGE_PASSWORD_RESPONSE) {
                        if (pkt.status == AUTH_SUCCESS) {
                            show_password_popup = 0;
                            strcpy(password_message, get_string(STR_SUCCESS_PASSWORD_CHANGED));
                            active_field = -1;
                        } else {
                            strncpy(password_message, pkt.msg, 127);
                        }
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
                    if (pkt.type == PACKET_NPC_LIST) {
                        // Update NPC list from server
                        client_npc_count = pkt.npc_count;
                        for (int i = 0; i < pkt.npc_count; i++) {
                            client_npcs[i] = pkt.npcs[i];
                        }
                        printf("NPCs updated: %d NPCs loaded\n", client_npc_count);
                    }
                    if (pkt.type == PACKET_CURRENCY_UPDATE) {
                        // Update player currency and stats
                        my_gold = pkt.gold;
                        my_xp = pkt.xp;
                        my_level = pkt.level;
                        printf("Currency updated: Gold=%d, XP=%d, Level=%d\n", my_gold, my_xp, my_level);
                    }
                    if (pkt.type == PACKET_INVENTORY_UPDATE) {
                        // Update local inventory from server
                        my_inventory_count = pkt.inventory_count;
                        for (int i = 0; i < pkt.inventory_count; i++) {
                            my_inventory[i] = pkt.inventory_slots[i];
                        }
                        // Update equipment
                        for (int i = 0; i < EQUIP_SLOT_COUNT; i++) {
                            my_equipment[i] = pkt.equipment[i];
                        }
                        printf("Inventory updated: %d items, equipment updated\n", my_inventory_count);
                    }
                    if (pkt.type == PACKET_GROUND_ITEMS) {
                        // Update ground items from server
                        client_ground_item_count = pkt.ground_item_count;
                        for (int i = 0; i < pkt.ground_item_count; i++) {
                            client_ground_items[i] = pkt.ground_items[i];
                        }
                        printf("Ground items updated: %d items on map\n", client_ground_item_count);
                    }
                } else if (recv_result <= 0) {
                    // Server disconnected or error
                    printf("Server disconnected (recv returned %d)\n", recv_result);
                    strcpy(auth_message, get_string(STR_ERROR_SERVER_DISCONNECTED));
                    close(sock); 
                    sock = -1; 
                    is_connected = 0;
                    client_state = STATE_AUTH;
                    Mix_HaltMusic();
                }
            }
        }

        // Render game content using SDL_Renderer
        // SDL automatically uses best rendering backend (OpenGL, OpenGL ES, or Vulkan on Linux)
        if (client_state == STATE_AUTH) render_auth_screen(renderer); else render_game(renderer);
        
        // Minimal delay to prevent excessive CPU usage while keeping high FPS
        // 1ms allows ~1000 FPS cap but prevents CPU spinning
        SDL_Delay(1);
    }
    
    if(tex_map) SDL_DestroyTexture(tex_map); if(tex_player) SDL_DestroyTexture(tex_player);
    if(cached_contributors_tex) SDL_DestroyTexture(cached_contributors_tex);
    if(cached_documentation_tex) SDL_DestroyTexture(cached_documentation_tex);
    if(cached_role_list_tex) SDL_DestroyTexture(cached_role_list_tex);
    if(bgm) Mix_FreeMusic(bgm); Mix_CloseAudio();
    if(sock > 0) close(sock); 
    TTF_CloseFont(font); TTF_Quit(); IMG_Quit();
    
    // SDL handles all Vulkan cleanup automatically when using SDL_Renderer
    // No need for manual vulkan_cleanup() call
    
    if (renderer) SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}