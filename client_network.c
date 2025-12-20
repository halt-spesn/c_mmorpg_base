#include "client.h"

int send_all(socket_t sockfd, const void *buf, size_t len) {
    size_t total = 0;
    size_t bytes_left = len;
    int n;

    while(total < len) {
        n = send(sockfd, (const char*)buf + total, bytes_left, 0);
        if (n == -1) { break; }
        total += n;
        bytes_left -= n;
    }
    return (n == -1) ? -1 : 0; // return -1 on failure, 0 on success
}

void send_packet(Packet *pkt) { 
    send_all(sock, pkt, sizeof(Packet)); 
}

int recv_total(socket_t sockfd, void *buf, size_t len) {
    size_t total = 0;
    size_t bytes_left = len;
    int n;
    while(total < len) {
        // Note: Using flag 0 instead of MSG_WAITALL
        n = recv(sockfd, (char*)buf + total, bytes_left, 0); 
        if(n <= 0) return n; // Error or disconnect
        total += n; 
        bytes_left -= n;
    }
    return total;
}

int try_connect(void) {
    if (is_connected) close(sock);
    
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == (socket_t)-1) {
        strcpy(auth_message, "Socket Error");
        return 0;
    }
    
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(atoi(input_port));
    
    // Try inet_pton first (for IP addresses)
    if (inet_pton(AF_INET, input_ip, &serv_addr.sin_addr) <= 0) {
        // If inet_pton fails, try getaddrinfo (for domain names)
        struct addrinfo hints, *result = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        if (getaddrinfo(input_ip, NULL, &hints, &result) != 0 || result == NULL) {
            if (result) freeaddrinfo(result);
            strcpy(auth_message, "Invalid Address");
            close(sock);
            return 0;
        }
        
        // Copy the resolved address
        memcpy(&serv_addr.sin_addr, &((struct sockaddr_in*)result->ai_addr)->sin_addr, sizeof(struct in_addr));
        freeaddrinfo(result);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        strcpy(auth_message, "Connection Failed");
        return 0;
    }

    strcpy(server_ip, input_ip); 

    is_connected = 1;
    strcpy(auth_message, "Connected! Logging in...");
    return 1;
}

void get_os_info(char *buffer, size_t size) {
    #ifdef _WIN32
    snprintf(buffer, size, "Windows");
    #else
    struct utsname u;
    if (uname(&u) == 0) {
        snprintf(buffer, size, "%s %s", u.sysname, u.release);
    } else {
        snprintf(buffer, size, "Unknown");
    }
    #endif
}

void send_telemetry(void) {
    Packet pkt;
    memset(&pkt, 0, sizeof(Packet));
    pkt.type = PACKET_TELEMETRY;
    pkt.player_id = local_player_id;
    
    // Get GL renderer info
    if (gl_renderer_cache[0]) {
        strncpy(pkt.gl_renderer, gl_renderer_cache, 127);
        pkt.gl_renderer[127] = '\0';
    } else {
        strncpy(pkt.gl_renderer, "Unknown", 127);
        pkt.gl_renderer[127] = '\0';
    }
    
    // Get OS info
    get_os_info(pkt.os_info, sizeof(pkt.os_info));
    
    send_packet(&pkt);
}
