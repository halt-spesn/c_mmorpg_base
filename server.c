#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sqlite3.h>
#include <math.h>
#include "common.h"
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h> // Added

Player players[MAX_CLIENTS];
int client_sockets[MAX_CLIENTS];
sqlite3 *db;
int next_player_id = 100;

pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

// --- Prototypes ---
void broadcast_state(); 

// --- Database ---
int get_role_id_from_name(char *name) {
    if (strcasecmp(name, "Admin") == 0) return ROLE_ADMIN;
    if (strcasecmp(name, "Dev") == 0) return ROLE_DEVELOPER;
    if (strcasecmp(name, "Contrib") == 0) return ROLE_CONTRIBUTOR;
    if (strcasecmp(name, "VIP") == 0) return ROLE_VIP;
    return -1; 
}

void init_db() {
    int rc = sqlite3_open("mmorpg.db", &db);
    if (rc) exit(1);
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", 0, 0, 0);
 char *sql_users = "CREATE TABLE IF NOT EXISTS users("
                "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
                "USERNAME TEXT NOT NULL UNIQUE,"
                "PASSWORD TEXT NOT NULL,"
                "X REAL NOT NULL,"
                "Y REAL NOT NULL,"
                "R INTEGER DEFAULT 255,"
                "G INTEGER DEFAULT 255,"
                "B INTEGER DEFAULT 0,"
                "ROLE INTEGER DEFAULT 0,"
                "LAST_LOGIN TEXT DEFAULT 'Never',"
                "LAST_NICK_CHANGE INTEGER DEFAULT 0);"; // NEW COLUMN (Unix Timestamp)
    sqlite3_exec(db, sql_users, 0, 0, 0);
    char *sql_friends = "CREATE TABLE IF NOT EXISTS friends(USER_ID INT, FRIEND_ID INT, STATUS INT, PRIMARY KEY (USER_ID, FRIEND_ID));";
    sqlite3_exec(db, sql_friends, 0, 0, 0);
    char *alter1 = "ALTER TABLE users ADD COLUMN WARN_COUNT INTEGER DEFAULT 0;";
    sqlite3_exec(db, alter1, 0, 0, 0);
    char *alter2 = "ALTER TABLE users ADD COLUMN BAN_EXPIRE INTEGER DEFAULT 0;";
    sqlite3_exec(db, alter2, 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN R2 INTEGER DEFAULT 255;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN G2 INTEGER DEFAULT 255;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN B2 INTEGER DEFAULT 255;", 0, 0, 0);
    sqlite3_exec(db, "ALTER TABLE users ADD COLUMN MAP TEXT DEFAULT 'map.jpg';", 0, 0, 0);

    // 2. Create Warnings Table
    char *sql_warn = 
        "CREATE TABLE IF NOT EXISTS warnings ("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "USER_ID INTEGER, "
        "REASON TEXT, "
        "TIMESTAMP INTEGER);";
    sqlite3_exec(db, sql_warn, 0, 0, 0);
}

long parse_ban_duration(const char* str) {
    int val = atoi(str);
    char unit = str[strlen(str)-1];
    long multiplier = 1;
    
    switch(unit) {
        case 's': multiplier = 1; break;
        case 'm': multiplier = 60; break;
        case 'h': multiplier = 3600; break;
        case 'd': multiplier = 86400; break;
        case 'w': multiplier = 604800; break;
        case 'y': multiplier = 31536000; break;
        default: multiplier = 60; break; // Default to minutes if no unit
    }
    return val * multiplier;
}

void init_storage() {
    struct stat st = {0};
    if (stat("avatars", &st) == -1) mkdir("avatars", 0700);
}

int get_user_id(const char* username) {
    sqlite3_stmt *stmt; char sql[256];
    snprintf(sql, 256, "SELECT ID FROM users WHERE USERNAME=?;");
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) id = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt); return id;
}



AuthStatus register_user(const char *user, const char *pass) {
    char sql[256]; int count = 0; sqlite3_stmt *stmt;
    snprintf(sql, 256, "SELECT ID FROM users WHERE USERNAME=?;");
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0); sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) count = 1; sqlite3_finalize(stmt);
    if (count > 0) return AUTH_REGISTER_FAILED_EXISTS;
    snprintf(sql, 256, "INSERT INTO users (USERNAME, PASSWORD, X, Y, R, G, B) VALUES ('%s', '%s', 100.0, 100.0, 255, 255, 0);", user, pass);
    sqlite3_exec(db, sql, 0, 0, 0); return AUTH_REGISTER_SUCCESS;
}

// Update signature to accept 'long *ban_expire'
int login_user(const char *username, const char *password, float *x, float *y, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *r2, uint8_t *g2, uint8_t *b2, int *role, long *ban_expire, char *map_name_out) {
    sqlite3_stmt *stmt;
  const char *sql = "SELECT X, Y, R, G, B, ROLE, BAN_EXPIRE, R2, G2, B2, MAP FROM users WHERE USERNAME=? AND PASSWORD=?;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return 0;
    
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);
    
    int success = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *x = (float)sqlite3_column_double(stmt, 0);
        *y = (float)sqlite3_column_double(stmt, 1);
        *r = (uint8_t)sqlite3_column_int(stmt, 2);
        *g = (uint8_t)sqlite3_column_int(stmt, 3);
        *b = (uint8_t)sqlite3_column_int(stmt, 4);
        *role = sqlite3_column_int(stmt, 5);
        *ban_expire = (long)sqlite3_column_int64(stmt, 6); // Fetch Ban Time
        *r2 = (uint8_t)sqlite3_column_int(stmt, 7); // Index 7
        *g2 = (uint8_t)sqlite3_column_int(stmt, 8); // Index 8
        *b2 = (uint8_t)sqlite3_column_int(stmt, 9); // Index 9
        const unsigned char *m = sqlite3_column_text(stmt, 10); // Index 10
        if (m) strncpy(map_name_out, (const char*)m, 31);
        else strcpy(map_name_out, "map.jpg");
        success = 1;
    }
    sqlite3_finalize(stmt);
    return success;
}

void save_player_location(const char *user, float x, float y) {
    char sql[256]; snprintf(sql, 256, "UPDATE users SET X=%f, Y=%f WHERE USERNAME='%s';", x, y, user); sqlite3_exec(db, sql, 0, 0, NULL);
}

void init_game() {
    init_db(); init_storage();
    for (int i = 0; i < MAX_CLIENTS; i++) { client_sockets[i] = 0; players[i].active = 0; players[i].id = -1; }
}


// Updated to accept 'int flags' so it matches the send() signature
int send_all(int sockfd, void *buf, size_t len, int flags) {
    size_t total = 0;
    size_t bytes_left = len;
    int n;
    while(total < len) {
        // Pass the 'flags' (usually 0) through to the real send
        n = send(sockfd, (char*)buf + total, bytes_left, flags);
        if (n == -1) { break; }
        total += n;
        bytes_left -= n;
    }
    return (n == -1) ? -1 : 0; 
}

void broadcast_state() {
    Packet pkt; pkt.type = PACKET_UPDATE;
    memcpy(pkt.players, players, sizeof(players));
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] > 0 && players[i].id != -1) send_all(client_sockets[i], &pkt, sizeof(Packet), 0);
    }
}

void send_friend_list(int client_index) {
    int my_id = players[client_index].id; if (my_id == -1) return;
    Packet pkt; pkt.type = PACKET_FRIEND_LIST; pkt.friend_count = 0;
    char sql[512];
    snprintf(sql, 512, "SELECT f.FRIEND_ID, u.USERNAME, u.LAST_LOGIN FROM friends f JOIN users u ON f.FRIEND_ID = u.ID WHERE f.USER_ID=%d AND f.STATUS=1;", my_id);
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW && pkt.friend_count < 20) {
            int fid = sqlite3_column_int(stmt, 0);
            const unsigned char *fname = sqlite3_column_text(stmt, 1);
            const unsigned char *flogin = sqlite3_column_text(stmt, 2);
            pkt.friends[pkt.friend_count].id = fid;
            strncpy(pkt.friends[pkt.friend_count].username, (const char*)fname, 31);
            if (flogin) strncpy(pkt.friends[pkt.friend_count].last_login, (const char*)flogin, 31);
            else strcpy(pkt.friends[pkt.friend_count].last_login, "Unknown");
            pkt.friends[pkt.friend_count].is_online = 0;
            for(int i=0; i<MAX_CLIENTS; i++) if(players[i].active && players[i].id == fid) pkt.friends[pkt.friend_count].is_online = 1;
            pkt.friend_count++;
        }
        sqlite3_finalize(stmt);
    }
    send_all(client_sockets[client_index], &pkt, sizeof(Packet), 0);
}


void broadcast_friend_update() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] > 0 && players[i].active) send_friend_list(i);
    }
}

void process_admin_command(int sender_idx, char *msg) {
    if (players[sender_idx].role != ROLE_ADMIN) {
        Packet err; err.type = PACKET_CHAT; err.player_id = -1; strcpy(err.msg, "Error: You do not have permission.");
        send_all(client_sockets[sender_idx], &err, sizeof(Packet), 0); return;
    }
    char cmd[16], arg1[32], arg2[32];
    int args = sscanf(msg, "%s %s %s", cmd, arg1, arg2);
    int target_id = -1; int new_role = ROLE_PLAYER;
    if (strcmp(cmd, "/demote") == 0 && args >= 2) { target_id = atoi(arg1); new_role = ROLE_PLAYER; }
    else if (strcmp(cmd, "/promote") == 0 && args >= 3) { new_role = get_role_id_from_name(arg1); target_id = atoi(arg2); if(new_role==-1)return; } else return;

    if (target_id != -1) {
        char sql[256]; snprintf(sql, 256, "UPDATE users SET ROLE=%d WHERE ID=%d;", new_role, target_id);
        sqlite3_exec(db, sql, 0, 0, 0);
        for(int i=0; i<MAX_CLIENTS; i++) {
            if (players[i].active && players[i].id == target_id) {
                players[i].role = new_role;
                Packet notif; notif.type = PACKET_CHAT; notif.player_id = -1; snprintf(notif.msg, 64, "Role updated.");
                send_all(client_sockets[i], &notif, sizeof(Packet), 0);
            }
        }
        broadcast_state();
    }
}

void process_nick_change(int index, Packet *pkt) {
    char *new_nick = pkt->username; // We reuse the username field for the NEW nick
    char *password = pkt->password; // We reuse password field
    char *confirm  = pkt->msg;      // We reuse msg field for "CONFIRM"

    // 1. Validation
    if (strcmp(confirm, "CONFIRM") != 0) {
        Packet resp; resp.type = PACKET_CHANGE_NICK_RESPONSE; resp.status = AUTH_FAILURE;
        strcpy(resp.msg, "Type CONFIRM to proceed.");
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
        return;
    }

    if (strlen(new_nick) < 3 || strlen(new_nick) > 31) {
        Packet resp; resp.type = PACKET_CHANGE_NICK_RESPONSE; resp.status = AUTH_FAILURE;
        strcpy(resp.msg, "Name too short/long.");
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
        return;
    }

    // 2. Check DB
    sqlite3_stmt *stmt;
    char sql[256];
    
    // Verify Password & Time Limit
    snprintf(sql, 256, "SELECT PASSWORD, LAST_NICK_CHANGE FROM users WHERE ID=%d;", players[index].id);
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *db_pass = (const char*)sqlite3_column_text(stmt, 0);
        int last_change = sqlite3_column_int(stmt, 1);
        
        if (strcmp(password, db_pass) != 0) {
            Packet resp; resp.type = PACKET_CHANGE_NICK_RESPONSE; resp.status = AUTH_FAILURE;
            strcpy(resp.msg, "Wrong Password.");
            send_all(client_sockets[index], &resp, sizeof(Packet), 0);
            sqlite3_finalize(stmt);
            return;
        }

        // Time Check (30 Days = 2592000 seconds)
        time_t now = time(NULL);
        if (now - last_change < 2592000 && last_change != 0) {
            Packet resp; resp.type = PACKET_CHANGE_NICK_RESPONSE; resp.status = AUTH_FAILURE;
            int days_left = (2592000 - (now - last_change)) / 86400;
            snprintf(resp.msg, 64, "Wait %d more days.", days_left);
            send_all(client_sockets[index], &resp, sizeof(Packet), 0);
            sqlite3_finalize(stmt);
            return;
        }
    }
    sqlite3_finalize(stmt);

    // 3. Check Uniqueness
    snprintf(sql, 256, "SELECT ID FROM users WHERE USERNAME=?;");
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, new_nick, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Packet resp; resp.type = PACKET_CHANGE_NICK_RESPONSE; resp.status = AUTH_FAILURE;
        strcpy(resp.msg, "Name taken.");
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_finalize(stmt);

    // 4. Update DB
    snprintf(sql, 256, "UPDATE users SET USERNAME='%s', LAST_NICK_CHANGE=%ld WHERE ID=%d;", new_nick, time(NULL), players[index].id);
    if (sqlite3_exec(db, sql, 0, 0, 0) == SQLITE_OK) {
        // Update Memory
        strncpy(players[index].username, new_nick, 31);
        
        Packet resp; resp.type = PACKET_CHANGE_NICK_RESPONSE; resp.status = AUTH_SUCCESS;
        strcpy(resp.msg, "Nickname Changed!");
        strncpy(resp.username, new_nick, 31); // Send back new name
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
        
        // Broadcast updates
        broadcast_state();
        broadcast_friend_update(); // Updates friend lists with new name
    } else {
        Packet resp; resp.type = PACKET_CHANGE_NICK_RESPONSE; resp.status = AUTH_FAILURE;
        strcpy(resp.msg, "Database Error.");
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
    }
}

void send_pending_requests(int client_index) {
    int my_id = players[client_index].id;
    
    // Find rows where FRIEND_ID is ME, and STATUS is 0 (Pending)
    char sql[512];
    snprintf(sql, 512, 
        "SELECT f.USER_ID, u.USERNAME "
        "FROM friends f "
        "JOIN users u ON f.USER_ID = u.ID "
        "WHERE f.FRIEND_ID=%d AND f.STATUS=0;", my_id);

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Packet req; 
            req.type = PACKET_FRIEND_INCOMING;
            req.player_id = sqlite3_column_int(stmt, 0); // The requester's ID
            strncpy(req.username, (const char*)sqlite3_column_text(stmt, 1), 31);
            
            send_all(client_sockets[client_index], &req, sizeof(Packet), 0);
        }
        sqlite3_finalize(stmt);
    }
}

void handle_client_message(int index, Packet *pkt) {
    if (players[index].id == -1) {
        Packet response; 
        memset(&response, 0, sizeof(Packet)); // Good practice to clear it
        response.type = PACKET_AUTH_RESPONSE;

        if (pkt->type == PACKET_REGISTER_REQUEST) {
        if (pkt->username[0] == '\0' || pkt->password[0] == '\0') {
                response.status = AUTH_FAILURE;
                strcpy(response.msg, "Username and password required.");
            } else {
                response.status = register_user(pkt->username, pkt->password);
                if (response.status == AUTH_REGISTER_SUCCESS) strcpy(response.msg, "Registered.");
                else if (response.status == AUTH_REGISTER_FAILED_EXISTS) strcpy(response.msg, "User exists.");
                else strcpy(response.msg, "Registration failed.");
            }
        } 
        else if (pkt->type == PACKET_LOGIN_REQUEST) {
            float x, y; 
            uint8_t r, g, b;
            uint8_t r2, g2, b2; 
            int role;
            long ban_expire = 0;
            char db_map[32] = "map.jpg";

            // Pass &ban_expire to the function
            if (pkt->username[0] == '\0' || pkt->password[0] == '\0') {
                response.status = AUTH_FAILURE;
                strcpy(response.msg, "Username and password required.");
            }
            else if (login_user(pkt->username, pkt->password, &x, &y, &r, &g, &b, &r2, &g2, &b2, &role, &ban_expire, &db_map)) {
                
                // 1. Check Ban Logic
                if (time(NULL) < ban_expire) {
                    response.status = AUTH_FAILURE;
                    strcpy(response.msg, "Account is Banned."); // Fixed variable name
                    send_all(client_sockets[index], &response, sizeof(Packet), 0);
                    return;
                }

                // 2. Check Double Login
                int already = 0; 
                for(int i=0; i<MAX_CLIENTS; i++) {
                    if(players[i].active && strcmp(players[i].username, pkt->username) == 0) {
                        already = 1; break;
                    }
                }

                if(already) {
                    response.status = AUTH_FAILURE;
                    strcpy(response.msg, "Already logged in.");
                } 
                else {
                    response.status = AUTH_SUCCESS; 
                    players[index].active = 1; 
                    players[index].id = get_user_id(pkt->username); 
                    
                    // Auto-Admin ID 1 (Optional, generally safer to do in DB manually)
                    if (players[index].id == 1 && role != ROLE_ADMIN) { 
                        role = ROLE_ADMIN; 
                        sqlite3_exec(db, "UPDATE users SET ROLE=1 WHERE ID=1;", 0, 0, 0); 
                    }

                    players[index].x = x; 
                    players[index].y = y; 
                    players[index].r = r; 
                    players[index].g = g; 
                    players[index].b = b; 
                    players[index].r2 = r2; 
                    players[index].g2 = g2; 
                    players[index].b2 = b2;
                    players[index].role = role; 
                    strncpy(players[index].username, pkt->username, 31);
                    strncpy(players[index].map_name, db_map, 31);
                    
                    response.player_id = players[index].id;
                    send_all(client_sockets[index], &response, sizeof(Packet), 0);
                    
                    broadcast_friend_update(); 
                    send_pending_requests(index); 
                    broadcast_state(); 
                    return;
                }
            } else {
                response.status = AUTH_FAILURE;
                strcpy(response.msg, "Invalid username or password.");
            }
        }
        send_all(client_sockets[index], &response, sizeof(Packet), 0); 
        return;
    }

    if (pkt->type == PACKET_MOVE) {
        float length = sqrt(pkt->dx * pkt->dx + pkt->dy * pkt->dy);
        if (length > 0) {
            pkt->dx /= length; pkt->dy /= length;
            players[index].x += pkt->dx * PLAYER_SPEED; players[index].y += pkt->dy * PLAYER_SPEED;
            if (players[index].x < 0) players[index].x = 0; if (players[index].x > MAP_WIDTH - 32) players[index].x = MAP_WIDTH - 32;
            if (players[index].y < 0) players[index].y = 0; if (players[index].y > MAP_HEIGHT - 32) players[index].y = MAP_HEIGHT - 32;
        }
        // REMOVED: broadcast_state(); -- Handled by Tick Thread
    } 
    else if (pkt->type == PACKET_CHAT) {
        // --- ADMIN COMMANDS ---
        if (pkt->msg[0] == '/') {
            // 1. Security Check
            if (players[index].role < ROLE_ADMIN) {
                Packet err; err.type = PACKET_CHAT; err.player_id = -1;
                strcpy(err.msg, "Unknown command.");
                send_all(client_sockets[index], &err, sizeof(Packet), 0);
                return;
            }

            // 2. /unban <ID>
            if (strncmp(pkt->msg, "/unban ", 7) == 0) {
                int target_id = atoi(pkt->msg + 7);
                if (target_id > 0) {
                    char sql[256];
                    // Reset ban expiration to 0
                    snprintf(sql, 256, "UPDATE users SET BAN_EXPIRE=0 WHERE ID=%d;", target_id);
                    sqlite3_exec(db, sql, 0, 0, 0);
                    
                    Packet resp; resp.type = PACKET_CHAT; resp.player_id = -1;
                    snprintf(resp.msg, 64, "ID %d has been UNBANNED.", target_id);
                    send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                }
            }
            // 3. /unwarn <ID> (Remove last warning)
            else if (strncmp(pkt->msg, "/unwarn ", 8) == 0) {
                int target_id = atoi(pkt->msg + 8);
                if (target_id > 0) {
                    char sql[256];
                    
                    // A. Decrement user count (prevent negative)
                    snprintf(sql, 256, "UPDATE users SET WARN_COUNT = MAX(0, WARN_COUNT - 1) WHERE ID=%d;", target_id);
                    sqlite3_exec(db, sql, 0, 0, 0);

                    // B. Delete most recent warning entry
                    // SQLite specific: Delete row with highest ID for this user
                    snprintf(sql, 256, "DELETE FROM warnings WHERE ID = (SELECT MAX(ID) FROM warnings WHERE USER_ID=%d);", target_id);
                    sqlite3_exec(db, sql, 0, 0, 0);

                    Packet resp; resp.type = PACKET_CHAT; resp.player_id = -1;
                    snprintf(resp.msg, 64, "ID %d: Last warning removed.", target_id);
                    send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                }
            }
            // 4. /role <ID> <LEVEL> (Bonus: Set role via command)
            else if (strncmp(pkt->msg, "/role ", 6) == 0) {
                int target_id, level;
                if (sscanf(pkt->msg + 6, "%d %d", &target_id, &level) == 2) {
                    char sql[256];
                    snprintf(sql, 256, "UPDATE users SET ROLE=%d WHERE ID=%d;", level, target_id);
                    sqlite3_exec(db, sql, 0, 0, 0);
                    
                    // Update online player if active
                    for(int i=0; i<MAX_CLIENTS; i++) {
                        if(players[i].active && players[i].id == target_id) {
                            players[i].role = level;
                            broadcast_state(); // Update colors/tags for everyone
                        }
                    }
                    Packet resp; resp.type = PACKET_CHAT; resp.player_id = -1;
                    snprintf(resp.msg, 64, "ID %d Role set to %d.", target_id, level);
                    send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                }
            }
            else {
                Packet err; err.type = PACKET_CHAT; err.player_id = -1;
                strcpy(err.msg, "Invalid command.");
                send_all(client_sockets[index], &err, sizeof(Packet), 0);
            }
            return; // Don't broadcast commands
        }
        // -----------------------

        // Normal Chat Broadcast
        pkt->player_id = players[index].id;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] > 0 && players[i].active) {
                send_all(client_sockets[i], pkt, sizeof(Packet), 0);
            }
        }
    }
    else if (pkt->type == PACKET_FRIEND_REQUEST) {
        int target_id = pkt->target_id;
        int my_id = players[index].id;

        // 1. Prevent adding self
        if (target_id == my_id) {
            Packet err; err.type = PACKET_CHAT; err.player_id = -1;
            strcpy(err.msg, "Error: You cannot add yourself.");
            send_all(client_sockets[index], &err, sizeof(Packet), 0);
            return;
        }

        // 2. Check if Target ID exists in Database
        sqlite3_stmt *stmt;
        char sql[256];
        snprintf(sql, 256, "SELECT ID FROM users WHERE ID=%d;", target_id);
        sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
        
        int exists = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) exists = 1;
        sqlite3_finalize(stmt);

        if (!exists) {
            Packet err; err.type = PACKET_CHAT; err.player_id = -1;
            snprintf(err.msg, 64, "Error: User ID %d does not exist.", target_id);
            send_all(client_sockets[index], &err, sizeof(Packet), 0);
            return;
        }
        
        // 3. Check if already friends (Optional, prevents spamming DB)
        // Note: INSERT OR IGNORE below handles unique constraint, but logic here is safer
        
        // 4. Valid User -> Proceed with Request
        // Store in DB as Pending (Status 0)
        // INSERT OR IGNORE prevents duplicates if request already sent
        snprintf(sql, 256, "INSERT OR IGNORE INTO friends (USER_ID, FRIEND_ID, STATUS) VALUES (%d, %d, 0);", my_id, target_id);
        sqlite3_exec(db, sql, 0, 0, 0);

        // 5. If Online, notify target immediately
        int target_idx = -1;
        for(int i=0; i<MAX_CLIENTS; i++) if (players[i].active && players[i].id == target_id) target_idx = i;
        
        if (target_idx != -1) {
            Packet req; req.type = PACKET_FRIEND_INCOMING;
            req.player_id = players[index].id; 
            strcpy(req.username, players[index].username);
            send_all(client_sockets[target_idx], &req, sizeof(Packet), 0);
        }

        // 6. Confirm to Sender
        Packet succ; succ.type = PACKET_CHAT; succ.player_id = -1;
        snprintf(succ.msg, 64, "Friend request sent to ID %d.", target_id);
        send_all(client_sockets[index], &succ, sizeof(Packet), 0);
    }
    else if (pkt->type == PACKET_FRIEND_RESPONSE) {
        int requester_id = pkt->target_id;
        int my_id = players[index].id;
        char sql[256];

        if (pkt->response_accepted) {
            // A. Update the original request to Status 1
            snprintf(sql, 256, "UPDATE friends SET STATUS=1 WHERE USER_ID=%d AND FRIEND_ID=%d;", requester_id, my_id);
            sqlite3_exec(db, sql, 0, 0, 0);
            
            // B. Insert the reverse relationship as Status 1
            snprintf(sql, 256, "INSERT OR REPLACE INTO friends (USER_ID, FRIEND_ID, STATUS) VALUES (%d, %d, 1);", my_id, requester_id);
            sqlite3_exec(db, sql, 0, 0, 0);
            
            // Refresh
            send_friend_list(index);
            for(int i=0; i<MAX_CLIENTS; i++) if(players[i].id == requester_id) send_friend_list(i);
        } else {
            // Deny: Delete the pending request
            snprintf(sql, 256, "DELETE FROM friends WHERE USER_ID=%d AND FRIEND_ID=%d;", requester_id, my_id);
            sqlite3_exec(db, sql, 0, 0, 0);
        }
    }
    else if (pkt->type == PACKET_FRIEND_REMOVE) {
        int my_id = players[index].id; int target_id = pkt->target_id;
        // Delete both directions
        char sql[256]; snprintf(sql, 256, "DELETE FROM friends WHERE (USER_ID=%d AND FRIEND_ID=%d) OR (USER_ID=%d AND FRIEND_ID=%d);", my_id, target_id, target_id, my_id);
        sqlite3_exec(db, sql, 0, 0, 0);
        send_friend_list(index);
        for(int i=0; i<MAX_CLIENTS; i++) if(players[i].active && players[i].id == target_id) send_friend_list(i);
    }
    else if (pkt->type == PACKET_PRIVATE_MESSAGE) {
        int target_id = pkt->target_id; int sender_id = players[index].id;
        Packet pm; pm.type = PACKET_PRIVATE_MESSAGE; pm.player_id = sender_id; pm.target_id = target_id; strncpy(pm.msg, pkt->msg, 64);
        int target_found = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) if (players[i].active && players[i].id == target_id) { if (client_sockets[i] > 0) { send_all(client_sockets[i], &pm, sizeof(Packet), 0); target_found = 1; } break; }
        if (target_found) send_all(client_sockets[index], &pm, sizeof(Packet), 0);
        else { Packet err; err.type = PACKET_CHAT; err.player_id = -1; strcpy(err.msg, "Player not online."); send_all(client_sockets[index], &err, sizeof(Packet), 0); }
    }
    else if (pkt->type == PACKET_PING) { send_all(client_sockets[index], pkt, sizeof(Packet), 0); }
    else if (pkt->type == PACKET_STATUS_CHANGE) { players[index].status = pkt->new_status; broadcast_state(); }
    else if (pkt->type == PACKET_COLOR_CHANGE) {
        players[index].r = pkt->r; players[index].g = pkt->g; players[index].b = pkt->b;
        players[index].r2 = pkt->r2; players[index].g2 = pkt->g2; players[index].b2 = pkt->b2;
        char sql[256]; snprintf(sql, 256, "UPDATE users SET R=%d, G=%d, B=%d WHERE ID=%d;", pkt->r, pkt->g, pkt->b, players[index].id);
        snprintf(sql, 256, "UPDATE users SET R=%d, G=%d, B=%d, R2=%d, G2=%d, B2=%d WHERE ID=%d;", 
            pkt->r, pkt->g, pkt->b, pkt->r2, pkt->g2, pkt->b2, players[index].id);
        sqlite3_exec(db, sql, 0, 0, 0); broadcast_state();
    }
    else if (pkt->type == PACKET_AVATAR_REQUEST) {
        int target = pkt->target_id; char filepath[64]; snprintf(filepath, 64, "avatars/%d.img", target);
        FILE *fp = fopen(filepath, "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END); int size = ftell(fp); fseek(fp, 0, SEEK_SET);
            if (size <= MAX_AVATAR_SIZE) {
                Packet resp; resp.type = PACKET_AVATAR_RESPONSE; resp.player_id = target; resp.image_size = size;
                send_all(client_sockets[index], &resp, sizeof(Packet), 0);
                uint8_t *file_buf = malloc(size); fread(file_buf, 1, size, fp);
                send_all(client_sockets[index], file_buf, size, 0); free(file_buf);
            }
            fclose(fp);
        }
    }
    else if (pkt->type == PACKET_CHANGE_NICK_REQUEST) {
        process_nick_change(index, pkt);
    }
    else if (pkt->type == PACKET_ROLE_LIST_REQUEST) {
        Packet resp; 
        resp.type = PACKET_ROLE_LIST_RESPONSE;
        resp.role_count = 0;

        sqlite3_stmt *stmt;
        const char *sql = "SELECT ID, USERNAME, ROLE FROM users WHERE ROLE > 0 ORDER BY ROLE ASC;";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW && resp.role_count < 50) {
                int idx = resp.role_count;
                resp.roles[idx].id = sqlite3_column_int(stmt, 0);
                const unsigned char *name = sqlite3_column_text(stmt, 1);
                strncpy(resp.roles[idx].username, (const char*)name, 31);
                resp.roles[idx].role = sqlite3_column_int(stmt, 2);
                resp.role_count++;
            }
        }
        sqlite3_finalize(stmt);
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
    }
    else if (pkt->type == PACKET_SANCTION_REQUEST) {
        if (players[index].role < ROLE_ADMIN) return; // Security Check

        int target_id = pkt->target_id;
        
        if (pkt->sanction_type == 0) { // WARN
            // Add to DB
            sqlite3_stmt *stmt;
            char sql[256];
            snprintf(sql, 256, "INSERT INTO warnings (USER_ID, REASON, TIMESTAMP) VALUES (%d, ?, %ld);", target_id, time(NULL));
            sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
            sqlite3_bind_text(stmt, 1, pkt->sanction_reason, -1, SQLITE_STATIC);
            sqlite3_step(stmt); sqlite3_finalize(stmt);

            // Increment Count
            snprintf(sql, 256, "UPDATE users SET WARN_COUNT = WARN_COUNT + 1 WHERE ID=%d;", target_id);
            sqlite3_exec(db, sql, 0, 0, 0);

            // Check Auto-Ban (3 Strikes)
            snprintf(sql, 256, "SELECT WARN_COUNT FROM users WHERE ID=%d;", target_id);
            sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int count = sqlite3_column_int(stmt, 0);
                if (count >= 3) {
                    // Auto Ban (10 Years)
                    long expire = time(NULL) + (10 * 31536000);
                    char ban_sql[256];
                    snprintf(ban_sql, 256, "UPDATE users SET BAN_EXPIRE=%ld WHERE ID=%d;", expire, target_id);
                    sqlite3_exec(db, ban_sql, 0, 0, 0);
                    
                    // Kick if online
                    for(int i=0; i<MAX_CLIENTS; i++) if(players[i].active && players[i].id == target_id) {
                        Packet k; k.type = PACKET_KICK; strcpy(k.msg, "Banned: 3 Warnings Reached.");
                        send_all(client_sockets[i], &k, sizeof(Packet), 0);
                        close(client_sockets[i]); players[i].active = 0;
                    }
                }
            }
            sqlite3_finalize(stmt);
            
            // Notify Admin
            Packet msg; msg.type = PACKET_CHAT; msg.player_id = -1; strcpy(msg.msg, "Player Warned.");
            send_all(client_sockets[index], &msg, sizeof(Packet), 0);

        } 
        else { // BAN
            long duration = parse_ban_duration(pkt->ban_duration);
            long expire = time(NULL) + duration;
            
            char sql[256];
            snprintf(sql, 256, "UPDATE users SET BAN_EXPIRE=%ld WHERE ID=%d;", expire, target_id);
            sqlite3_exec(db, sql, 0, 0, 0);

            // Kick if online
            for(int i=0; i<MAX_CLIENTS; i++) if(players[i].active && players[i].id == target_id) {
                Packet k; k.type = PACKET_KICK; 
                snprintf(k.msg, 64, "Banned: %s", pkt->sanction_reason);
                send_all(client_sockets[i], &k, sizeof(Packet), 0);
                close(client_sockets[i]); players[i].active = 0;
            }
             Packet msg; msg.type = PACKET_CHAT; msg.player_id = -1; strcpy(msg.msg, "Player Banned.");
             send_all(client_sockets[index], &msg, sizeof(Packet), 0);
        }
    }

    // 3. FETCH WARNINGS (For User)
    else if (pkt->type == PACKET_WARNINGS_REQUEST) {
        Packet resp; resp.type = PACKET_WARNINGS_RESPONSE; resp.warning_count = 0;
        
        char sql[256];
        snprintf(sql, 256, "SELECT REASON, TIMESTAMP FROM warnings WHERE USER_ID=%d ORDER BY ID DESC LIMIT 20;", players[index].id);
        
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int i = resp.warning_count++;
                const char *r = (const char*)sqlite3_column_text(stmt, 0);
                time_t t = sqlite3_column_int64(stmt, 1);
                
                strncpy(resp.warnings[i].reason, r, 63);
                struct tm *tm_info = localtime(&t);
                strftime(resp.warnings[i].date, 32, "%Y-%m-%d", tm_info);
            }
        }
        sqlite3_finalize(stmt);
        send_all(client_sockets[index], &resp, sizeof(Packet), 0);
    }
    else if (pkt->type == PACKET_MAP_CHANGE) {
        strncpy(players[index].map_name, pkt->target_map, 31);
        players[index].x = pkt->dx; // Use dx/dy for spawn coords
        players[index].y = pkt->dy;
        
        // Save to DB
        char sql[256];
        snprintf(sql, 256, "UPDATE users SET MAP='%s', X=%f, Y=%f WHERE ID=%d;", 
            players[index].map_name, players[index].x, players[index].y, players[index].id);
        sqlite3_exec(db, sql, 0, 0, 0);
        
        broadcast_state(); // Tell everyone I moved to a new dimension
    }
}

int recv_full(int sockfd, void *buf, size_t len) {
    size_t total = 0;
    size_t bytes_left = len;
    int n;
    while(total < len) {
        n = recv(sockfd, (char*)buf + total, bytes_left, 0); 
        if(n <= 0) return n; // Error or disconnect
        total += n; 
        bytes_left -= n;
    }
    return total;
}

// --- NEW TICK THREAD ---
void *tick_thread(void *arg) {
    while (1) {
        usleep(50000); // 50ms (20 updates/sec)
        pthread_mutex_lock(&state_mutex);
        broadcast_state();
        pthread_mutex_unlock(&state_mutex);
    }
    return NULL;
}

void *client_handler(void *arg) {
    int index = *(int*)arg; free(arg); int sd = client_sockets[index]; Packet pkt;
    while (1) {
        int valread = recv_full(sd, &pkt, sizeof(Packet));
        if (valread <= 0) {
            pthread_mutex_lock(&state_mutex);
            close(sd); client_sockets[index] = 0;
            if(players[index].active) {
                save_player_location(players[index].username, players[index].x, players[index].y);
                char sql[256]; snprintf(sql, 256, "UPDATE users SET LAST_LOGIN=datetime('now', 'localtime') WHERE ID=%d;", players[index].id);
                sqlite3_exec(db, sql, 0, 0, 0);
            }
            players[index].active = 0; broadcast_state(); broadcast_friend_update();
            pthread_mutex_unlock(&state_mutex);
            break;
        }
        if (pkt.type == PACKET_AVATAR_UPLOAD) {
            if (pkt.image_size > 0 && pkt.image_size <= MAX_AVATAR_SIZE) {
                uint8_t *img_buf = malloc(pkt.image_size);
                if (recv_full(sd, img_buf, pkt.image_size) == pkt.image_size) {
                    pthread_mutex_lock(&state_mutex); 
                    char filepath[64]; snprintf(filepath, 64, "avatars/%d.img", players[index].id);
                    FILE *fp = fopen(filepath, "wb");
                    if (fp) { fwrite(img_buf, 1, pkt.image_size, fp); fclose(fp); printf("Saved avatar User %d\n", players[index].id); }
                    pthread_mutex_unlock(&state_mutex);
                }
                free(img_buf);
            }
        } else {
            pthread_mutex_lock(&state_mutex);
            handle_client_message(index, &pkt);
            pthread_mutex_unlock(&state_mutex);
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    //printf("DEBUG: Packet Size is %d bytes\n", (int)sizeof(Packet));
    int server_fd, new_socket; 
    struct sockaddr_in address;
    
    // 1. Set Default Port
    int current_port = PORT; // Default 8888 from common.h

    // 2. Parse Command Line Arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            current_port = atoi(argv[i+1]);
            if (current_port <= 0 || current_port > 65535) {
                printf("Invalid port number. Using default %d.\n", PORT);
                current_port = PORT;
            }
        }
    }

    signal(SIGPIPE, SIG_IGN); 
    init_game();
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) exit(1);
    
    int opt = 1; 
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    
    // 3. Use the variable port
    address.sin_port = htons(current_port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { 
        perror("Bind failed"); 
        exit(1); 
    }
    
    if (listen(server_fd, MAX_CLIENTS) < 0) { 
        perror("Listen failed"); 
        exit(1); 
    }
    
    printf("Server running on port %d...\n", current_port);

    // Start Tick Thread
    pthread_t ticker;
    if (pthread_create(&ticker, NULL, tick_thread, NULL) != 0) { 
        perror("Failed to start tick thread"); 
        return 1; 
    }

    while (1) {
        socklen_t addrlen = sizeof(address);
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) { 
            perror("Accept failed"); 
            continue; 
        }
        
        pthread_mutex_lock(&state_mutex);
        int slot = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) { 
            if (client_sockets[i] == 0) { 
                client_sockets[i] = new_socket; 
                players[i].id = -1; 
                slot = i; 
                break; 
            } 
        }
        pthread_mutex_unlock(&state_mutex);
        
        if (slot != -1) {
            int *arg = malloc(sizeof(int)); 
            *arg = slot;
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, client_handler, arg) != 0) { 
                perror("Thread create failed"); 
                close(new_socket); 
            } else {
                pthread_detach(thread_id);
            }
        } else { 
            printf("Server full.\n"); 
            close(new_socket); 
        }
    }
    return 0;
}