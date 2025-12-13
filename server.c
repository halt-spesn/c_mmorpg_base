#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sqlite3.h>
#include <math.h>
#include "common.h"

Player players[MAX_CLIENTS];
int client_sockets[MAX_CLIENTS];
sqlite3 *db;
int next_player_id = 100;

// --- Database (Standard Functions) ---

void init_db() {
    int rc = sqlite3_open("mmorpg.db", &db);
    if (rc) exit(1);
    
    char *sql_users = "CREATE TABLE IF NOT EXISTS users("
                "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
                "USERNAME TEXT NOT NULL UNIQUE,"
                "PASSWORD TEXT NOT NULL,"
                "X REAL NOT NULL,"
                "Y REAL NOT NULL);";
    sqlite3_exec(db, sql_users, 0, 0, 0);

    // Friends Table
    char *sql_friends = "CREATE TABLE IF NOT EXISTS friends("
                        "USER_ID INTEGER,"
                        "FRIEND_ID INTEGER,"
                        "STATUS INTEGER,"
                        "PRIMARY KEY (USER_ID, FRIEND_ID));";
    sqlite3_exec(db, sql_friends, 0, 0, 0);
}

// Helper to get ID from Username
int get_user_id(const char* username) {
    sqlite3_stmt *stmt;
    char sql[256];
    snprintf(sql, 256, "SELECT ID FROM users WHERE USERNAME=?;");
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    int id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) id = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return id;
}

void send_friend_list(int client_index) {
    int my_id = players[client_index].id;
    if (my_id == -1) return;

    Packet pkt;
    pkt.type = PACKET_FRIEND_LIST;
    pkt.friend_count = 0;

    // Get friends where STATUS=1 (Accepted)
    char sql[256];
    snprintf(sql, 256, "SELECT FRIEND_ID FROM friends WHERE USER_ID=%d AND STATUS=1;", my_id);
    
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    while (sqlite3_step(stmt) == SQLITE_ROW && pkt.friend_count < 20) {
        pkt.friend_ids[pkt.friend_count++] = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    send(client_sockets[client_index], &pkt, sizeof(Packet), 0);
}

// ... (Auth/DB Functions - Condensed for brevity, they remain the same) ...
AuthStatus register_user(const char *user, const char *pass) {
    char sql[256]; int count = 0; sqlite3_stmt *stmt;
    snprintf(sql, 256, "SELECT ID FROM users WHERE USERNAME=?;");
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0); sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) count = 1; sqlite3_finalize(stmt);
    if (count > 0) return AUTH_REGISTER_FAILED_EXISTS;
    snprintf(sql, 256, "INSERT INTO users (USERNAME, PASSWORD, X, Y) VALUES ('%s', '%s', 100.0, 100.0);", user, pass);
    sqlite3_exec(db, sql, 0, 0, 0); return AUTH_REGISTER_SUCCESS;
}

int login_user(const char *user, const char *pass, float *player_x, float *player_y) {
    sqlite3_stmt *stmt; char sql[256];
    snprintf(sql, 256, "SELECT X, Y FROM users WHERE USERNAME=? AND PASSWORD=?;");
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return 0;
    sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC); sqlite3_bind_text(stmt, 2, pass, -1, SQLITE_STATIC);
    int logged_in = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) { *player_x = (float)sqlite3_column_double(stmt, 0); *player_y = (float)sqlite3_column_double(stmt, 1); logged_in = 1; }
    sqlite3_finalize(stmt); return logged_in;
}
void save_player_location(const char *user, float x, float y) {
    char sql[256]; snprintf(sql, 256, "UPDATE users SET X=%f, Y=%f WHERE USERNAME='%s';", x, y, user); sqlite3_exec(db, sql, 0, 0, NULL);
}

void init_game() {
    init_db();
    for (int i = 0; i < MAX_CLIENTS; i++) { client_sockets[i] = 0; players[i].active = 0; players[i].id = -1; }
}

void broadcast_state() {
    Packet pkt; pkt.type = PACKET_UPDATE;
    memcpy(pkt.players, players, sizeof(players));
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] > 0 && players[i].id != -1) send(client_sockets[i], &pkt, sizeof(Packet), 0);
    }
}

void handle_client_message(int index, Packet *pkt) {
    if (players[index].id == -1) {
        // Auth Logic
        Packet response; response.type = PACKET_AUTH_RESPONSE;
        if (pkt->type == PACKET_REGISTER_REQUEST) {
            response.status = register_user(pkt->username, pkt->password);
        } else if (pkt->type == PACKET_LOGIN_REQUEST) {
            float x, y;
            if (login_user(pkt->username, pkt->password, &x, &y)) {
                 int already = 0; for(int i=0; i<MAX_CLIENTS; i++) if(players[i].active && strcmp(players[i].username, pkt->username)==0) already=1;
                 if(already) response.status = AUTH_FAILURE; 
                 else {
                    response.status = AUTH_SUCCESS;
                    players[index].active = 1;
                    players[index].id = get_user_id(pkt->username); 
                    players[index].x = x; players[index].y = y;
                    strncpy(players[index].username, pkt->username, 31);
                    response.player_id = players[index].id;
                    send(client_sockets[index], &response, sizeof(Packet), 0);
                    send_friend_list(index);
                    broadcast_state(); 
                    return;
                 }
            } else response.status = AUTH_FAILURE;
        }
        send(client_sockets[index], &response, sizeof(Packet), 0);
        return;
    }

    if (pkt->type == PACKET_MOVE) {
        float length = sqrt(pkt->dx * pkt->dx + pkt->dy * pkt->dy);
        if (length > 0) {
            pkt->dx /= length; pkt->dy /= length;
            players[index].x += pkt->dx * PLAYER_SPEED;
            players[index].y += pkt->dy * PLAYER_SPEED;
            if (players[index].x < 0) players[index].x = 0;
            if (players[index].x > MAP_WIDTH - 32) players[index].x = MAP_WIDTH - 32;
            if (players[index].y < 0) players[index].y = 0;
            if (players[index].y > MAP_HEIGHT - 32) players[index].y = MAP_HEIGHT - 32;
        }
        broadcast_state();
    } 
    else if (pkt->type == PACKET_CHAT) {
        Packet chatPkt = *pkt;
        chatPkt.player_id = players[index].id;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] > 0 && players[i].id != -1) send(client_sockets[i], &chatPkt, sizeof(Packet), 0);
        }
    }
    else if (pkt->type == PACKET_FRIEND_REQUEST) {
        // Find target socket
        int target_idx = -1;
        for(int i=0; i<MAX_CLIENTS; i++) {
            if (players[i].active && players[i].id == pkt->target_id) {
                target_idx = i; break;
            }
        }
        if (target_idx != -1) {
            Packet req; 
            req.type = PACKET_FRIEND_INCOMING;
            req.player_id = players[index].id; // Sender ID
            strcpy(req.username, players[index].username);
            send(client_sockets[target_idx], &req, sizeof(Packet), 0);
        }
    }
    else if (pkt->type == PACKET_FRIEND_RESPONSE) {
        if (pkt->response_accepted) {
            int requester_id = pkt->target_id;
            int my_id = players[index].id;
            char sql[512];
            snprintf(sql, 512, "INSERT OR REPLACE INTO friends (USER_ID, FRIEND_ID, STATUS) VALUES (%d, %d, 1), (%d, %d, 1);", 
                     my_id, requester_id, requester_id, my_id);
            sqlite3_exec(db, sql, 0, 0, 0);
            
            // Update lists
            send_friend_list(index);
            for(int i=0; i<MAX_CLIENTS; i++) if(players[i].id == requester_id) send_friend_list(i);
        }
    }
    else if (pkt->type == PACKET_FRIEND_REMOVE) {
        int my_id = players[index].id;
        int target_id = pkt->target_id;

        // Delete BOTH sides of the relationship to cleanly break the friendship
        char sql[256];
        snprintf(sql, 256, "DELETE FROM friends WHERE (USER_ID=%d AND FRIEND_ID=%d) OR (USER_ID=%d AND FRIEND_ID=%d);", 
                 my_id, target_id, target_id, my_id);
        
        char *err_msg = 0;
        sqlite3_exec(db, sql, 0, 0, &err_msg);
        
        // Update MY list
        send_friend_list(index);
        
        // Update THE EX-FRIEND'S list (if they are online)
        for(int i=0; i<MAX_CLIENTS; i++) {
            if(players[i].active && players[i].id == target_id) {
                send_friend_list(i);
                break;
           }
        }
    }
    else if (pkt->type == PACKET_PRIVATE_MESSAGE) {
        int target_id = pkt->target_id;
        int sender_id = players[index].id;
        
        // Prepare packet
        Packet pm;
        pm.type = PACKET_PRIVATE_MESSAGE;
        pm.player_id = sender_id;      // Who sent it
        pm.target_id = target_id;      // Who it is for
        strncpy(pm.msg, pkt->msg, 64);

        // 1. Send to Target
        int target_found = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (players[i].active && players[i].id == target_id) {
                if (client_sockets[i] > 0) {
                    send(client_sockets[i], &pm, sizeof(Packet), 0);
                    target_found = 1;
                }
                break;
            }
        }

        // 2. Send Echo back to Sender (so it appears in their chat)
        // If target not found, maybe send a system error message instead?
        if (target_found) {
            send(client_sockets[index], &pm, sizeof(Packet), 0);
        } else {
            Packet err; err.type = PACKET_CHAT; err.player_id = -1;
            strcpy(err.msg, "Player not online.");
            send(client_sockets[index], &err, sizeof(Packet), 0);
        }
    }
    else if (pkt->type == PACKET_PING) {
        // Echo back immediately
        send(client_sockets[index], pkt, sizeof(Packet), 0);
    }
}

int main() {
    int server_fd, new_socket, max_sd, sd;
    struct sockaddr_in address; fd_set readfds;
    init_game();
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) exit(1);
    int opt = 1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET; address.sin_addr.s_addr = INADDR_ANY; address.sin_port = htons(PORT);
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);
    printf("Server with GUI Friends system running...\n");

    while (1) {
        FD_ZERO(&readfds); FD_SET(server_fd, &readfds); max_sd = server_fd;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }
        select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(server_fd, &readfds)) {
            socklen_t addrlen = sizeof(address);
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) >= 0) {
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (client_sockets[i] == 0) { client_sockets[i] = new_socket; players[i].id = -1; break; }
                }
            }
        }
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_sockets[i];
            if (FD_ISSET(sd, &readfds)) {
                Packet pkt;
                if (read(sd, &pkt, sizeof(Packet)) == 0) {
                    close(sd); client_sockets[i] = 0;
                    if(players[i].active) save_player_location(players[i].username, players[i].x, players[i].y);
                    players[i].active = 0; broadcast_state();
                } else { handle_client_message(i, &pkt); }
            }
        }
    }
    return 0;
}