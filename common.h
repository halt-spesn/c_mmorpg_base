#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define PORT 8888
#define MAX_CLIENTS 10
#define MAP_WIDTH 2000
#define MAP_HEIGHT 2000
#define PLAYER_SPEED 4.0f
#define MAX_AVATAR_SIZE 16384 

// Keep Enums for code readability, but don't use them in the struct directly
typedef enum {
    STATUS_ONLINE, STATUS_AFK, STATUS_DND, STATUS_ROLEPLAY, STATUS_TALK
} PlayerStatus;

typedef enum {
    ROLE_PLAYER, ROLE_ADMIN, ROLE_DEVELOPER, ROLE_CONTRIBUTOR, ROLE_VIP
} PlayerRole;

typedef enum {
    PACKET_REGISTER_REQUEST, PACKET_LOGIN_REQUEST, PACKET_AUTH_RESPONSE,
    PACKET_JOIN, PACKET_MOVE, PACKET_CHAT, PACKET_UPDATE,
    PACKET_FRIEND_LIST, PACKET_FRIEND_REQUEST, PACKET_FRIEND_INCOMING,
    PACKET_FRIEND_RESPONSE, PACKET_FRIEND_REMOVE,
    PACKET_PING, PACKET_PRIVATE_MESSAGE, PACKET_STATUS_CHANGE,
    PACKET_COLOR_CHANGE,
    PACKET_AVATAR_UPLOAD, PACKET_AVATAR_REQUEST, PACKET_AVATAR_RESPONSE,
    PACKET_CHANGE_NICK_REQUEST, PACKET_CHANGE_NICK_RESPONSE,
    PACKET_ROLE_LIST_REQUEST, PACKET_ROLE_LIST_RESPONSE,
    PACKET_SANCTION_REQUEST, PACKET_WARNINGS_REQUEST, PACKET_WARNINGS_RESPONSE,
    PACKET_KICK, PACKET_MAP_CHANGE
} PacketType;

typedef enum {
    AUTH_SUCCESS, AUTH_FAILURE, AUTH_REGISTER_SUCCESS,
    AUTH_REGISTER_FAILED_EXISTS, AUTH_REGISTER_FAILED_DB
} AuthStatus;

// --- FORCE 1-BYTE PACKING ---
#pragma pack(push, 1)

typedef struct {
    int32_t id;
    char username[32];
    char last_login[32];
    int32_t is_online;
} FriendInfo;

typedef struct {
    int32_t id;
    float x; float y;
    int32_t direction; int32_t is_moving; int32_t active;
    char username[64];
    int32_t status;
    int32_t role;
    uint8_t r, g, b;
    uint8_t r2, g2, b2;
    char map_name[32];
} Player;

typedef struct {
    int32_t type; // CHANGED: enum -> int32_t (Fixed 4 bytes)
    int32_t player_id; int32_t target_id;
    float dx; float dy;
    char msg[64];
    int32_t role_count;
    struct {
        int32_t id;
        char username[32];
        int32_t role;
    } roles[50];
    Player players[MAX_CLIENTS];
    char username[64]; char password[64];
    int32_t status; // CHANGED: enum -> int32_t
    FriendInfo friends[20]; 
    int32_t friend_count;
    int32_t response_accepted;
    uint32_t timestamp;
    int32_t new_status;
    uint8_t r, g, b;
    uint8_t r2, g2, b2;
    int32_t image_size;
    uint8_t image_data;
    char target_map[32]; 
    int32_t sanction_type;
    char sanction_reason[64];
    char ban_duration[16];
    int32_t warning_count;
    struct {
        char reason[64];
        char date[32];
    } warnings[20];
} Packet;

#pragma pack(pop)
#endif