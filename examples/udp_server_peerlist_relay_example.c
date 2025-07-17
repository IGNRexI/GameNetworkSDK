#include "../sdk/gamenetwork.h"

#define MAX_CLIENTS 64

typedef struct {
    int id;
    struct sockaddr_in addr;
    int active;
} udp_client_info_t;

udp_client_info_t clients[MAX_CLIENTS];
int client_count = 0;

// Basit id bulucu
int find_client_id(struct sockaddr_in* addr) {
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i].active && memcmp(&clients[i].addr, addr, sizeof(*addr)) == 0)
            return clients[i].id;
    }
    return -1;
}

void on_udp_receive(struct sockaddr_in* from, const char* data, int len, void* user_data) {
    int id = find_client_id(from);
    if (id < 0) {
        // Yeni client, id ata
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (!clients[i].active) {
                clients[i].id = i+1;
                clients[i].addr = *from;
                clients[i].active = 1;
                client_count++;
                id = clients[i].id;
                break;
            }
        }
    }
    // Peer listesi isteği
    if (len >= 8 && memcmp(data, "PEERLIST", 8) == 0) {
        char msg[256];
        int offset = 0;
        offset += sprintf(msg+offset, "PEERLIST:");
        for (int i = 0; i < MAX_CLIENTS; ++i)
            if (clients[i].active) offset += sprintf(msg+offset, "%d,", clients[i].id);
        gnet_udp_server_sendto((gnet_udp_server_t*)user_data, from, msg, (int)strlen(msg));
        return;
    }
    // Relay: [hedef_id][payload]
    if (len >= 4) {
        int to_id = 0;
        memcpy(&to_id, data, 4);
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i].active && clients[i].id == to_id) {
                gnet_udp_server_sendto((gnet_udp_server_t*)user_data, &clients[i].addr, data+4, len-4);
                break;
            }
        }
    }
}

int main() {
    memset(clients, 0, sizeof(clients));
    gnet_udp_server_t* server = gnet_udp_server_start(12345, on_udp_receive, NULL);
    if (!server) {
        gnet_log("UDP sunucu başlatılamadı!");
        return 1;
    }
    server->user_data = server;
    gnet_log("UDP sunucu başlatıldı. Port: 12345");
    gnet_udp_server_start_async(server);
    getchar();
    gnet_udp_server_stop_async(server);
    return 0;
} 