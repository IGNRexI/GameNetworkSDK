#include "../sdk/gamenetwork.h"

gns_server_t* server = NULL;

// Yeni: gnet_socket_t* ile veri alım callback'i
void on_receive_ex(gnet_socket_t* client, const char* data, int len) {
    char ip[32];
    inet_ntop(AF_INET, &(client->addr.sin_addr), ip, sizeof(ip));
    printf("[SERVER] %s:%d -> %.*s\n", ip, ntohs(client->addr.sin_port), len, data);
    // İstemciye özel cevap
    char reply[64];
    snprintf(reply, sizeof(reply), "[SERVER] Merhaba %s:%d!", ip, ntohs(client->addr.sin_port));
    gns_server_send2(server, client, reply, (int)strlen(reply));
}

void on_connect(int client_id, const struct sockaddr_in* addr) {
    printf("[SERVER] Yeni bağlantı: %d\n", client_id);
}

void on_disconnect(int client_id) {
    printf("[SERVER] Bağlantı koptu: %d\n", client_id);
}

int main() {
    server = gns_start_server(12345, NULL, on_connect, on_disconnect);
    if (!server) {
        printf("Sunucu başlatılamadı!\n");
        return 1;
    }
    // Otomatik buffer ayarı
    gns_server_set_autobufferassign(server, 1);
    // Manuel buffer ayarı örneği (otomatik kapalıysa)
    // gns_server_set_buffer_size(server, 8192);
    printf("Sunucu başlatıldı. Port: 12345\n");
    gns_server_start_async(server);
    // Manuel döngü ile veri alımını gnet_socket_t* ile yap
    while (1) {
        for (int i = 0; i < GNS_MAX_CLIENTS; ++i) {
            gnet_socket_t* client = gns_server_get_client(server, i);
            if (client) {
                char buf[1024];
                int len = recv(client->socket, buf, sizeof(buf), MSG_DONTWAIT);
                if (len > 0) {
                    on_receive_ex(client, buf, len);
                }
            }
        }
        // ... mevcut komut döngüsü ...
        printf("\nKomutlar: [c]lient sayısı, [b]roadcast, [k]ick <id>, [m]esaj <id>, [q]uit\n");
        char cmd[64];
        fgets(cmd, sizeof(cmd), stdin);
        if (cmd[0] == 'c') {
            printf("Bağlı istemci sayısı: %d\n", gns_server_client_count(server));
        } else if (cmd[0] == 'b') {
            gns_server_broadcast(server, "[SERVER] Merhaba tüm istemcilere!", 32);
            printf("Broadcast gönderildi.\n");
        } else if (cmd[0] == 'k') {
            int id = atoi(&cmd[2]);
            gnet_socket_t* client = gns_server_get_client(server, id);
            gns_server_kick2(server, client);
            printf("%d numaralı istemci atıldı.\n", id);
        } else if (cmd[0] == 'm') {
            int id = atoi(&cmd[2]);
            gnet_socket_t* client = gns_server_get_client(server, id);
            if (client) {
                gns_server_send2(server, client, "[SERVER] Size özel mesaj!", 25);
                printf("%d numaralı istemciye özel mesaj gönderildi.\n", id);
            }
        } else if (cmd[0] == 'q') {
            break;
        }
    }
    gns_server_stop_async(server);
    return 0;
} 