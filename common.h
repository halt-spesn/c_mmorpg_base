#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define PORT 8888
#define MAX_CLIENTS 10
#define MAP_WIDTH 2000
#define MAP_HEIGHT 2000
#define PLAYER_SPEED 4.0f

// NEW: Status Enum
typedef enum {
    STATUS_ONLINE,
    STATUS_AFK,
    STATUS_DND,
    STATUS_ROLEPLAY,
    STATUS_TALK
} PlayerStatus;

typedef enum {
    PACKET_REGISTER_REQUEST,
    PACKET_LOGIN_REQUEST,
    PACKET_AUTH_RESPONSE,
    PACKET_JOIN,
    PACKET_MOVE,
    PACKET_CHAT,
    PACKET_UPDATE,
    PACKET_FRIEND_LIST,
    PACKET_FRIEND_REQUEST,
    PACKET_FRIEND_INCOMING,
    PACKET_FRIEND_RESPONSE,
    PACKET_FRIEND_REMOVE,
    PACKET_PING,
    PACKET_PRIVATE_MESSAGE,
    PACKET_STATUS_CHANGE // NEW
} PacketType;

typedef enum {
    AUTH_SUCCESS,
    AUTH_FAILURE,
    AUTH_REGISTER_SUCCESS,
    AUTH_REGISTER_FAILED_EXISTS,
    AUTH_REGISTER_FAILED_DB
} AuthStatus;

typedef struct {
    int id;
    float x;
    float y;
    int direction; 
    int is_moving; 
    int active;
    char username[32];
    int status; // NEW: Holds PlayerStatus
} Player;

typedef struct {
    PacketType type;
    int player_id;
    int target_id;
    float dx; 
    float dy;
    char msg[64];
    Player players[MAX_CLIENTS];
    char username[32];
    char password[32];
    AuthStatus status;
    int friend_ids[20]; 
    int friend_count;
    int response_accepted;
    uint32_t timestamp;
    int new_status; // NEW: Payload for status change
} Packet;

#endif