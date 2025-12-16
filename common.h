#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define PORT 8888
#define MAX_CLIENTS 10
#define MAP_WIDTH 2000
#define MAP_HEIGHT 2000
#define PLAYER_SPEED 4.0f
#define MAX_AVATAR_SIZE 16384 

typedef enum {
    STATUS_ONLINE, STATUS_AFK, STATUS_DND, STATUS_ROLEPLAY, STATUS_TALK
} PlayerStatus;

// NEW: Roles
typedef enum {
    ROLE_PLAYER,
    ROLE_ADMIN,
    ROLE_DEVELOPER,
    ROLE_CONTRIBUTOR,
    ROLE_VIP
} PlayerRole;

// ... (PacketType enum remains the same) ...
typedef enum {
    PACKET_REGISTER_REQUEST, PACKET_LOGIN_REQUEST, PACKET_AUTH_RESPONSE,
    PACKET_JOIN, PACKET_MOVE, PACKET_CHAT, PACKET_UPDATE,
    PACKET_FRIEND_LIST, PACKET_FRIEND_REQUEST, PACKET_FRIEND_INCOMING,
    PACKET_FRIEND_RESPONSE, PACKET_FRIEND_REMOVE,
    PACKET_PING, PACKET_PRIVATE_MESSAGE, PACKET_STATUS_CHANGE,
    PACKET_COLOR_CHANGE,
    PACKET_AVATAR_UPLOAD, PACKET_AVATAR_REQUEST, PACKET_AVATAR_RESPONSE,
    // NEW: Nickname Change
    PACKET_CHANGE_NICK_REQUEST, 
    PACKET_CHANGE_NICK_RESPONSE,
    PACKET_ROLE_LIST_REQUEST,  // New
    PACKET_ROLE_LIST_RESPONSE,  // New
    // NEW: Sanction System
    PACKET_SANCTION_REQUEST,   // Admin -> Server (Warn/Ban)
    PACKET_WARNINGS_REQUEST,   // User -> Server (Get history)
    PACKET_WARNINGS_RESPONSE,  // Server -> User (History data)
    PACKET_KICK,                // Server -> Client (Force disconnect)
    PACKET_MAP_CHANGE
} PacketType;

typedef enum {
    AUTH_SUCCESS, AUTH_FAILURE, AUTH_REGISTER_SUCCESS,
    AUTH_REGISTER_FAILED_EXISTS, AUTH_REGISTER_FAILED_DB
} AuthStatus;

// NEW: Struct for Friend Data
typedef struct {
    int id;
    char username[32];
    char last_login[32]; // e.g. "2023-10-27 14:30"
    int is_online;       // 1=Online, 0=Offline
} FriendInfo;

typedef struct {
    int id;
    float x; float y;
    int direction; int is_moving; int active;
    char username[64];
    int status;
    int role; // NEW: Role ID
    uint8_t r, g, b;
    uint8_t r2, g2, b2;
    char map_name[32];
} Player;

typedef struct {
    PacketType type;
    int player_id; int target_id;
    float dx; float dy;
    char msg[64];
    int role_count;
    struct {
        int id;
        char username[32];
        int role;
    } roles[50];
    Player players[MAX_CLIENTS];
    char username[64]; char password[64];
    AuthStatus status;
    // UPDATED: Rich Friend Data
    FriendInfo friends[20]; 
    int friend_count;
    int response_accepted;
    uint32_t timestamp;
    int new_status;
    uint8_t r, g, b;
    uint8_t r2, g2, b2;
    int image_size;
    uint8_t image_data;
    char target_map[32]; 
    int sanction_type; // 0 = Warn, 1 = Ban
    char sanction_reason[64];
    char ban_duration[16]; // "1h", "1d", etc.
    
    // NEW: Warning History List
    int warning_count;
    struct {
        char reason[64];
        char date[32];
    } warnings[20];
} Packet;

#endif