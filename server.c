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
    char *sql_users = "CREATE TABLE IF NOT EXISTS users(ID INTEGER PRIMARY KEY AUTOINCREMENT, USERNAME TEXT UNIQUE, PASSWORD TEXT, X REAL, Y REAL, R INT DEFAULT 255, G INT DEFAULT 255, B INT DEFAULT 0, ROLE INT DEFAULT 0, LAST_LOGIN TEXT DEFAULT 'Never');"; 
    sqlite3_exec(db, sql_users, 0, 0, 0);
    char *sql_friends = "CREATE TABLE IF NOT EXISTS friends(USER_ID INT, FRIEND_ID INT, STATUS INT, PRIMARY KEY (USER_ID, FRIEND_ID));";
    sqlite3_exec(db, sql_friends, 0, 0, 0);
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
    send(client_sockets[client_index], &pkt, sizeof(Packet), 0);
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

int login_user(const char *user, const char *pass, float *px, float *py, uint8_t *r, uint8_t *g, uint8_t *b, int *role) {
    sqlite3_stmt *stmt; char sql[256];
    snprintf(sql, 256, "SELECT X, Y, R, G, B, ROLE FROM users WHERE USERNAME=? AND PASSWORD=?;");
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return 0;
    sqlite3_bind_text(stmt, 1, user, -1, SQLITE_STATIC); sqlite3_bind_text(stmt, 2, pass, -1, SQLITE_STATIC);
    int logged_in = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) { 
        *px = (float)sqlite3_column_double(stmt, 0); *py = (float)sqlite3_column_double(stmt, 1);
        *r = (uint8_t)sqlite3_column_int(stmt, 2); *g = (uint8_t)sqlite3_column_int(stmt, 3); *b = (uint8_t)sqlite3_column_int(stmt, 4);
        *role = sqlite3_column_int(stmt, 5); logged_in = 1; 
    }
    if (logged_in) {
        char update_sql[256]; snprintf(update_sql, 256, "UPDATE users SET LAST_LOGIN=datetime('now', 'localtime') WHERE USERNAME='%s';", user);
        sqlite3_exec(db, update_sql, 0, 0, 0);
    }
    sqlite3_finalize(stmt); return logged_in;
}

void save_player_location(const char *user, float x, float y) {
    char sql[256]; snprintf(sql, 256, "UPDATE users SET X=%f, Y=%f WHERE USERNAME='%s';", x, y, user); sqlite3_exec(db, sql, 0, 0, NULL);
}

void init_game() {
    init_db(); init_storage();
    for (int i = 0; i < MAX_CLIENTS; i++) { client_sockets[i] = 0; players[i].active = 0; players[i].id = -1; }
}

void broadcast_state() {
    Packet pkt; pkt.type = PACKET_UPDATE;
    memcpy(pkt.players, players, sizeof(players));
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] > 0 && players[i].id != -1) send(client_sockets[i], &pkt, sizeof(Packet), 0);
    }
}

void broadcast_friend_update() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] > 0 && players[i].active) send_friend_list(i);
    }
}

void process_admin_command(int sender_idx, char *msg) {
    if (players[sender_idx].role != ROLE_ADMIN) {
        Packet err; err.type = PACKET_CHAT; err.player_id = -1; strcpy(err.msg, "Error: You do not have permission.");
        send(client_sockets[sender_idx], &err, sizeof(Packet), 0); return;
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
                send(client_sockets[i], &notif, sizeof(Packet), 0);
            }
        }
        broadcast_state();
    }
}

void handle_client_message(int index, Packet *pkt) {
    if (players[index].id == -1) {
        Packet response; response.type = PACKET_AUTH_RESPONSE;
        if (pkt->type == PACKET_REGISTER_REQUEST) {
            response.status = register_user(pkt->username, pkt->password);
        } else if (pkt->type == PACKET_LOGIN_REQUEST) {
            float x, y; uint8_t r, g, b; int role;
            if (login_user(pkt->username, pkt->password, &x, &y, &r, &g, &b, &role)) {
                 int already = 0; for(int i=0; i<MAX_CLIENTS; i++) if(players[i].active && strcmp(players[i].username, pkt->username)==0) already=1;
                 if(already) response.status = AUTH_FAILURE; 
                 else {
                    response.status = AUTH_SUCCESS; players[index].active = 1; players[index].id = get_user_id(pkt->username); 
                    if (players[index].id == 1) { role = ROLE_ADMIN; sqlite3_exec(db, "UPDATE users SET ROLE=1 WHERE ID=1;", 0, 0, 0); }
                    players[index].x = x; players[index].y = y; players[index].r = r; players[index].g = g; players[index].b = b; players[index].role = role; 
                    strncpy(players[index].username, pkt->username, 31);
                    response.player_id = players[index].id;
                    send(client_sockets[index], &response, sizeof(Packet), 0);
                    broadcast_friend_update(); broadcast_state(); return;
                 }
            } else response.status = AUTH_FAILURE;
        }
        send(client_sockets[index], &response, sizeof(Packet), 0); return;
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
        if (pkt->msg[0] == '/') {
            if (strncmp(pkt->msg, "/promote", 8) == 0 || strncmp(pkt->msg, "/demote", 7) == 0) process_admin_command(index, pkt->msg);
        } else {
            Packet chatPkt = *pkt; chatPkt.player_id = players[index].id;
            for (int i = 0; i < MAX_CLIENTS; i++) if (client_sockets[i] > 0 && players[i].id != -1) send(client_sockets[i], &chatPkt, sizeof(Packet), 0);
        }
    }
    else if (pkt->type == PACKET_FRIEND_REQUEST) {
        int target_idx = -1; for(int i=0; i<MAX_CLIENTS; i++) if (players[i].active && players[i].id == pkt->target_id) target_idx = i;
        if (target_idx != -1) { Packet req; req.type = PACKET_FRIEND_INCOMING; req.player_id = players[index].id; strcpy(req.username, players[index].username); send(client_sockets[target_idx], &req, sizeof(Packet), 0); }
    }
    else if (pkt->type == PACKET_FRIEND_RESPONSE) {
        if (pkt->response_accepted) {
            int requester_id = pkt->target_id; int my_id = players[index].id;
            char sql[256]; char *err_msg = 0;
            snprintf(sql, 256, "INSERT OR REPLACE INTO friends (USER_ID, FRIEND_ID, STATUS) VALUES (%d, %d, 1);", my_id, requester_id); sqlite3_exec(db, sql, 0, 0, &err_msg);
            snprintf(sql, 256, "INSERT OR REPLACE INTO friends (USER_ID, FRIEND_ID, STATUS) VALUES (%d, %d, 1);", requester_id, my_id); sqlite3_exec(db, sql, 0, 0, &err_msg);
            send_friend_list(index);
            for(int i=0; i<MAX_CLIENTS; i++) if(players[i].id == requester_id) send_friend_list(i);
        }
    }
    else if (pkt->type == PACKET_FRIEND_REMOVE) {
        int my_id = players[index].id; int target_id = pkt->target_id;
        char sql[256]; snprintf(sql, 256, "DELETE FROM friends WHERE (USER_ID=%d AND FRIEND_ID=%d) OR (USER_ID=%d AND FRIEND_ID=%d);", my_id, target_id, target_id, my_id);
        sqlite3_exec(db, sql, 0, 0, 0); send_friend_list(index);
        for(int i=0; i<MAX_CLIENTS; i++) if(players[i].active && players[i].id == target_id) send_friend_list(i);
    }
    else if (pkt->type == PACKET_PRIVATE_MESSAGE) {
        int target_id = pkt->target_id; int sender_id = players[index].id;
        Packet pm; pm.type = PACKET_PRIVATE_MESSAGE; pm.player_id = sender_id; pm.target_id = target_id; strncpy(pm.msg, pkt->msg, 64);
        int target_found = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) if (players[i].active && players[i].id == target_id) { if (client_sockets[i] > 0) { send(client_sockets[i], &pm, sizeof(Packet), 0); target_found = 1; } break; }
        if (target_found) send(client_sockets[index], &pm, sizeof(Packet), 0);
        else { Packet err; err.type = PACKET_CHAT; err.player_id = -1; strcpy(err.msg, "Player not online."); send(client_sockets[index], &err, sizeof(Packet), 0); }
    }
    else if (pkt->type == PACKET_PING) { send(client_sockets[index], pkt, sizeof(Packet), 0); }
    else if (pkt->type == PACKET_STATUS_CHANGE) { players[index].status = pkt->new_status; broadcast_state(); }
    else if (pkt->type == PACKET_COLOR_CHANGE) {
        players[index].r = pkt->r; players[index].g = pkt->g; players[index].b = pkt->b;
        char sql[256]; snprintf(sql, 256, "UPDATE users SET R=%d, G=%d, B=%d WHERE ID=%d;", pkt->r, pkt->g, pkt->b, players[index].id);
        sqlite3_exec(db, sql, 0, 0, 0); broadcast_state();
    }
    else if (pkt->type == PACKET_AVATAR_REQUEST) {
        int target = pkt->target_id; char filepath[64]; snprintf(filepath, 64, "avatars/%d.img", target);
        FILE *fp = fopen(filepath, "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END); int size = ftell(fp); fseek(fp, 0, SEEK_SET);
            if (size <= MAX_AVATAR_SIZE) {
                Packet resp; resp.type = PACKET_AVATAR_RESPONSE; resp.player_id = target; resp.image_size = size;
                send(client_sockets[index], &resp, sizeof(Packet), 0);
                uint8_t *file_buf = malloc(size); fread(file_buf, 1, size, fp);
                send(client_sockets[index], file_buf, size, 0); free(file_buf);
            }
            fclose(fp);
        }
    }
}

int recv_full(int sockfd, void *buf, size_t len) {
    size_t total = 0; size_t bytes_left = len; int n;
    while(total < len) {
        n = recv(sockfd, (char*)buf + total, bytes_left, 0); if(n <= 0) return n; 
        total += n; bytes_left -= n;
    } return total;
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
        int valread = recv(sd, &pkt, sizeof(Packet), MSG_WAITALL);
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

int main() {
    int server_fd, new_socket; struct sockaddr_in address;
    signal(SIGPIPE, SIG_IGN); // Ignore broken pipe errors
    init_game();
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) exit(1);
    int opt = 1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET; address.sin_addr.s_addr = INADDR_ANY; address.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { perror("Bind failed"); exit(1); }
    if (listen(server_fd, MAX_CLIENTS) < 0) { perror("Listen failed"); exit(1); }
    printf("Server running on port %d...\n", PORT);

    // Start Tick Thread
    pthread_t ticker;
    if (pthread_create(&ticker, NULL, tick_thread, NULL) != 0) { perror("Failed to start tick thread"); return 1; }

    while (1) {
        socklen_t addrlen = sizeof(address);
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) { perror("Accept failed"); continue; }
        pthread_mutex_lock(&state_mutex);
        int slot = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) { if (client_sockets[i] == 0) { client_sockets[i] = new_socket; players[i].id = -1; slot = i; break; } }
        pthread_mutex_unlock(&state_mutex);
        if (slot != -1) {
            int *arg = malloc(sizeof(int)); *arg = slot;
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, client_handler, arg) != 0) { perror("Thread create failed"); close(new_socket); }
            else pthread_detach(thread_id);
        } else { printf("Server full.\n"); close(new_socket); }
    }
    return 0;
}