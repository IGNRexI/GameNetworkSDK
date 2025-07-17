#include "../sdk/gamenetwork.h"

void on_udp_receive(struct sockaddr_in* from, const char* data, int len, void* user_data) {
    char ip[32];
    inet_ntop(AF_INET, &(from->sin_addr), ip, sizeof(ip));
    printf("[UDP SERVER] %s:%d -> %.*s\n", ip, ntohs(from->sin_port), len, data);
    // Cevap gönder
    gnet_udp_server_t* server = (gnet_udp_server_t*)user_data;
    char reply[64];
    snprintf(reply, sizeof(reply), "[UDP SERVER] Merhaba %s:%d!", ip, ntohs(from->sin_port));
    gnet_udp_server_sendto(server, from, reply, (int)strlen(reply));
}

int main() {
    gnet_udp_server_t* server = gnet_udp_server_start(12345, on_udp_receive, NULL);
    if (!server) {
        printf("UDP sunucu başlatılamadı!\n");
        return 1;
    }
    // Otomatik buffer ayarı
    gnet_udp_server_set_autobufferassign(server, 1);
    // Manuel buffer ayarı örneği (otomatik kapalıysa)
    // gnet_udp_server_set_buffer_size(server, 8192);
    server->user_data = server;
    printf("UDP sunucu başlatıldı. Port: 12345\n");
    gnet_udp_server_start_async(server);
    getchar();
    gnet_udp_server_stop_async(server);
    return 0;
} 