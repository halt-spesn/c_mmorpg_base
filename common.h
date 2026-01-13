#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define PORT 8888
#define MAX_CLIENTS 10
#define MAP_WIDTH 2000
#define MAP_HEIGHT 2000
#define PLAYER_SPEED 4.0f
#define MAX_AVATAR_SIZE 16384
#define MAX_INVENTORY_SLOTS 20
#define MAX_GROUND_ITEMS 100 

// Keep Enums for code readability, but don't use them in the struct directly
typedef enum {
    STATUS_ONLINE, STATUS_AFK, STATUS_DND, STATUS_ROLEPLAY, STATUS_TALK
} PlayerStatus;

typedef enum {
    ROLE_PLAYER, ROLE_ADMIN, ROLE_DEVELOPER, ROLE_CONTRIBUTOR, ROLE_VIP
} PlayerRole;

typedef enum {
    ITEM_TYPE_CONSUMABLE, ITEM_TYPE_WEAPON, ITEM_TYPE_ARMOR, 
    ITEM_TYPE_ACCESSORY, ITEM_TYPE_MATERIAL, ITEM_TYPE_QUEST
} ItemType;

typedef enum {
    NPC_TYPE_QUEST, NPC_TYPE_MERCHANT, NPC_TYPE_GENERIC
} NPCType;

typedef enum {
    QUEST_STATUS_ACTIVE, QUEST_STATUS_COMPLETED, QUEST_STATUS_FAILED
} QuestStatus;

typedef enum {
    OBJECTIVE_TYPE_KILL, OBJECTIVE_TYPE_COLLECT, OBJECTIVE_TYPE_VISIT
} ObjectiveType;

typedef enum {
    CHAT_CHANNEL_GLOBAL, CHAT_CHANNEL_LOCAL, CHAT_CHANNEL_TRADE, 
    CHAT_CHANNEL_GUILD, CHAT_CHANNEL_WHISPER
} ChatChannel;

typedef enum {
    EQUIP_SLOT_WEAPON, EQUIP_SLOT_HELMET, EQUIP_SLOT_CHEST,
    EQUIP_SLOT_LEGS, EQUIP_SLOT_BOOTS, EQUIP_SLOT_ACCESSORY,
    EQUIP_SLOT_COUNT
} EquipmentSlot;

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
    PACKET_KICK, PACKET_MAP_CHANGE, PACKET_TRIGGERS_DATA,
    PACKET_CHANGE_PASSWORD_REQUEST, PACKET_CHANGE_PASSWORD_RESPONSE,
    PACKET_TELEMETRY, PACKET_TYPING,
    PACKET_INVENTORY_UPDATE, PACKET_ITEM_PICKUP, PACKET_ITEM_DROP,
    PACKET_ITEM_USE, PACKET_ITEM_EQUIP, PACKET_ITEM_UNEQUIP, PACKET_GROUND_ITEMS,
    PACKET_NPC_LIST, PACKET_NPC_INTERACT, PACKET_DIALOGUE,
    PACKET_QUEST_LIST, PACKET_QUEST_ACCEPT, PACKET_QUEST_COMPLETE, PACKET_QUEST_ABANDON, PACKET_QUEST_PROGRESS,
    PACKET_SHOP_OPEN, PACKET_SHOP_BUY, PACKET_SHOP_SELL,
    PACKET_TRADE_REQUEST, PACKET_TRADE_RESPONSE, PACKET_TRADE_OFFER, PACKET_TRADE_CONFIRM, PACKET_TRADE_CANCEL,
    PACKET_CURRENCY_UPDATE, PACKET_ENEMY_LIST, PACKET_ENEMY_ATTACK
} PacketType;

typedef enum {
    AUTH_SUCCESS, AUTH_FAILURE, AUTH_REGISTER_SUCCESS,
    AUTH_REGISTER_FAILED_EXISTS, AUTH_REGISTER_FAILED_DB
} AuthStatus;

// --- FORCE 1-BYTE PACKING ---
#pragma pack(push, 1)

typedef struct {
    int32_t item_id;      // Unique item ID from database
    int32_t quantity;     // Stack count
    int32_t type;         // ItemType enum
    char name[32];        // Item name
    char icon[16];        // Icon filename (e.g., "sword.png")
} Item;

typedef struct {
    Item item;
    int32_t slot_index;   // Inventory slot (0-19) or equipment slot
    int32_t is_equipped;  // 1 if equipped, 0 if in inventory
} InventorySlot;

typedef struct {
    int32_t item_id;
    float x, y;           // Position on map
    int32_t quantity;
    char name[32];        // Item name for tooltip
    char map_name[32];
    uint32_t spawn_time;  // Server timestamp when dropped
} GroundItem;

typedef struct {
    int32_t npc_id;
    char name[32];
    float x, y;
    char map_name[32];
    int32_t npc_type;     // NPCType enum
    char icon[16];
} NPC;

typedef struct {
    int32_t dialogue_id;
    int32_t npc_id;
    char text[256];
    int32_t next_dialogue_id;
} Dialogue;

typedef struct {
    int32_t objective_id;
    int32_t quest_id;
    int32_t objective_type;  // ObjectiveType enum
    int32_t target_id;
    char target_name[32];
    int32_t required_count;
    int32_t current_count;   // For tracking progress
} QuestObjective;

typedef struct {
    int32_t quest_id;
    char name[64];
    char description[256];
    int32_t npc_id;
    int32_t reward_gold;
    int32_t reward_xp;
    int32_t reward_item_id;
    int32_t reward_item_qty;
    int32_t level_required;
    int32_t status;          // QuestStatus enum
    QuestObjective objectives[5];  // Max 5 objectives per quest
    int32_t objective_count;
} Quest;

typedef struct {
    int32_t shop_id;
    int32_t npc_id;
    Item items[20];       // Shop inventory
    int32_t buy_prices[20];
    int32_t sell_prices[20];
    int32_t item_count;
} ShopData;

typedef struct {
    char src_map[32];
    int32_t rect_x;
    int32_t rect_y;
    int32_t rect_w;
    int32_t rect_h;
    char target_map[32];
    int32_t spawn_x;
    int32_t spawn_y;
} TriggerData;

typedef struct {
    int32_t id;
    char username[32];
    char last_login[32];
    int32_t is_online;
} FriendInfo;

typedef struct {
    int32_t enemy_id;
    int32_t type;        // 1=Rat, 2=Goblin, etc.
    char name[32];
    float x, y;
    int32_t hp;
    int32_t max_hp;
    int32_t active;
    char map_name[32];
} Enemy;

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
    int32_t is_typing;
    InventorySlot inventory[MAX_INVENTORY_SLOTS];
    Item equipment[EQUIP_SLOT_COUNT];
    int32_t inventory_count;  // Number of items in inventory
    int32_t gold;             // Player currency
    int32_t xp;               // Experience points
    int32_t level;            // Player level
    // NOTE: Quest data NOT included in Player struct for broadcast_state (too large)
    // Quests are stored separately and sent via PACKET_QUEST_LIST
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
    int32_t trigger_count;
    TriggerData triggers[20];
    char gl_renderer[128];
    char os_info[128];
    // Inventory-related fields
    InventorySlot inventory_slots[MAX_INVENTORY_SLOTS];
    int32_t inventory_count;
    Item equipment[EQUIP_SLOT_COUNT];  // Equipment slots
    Item item_data;               // Single item for pickup/drop/use
    int32_t item_slot;            // Slot index for operations
    int32_t equip_slot;           // Equipment slot for equip operations
    GroundItem ground_items[MAX_GROUND_ITEMS];
    int32_t ground_item_count;
    // Currency and level
    int32_t gold;
    int32_t xp;
    int32_t level;
    // NPC and quest fields
    NPC npcs[20];
    int32_t npc_count;
    int32_t npc_id;               // For NPC interactions
    Dialogue dialogue;            // Current dialogue
    Quest quests[3];              // Reduced from 10 to 3 for packet size
    int32_t quest_count;
    int32_t quest_id;             // For quest operations
    ShopData shop_data;           // Shop contents
    int32_t buy_sell_item_id;     // Item ID for shop transactions
    int32_t buy_sell_quantity;    // Quantity for shop transactions
    // Enemy fields
    Enemy enemies[20];
    int32_t enemy_count;
    int32_t enemy_id;             // For enemy interactions/combat
    // Trade fields
    int32_t trade_partner_id;
    Item trade_offer_items[10];
    int32_t trade_offer_count;
    int32_t trade_offer_gold;
    int32_t trade_confirmed;
    // Chat channel
    int32_t chat_channel;         // ChatChannel enum
} Packet;

#pragma pack(pop)
#endif