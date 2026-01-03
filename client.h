#ifndef CLIENT_H
#define CLIENT_H

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
#ifdef __ANDROID__
#include <SDL2/SDL_system.h>
#include <android/log.h>
#define ALOG(...) __android_log_print(ANDROID_LOG_INFO, "C_MMO_Client", __VA_ARGS__)
#else
#define ALOG(...) printf(__VA_ARGS__)
#endif
#include <sys/types.h>

// Handle OpenGL headers
#if defined(__IPHONEOS__) || defined(__ANDROID__)
#include <SDL2/SDL_opengles2.h>
#elif defined(__APPLE__)
#include <SDL2/SDL_opengl.h>
#elif defined(_WIN32)
#include <SDL2/SDL_opengl.h>
#else
#include <GL/gl.h>
#endif

#include "common.h"
#include <dirent.h>
#include "localization.h"

// Vulkan support (optional)
#ifdef USE_VULKAN
#include "renderer_vulkan.h"
#endif

// --- Config ---
#define PLAYER_WIDTH 32
#define PLAYER_HEIGHT 32
#define PLAYER_FILE "player.png"
#define FONT_PATH "DejaVuSans.ttf"
#define FONT_SIZE 14
#define MIN_FONT_SIZE 8
#define SCROLL_SPEED 30

// --- Rendering Backend ---
typedef enum {
    RENDER_BACKEND_OPENGL,
    RENDER_BACKEND_VULKAN
} RenderBackend;

// --- Socket type ---
#ifdef _WIN32
    typedef SOCKET socket_t;
#else
    typedef int socket_t;
#endif

// --- Chat & Auth ---
typedef struct { char msg[64]; Uint32 timestamp; } FloatingText;
#define CHAT_HISTORY 100
#define CHAT_VISIBLE_LINES 10

typedef enum { STATE_AUTH, STATE_GAME } ClientState;

#define MAX_INPUT_LEN 31
#define MAX_PASSWORD_LEN 63

// Field IDs
#define FIELD_AUTH_USERNAME 0
#define FIELD_AUTH_PASSWORD 1
#define FIELD_IP 2
#define FIELD_PORT 3
#define FIELD_NICK_NEW 10
#define FIELD_NICK_CONFIRM 11
#define FIELD_NICK_PASS 12
#define FIELD_PASSWORD_CURRENT 20
#define FIELD_PASSWORD_NEW 21
#define FIELD_PASSWORD_CONFIRM 22
#define FIELD_FRIEND_ID 25
#define FIELD_SANCTION_REASON 30
#define FIELD_BAN_TIME 31

// --- Server List ---
typedef struct { char name[32]; char ip[64]; int port; } ServerEntry;

// --- Profile List ---
typedef struct { char username[32]; char password[32]; } ProfileEntry;

// --- Inbox ---
typedef struct { int id; char name[32]; } IncomingReq;

// --- Map Triggers ---
typedef struct {
    char src_map[32];
    SDL_Rect rect;
    char target_map[32];
    int spawn_x, spawn_y;
} MapTrigger;

// --- Staff List ---
typedef struct { int id; char name[32]; int role; } StaffEntry;

// --- Warning List ---
typedef struct { char reason[64]; char date[32]; } WarningEntry;

// Slider types
enum { SLIDER_NONE, SLIDER_R, SLIDER_G, SLIDER_B, SLIDER_R2, SLIDER_G2, SLIDER_B2, SLIDER_VOL, SLIDER_AFK, SLIDER_UI_SCALE, SLIDER_GAME_ZOOM };

// Long press duration for mobile
#define LONG_PRESS_DURATION 500

// Mobile text menu dimensions
#define MOBILE_TEXT_MENU_WIDTH 350
#define MOBILE_TEXT_MENU_HEIGHT 50

// --- Global Variables (extern declarations) ---
extern char current_map_file[32];
extern RenderBackend render_backend;
#ifdef USE_VULKAN
extern VulkanRenderer vk_renderer;
extern int use_vulkan;
extern int backend_set_by_cmdline;
#endif

extern socket_t sock;
extern char server_ip[16];
extern TTF_Font *font;
extern SDL_Texture *tex_map;
extern SDL_Texture *tex_player;
extern SDL_Renderer *global_renderer;
extern int map_w, map_h;

extern int local_player_id;
extern Player local_players[MAX_CLIENTS];
extern FriendInfo my_friends[20];
extern int friend_count;

extern int blocked_ids[50];
extern int blocked_count;
extern int show_blocked_list;
extern int blocked_scroll;

extern SDL_Rect btn_hide_player;
extern SDL_Rect btn_view_blocked;
extern SDL_Rect blocked_win;
extern SDL_Rect btn_close_blocked;
extern SDL_Rect btn_hide_player_dyn;
extern SDL_Rect btn_change_avatar;
extern SDL_Rect btn_change_password;

extern int selected_player_id;
extern int pending_friend_req_id;
extern char pending_friend_name[32];

extern SDL_Rect profile_win;
extern SDL_Rect btn_add_friend;
extern SDL_Rect btn_send_pm;
extern SDL_Rect popup_win;

extern int is_settings_open;
extern int show_debug_info;
extern int show_fps;
extern int show_coords;
extern int current_ping;
extern Uint32 last_ping_sent;

extern int current_fps;
extern int frame_count;
extern Uint32 last_fps_check;

extern SDL_Rect btn_settings_toggle;
extern SDL_Rect settings_win;
extern SDL_Rect btn_toggle_debug;
extern SDL_Rect btn_toggle_fps;
extern SDL_Rect btn_toggle_coords;

extern FloatingText floating_texts[MAX_CLIENTS];
extern char chat_log[CHAT_HISTORY][64];
extern int chat_log_count;
extern int chat_scroll;
extern int is_chat_open;
extern int chat_target_id;
extern int chat_input_active;
extern char input_buffer[64];

extern ClientState client_state;
extern char auth_username[MAX_INPUT_LEN+1];
extern char auth_password[MAX_INPUT_LEN+1];
extern int active_field;
extern int show_password;
extern char auth_message[128];

extern SDL_Rect auth_box, btn_login, btn_register, btn_chat_toggle, btn_show_pass;
extern SDL_Color col_white, col_red, col_yellow, col_cyan, col_green, col_btn, col_black, col_magenta;
extern SDL_Color col_status_online, col_status_afk, col_status_dnd, col_status_rp, col_status_talk;
extern const char* status_names[];

extern SDL_Rect btn_cycle_status;
extern SDL_Rect btn_view_friends;
extern SDL_Rect friend_list_win;
extern int show_friend_list;
extern int friend_list_scroll;

extern SDL_Rect slider_r, slider_g, slider_b;
extern SDL_Texture* avatar_cache[MAX_CLIENTS];
extern int avatar_status[MAX_CLIENTS];

extern char music_playlist[20][64];
extern int music_count;
extern int current_track;
extern Mix_Music *bgm;
extern int music_volume;
extern SDL_Rect slider_volume;

extern uint8_t temp_avatar_buf[MAX_AVATAR_SIZE];

extern char input_ip[64];
extern char input_port[16];
extern int is_connected;

extern ServerEntry server_list[10];
extern int server_count;
extern int show_server_list;
extern SDL_Rect server_list_win;
extern SDL_Rect btn_open_servers;
extern SDL_Rect btn_add_server;
extern SDL_Rect btn_close_servers;

extern ProfileEntry profile_list[10];
extern int profile_count;
extern int show_profile_list;
extern SDL_Rect profile_list_win;
extern SDL_Rect btn_open_profiles;
extern SDL_Rect btn_add_profile;
extern SDL_Rect btn_close_profiles;

extern int show_nick_popup;
extern char nick_new[32];
extern char nick_confirm[32];
extern char nick_pass[32];

extern int show_password_popup;
extern char password_current[64];
extern char password_new[64];
extern char password_confirm[64];
extern char password_message[128];
extern int show_password_current;
extern int show_password_new;
extern int show_password_confirm;

extern IncomingReq inbox[10];
extern int inbox_count;
extern int is_inbox_open;
extern SDL_Rect btn_inbox;
extern int inbox_scroll;

extern int show_add_friend_popup;
extern char input_friend_id[10];
extern char friend_popup_msg[128];

extern SDL_Rect btn_friend_add_id_rect;
extern SDL_Rect friend_win_rect;
extern SDL_Rect btn_friend_close_rect;
extern SDL_Rect blocked_win_rect;
extern SDL_Rect btn_blocked_close_rect;
extern SDL_Rect slider_afk;
extern SDL_Rect btn_disconnect_rect;
extern SDL_Rect btn_inbox_rect;

extern int cursor_pos;
extern int unread_chat_count;
extern int show_unread_counter;
extern SDL_Rect btn_toggle_unread;
extern SDL_Rect btn_toggle_vulkan;
extern SDL_Rect btn_toggle_nvidia_gpu;
extern SDL_Rect btn_cycle_language;

extern char gl_renderer_cache[128];
extern char gl_vendor_cache[128];
extern int gl_probe_done;

#ifdef USE_VULKAN
extern char vk_device_name[128];
extern int vk_probe_done;
#endif

extern Uint32 last_input_tick;
extern int afk_timeout_minutes;
extern Uint32 last_color_packet_ms;
extern int is_auto_afk;

extern int settings_scroll_y;
extern int settings_content_h;
extern SDL_Rect settings_view_port;

extern SDL_Rect slider_r2, slider_g2, slider_b2;
extern int my_r2, my_g2, my_b2;
extern int saved_r, saved_g, saved_b;

extern int selection_start;
extern int selection_len;
extern int is_dragging;
extern SDL_Rect active_input_rect;

extern int show_nick_pass;
extern SDL_Rect btn_show_nick_pass;

extern int show_contributors;

extern float ui_scale;
extern float pending_ui_scale;
extern SDL_Rect slider_ui_scale;

extern float game_zoom;
extern float pending_game_zoom;
extern SDL_Rect slider_game_zoom;

extern int config_use_vulkan;
extern int config_use_nvidia_gpu;

#if defined(__ANDROID__) || defined(__IPHONEOS__)
extern int keyboard_height;
extern int chat_window_shift;
extern int show_mobile_text_menu;
extern SDL_Rect mobile_text_menu_rect;
extern int mobile_text_menu_x;
extern int mobile_text_menu_y;
extern Uint32 long_press_start_time;
extern int long_press_active;
extern int long_press_start_x;
extern int long_press_start_y;
#endif

extern int show_documentation;
extern SDL_Rect btn_contributors_rect;
extern SDL_Rect btn_documentation_rect;

extern SDL_Texture *cached_contributors_tex;
extern SDL_Texture *cached_documentation_tex;
extern int contributors_tex_w, contributors_tex_h;
extern int documentation_tex_w, documentation_tex_h;
extern int contributors_scroll;
extern int documentation_scroll;

extern int show_role_list;
extern int role_list_scroll;
extern StaffEntry staff_list[50];
extern int staff_count;
extern SDL_Rect btn_staff_list_rect;

extern int show_sanction_popup;
extern int sanction_target_id;
extern int sanction_mode;
extern char input_sanction_reason[64];
extern char input_ban_time[16];

extern int show_my_warnings;
extern WarningEntry my_warning_list[20];
extern int my_warning_count;
extern int warnings_scroll;

extern SDL_Rect btn_sanction_open;
extern SDL_Rect btn_my_warnings;

extern int active_slider;

extern int my_r, my_g, my_b;

extern MapTrigger triggers[20];
extern int trigger_count;
extern Uint32 last_map_switch_time;
extern Uint32 last_move_time;

extern SDL_Rect dpad_rect;
extern float vjoy_dx, vjoy_dy;
extern SDL_FingerID touch_id_dpad;
extern SDL_FingerID scroll_touch_id;
extern int scroll_last_y;
extern int joystick_active;

// --- Inventory System ---
extern InventorySlot my_inventory[MAX_INVENTORY_SLOTS];
extern Item my_equipment[EQUIP_SLOT_COUNT];
extern int my_inventory_count;
extern int show_inventory;
extern SDL_Rect inventory_win;
extern SDL_Rect btn_inventory_toggle;
extern int inventory_scroll;
extern int selected_inv_slot;
extern int dragging_item_slot;

// --- Function Declarations ---

// Network functions (client_network.c)
int send_all(socket_t sockfd, const void *buf, size_t len);
void send_packet(Packet *pkt);
int recv_total(socket_t sockfd, void *buf, size_t len);
int try_connect(void);
void get_os_info(char *buffer, size_t size);
void send_telemetry(void);

// Config functions (client_config.c)
void get_path(char *out, const char *filename, int is_save_file);
void ensure_save_file(const char *filename, const char *asset_name);
void load_servers(void);
void save_config(void);
void load_config(void);
void load_triggers(void);
void receive_triggers_from_server(Packet *pkt);
void save_servers(void);
void add_server_to_list(const char* name, const char* ip, int port);
void load_profiles(void);
void save_profiles(void);
void add_profile_to_list(const char* username, const char* password);

// Utils functions (client_utils.c)
int is_blocked(int id);
void reset_avatar_cache(void);
void push_chat_line(const char *text);
void toggle_block(int id);
SDL_Color get_status_color(int status);
int scale_ui(int value);
SDL_Rect scale_rect(int x, int y, int w, int h);
SDL_Color get_color_from_code(char code);
int calculate_scaled_font_size(void);
void reload_font_for_ui_scale(void);
void render_raw_text(SDL_Renderer *renderer, const char *text, int x, int y, SDL_Color color, int center);
void remove_last_utf8_char(char *str);
void render_text_gradient(SDL_Renderer *renderer, const char *text, int x, int y, SDL_Color c1, SDL_Color c2, int center);
void render_text(SDL_Renderer *renderer, const char *text, int x, int y, SDL_Color default_color, int center);
const char* get_role_name(int role);
SDL_Color get_role_color(int role);
void switch_map(const char* new_map, int x, int y);

// Audio functions (client_audio.c)
void play_next_track(void);
void init_audio(void);

// Input functions (client_input.c)
int get_cursor_pos_from_click(const char *text, int mouse_x, int rect_x);
void delete_selection(char *buffer);
char* get_active_field_buffer_for_touch(int *max_len_out);
void position_mobile_text_menu(int touch_x, int touch_y, int scaled_w, int scaled_h, float ui_scale_val);
void handle_text_edit(char *buffer, int max_len, SDL_Event *ev);
void handle_game_click(int mx, int my, int cam_x, int cam_y, int w, int h);
void handle_auth_click(int mx, int my);

// Rendering functions (client_ui_render.c and client_game_render.c)
void render_input_with_cursor(SDL_Renderer *renderer, SDL_Rect rect, char *buffer, int is_active, int is_password);
void add_chat_message(Packet *pkt);
void render_hud(SDL_Renderer *renderer, int screen_w, int screen_h);
void render_inbox(SDL_Renderer *renderer, int w, int h);
void render_add_friend_popup(SDL_Renderer *renderer, int w, int h);
void render_sanction_popup(SDL_Renderer *renderer, int w, int h);
void render_my_warnings(SDL_Renderer *renderer, int w, int h);
void render_inventory(SDL_Renderer *renderer, int w, int h);
void render_equipment_slot(SDL_Renderer *renderer, int slot, SDL_Rect rect);
void render_inventory_slot(SDL_Renderer *renderer, InventorySlot *slot, SDL_Rect rect);
void handle_inventory_click(int mx, int my);
void request_item_pickup(void);
void request_item_drop(int slot);
void request_item_use(int slot);
void request_item_equip(int slot, int equip_slot);
void render_debug_overlay(SDL_Renderer *renderer, int screen_w);
void render_role_list(SDL_Renderer *renderer, int w, int h);
void process_slider_drag(int mx);
void render_fancy_slider(SDL_Renderer *renderer, SDL_Rect *rect, float pct, SDL_Color fill_col);
void open_file_picker_for_avatar(void);

#ifdef __IPHONEOS__
// iOS file picker functions (implemented in ios_file_picker.m)
void ios_set_image_callback(void (*callback)(const uint8_t* data, size_t size));
void ios_open_file_picker(void);
#endif

void render_settings_menu(SDL_Renderer *renderer, int screen_w, int screen_h);
void render_friend_list(SDL_Renderer *renderer, int w, int h);
void render_popup(SDL_Renderer *renderer, int w, int h);
void render_profile(SDL_Renderer *renderer);
void render_auth_screen(SDL_Renderer *renderer);
void render_blocked_list(SDL_Renderer *renderer, int w, int h);
void render_contributors(SDL_Renderer *renderer, int w, int h);
void render_documentation(SDL_Renderer *renderer, int w, int h);
void render_mobile_controls(SDL_Renderer *renderer, int h);
void render_mobile_text_menu(SDL_Renderer *renderer);
void render_game(SDL_Renderer *renderer);

#endif // CLIENT_H
