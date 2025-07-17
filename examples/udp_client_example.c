#include "../sdk/gamenetwork.h"

void on_udp_receive(const char* data, int len) {
    printf("[UDP CLIENT] Sunucudan cevap: %.*s\n", len, data);
}

int main() {
    gnet_udp_client_t* client = gnet_udp_client_start("127.0.0.1", 12345, NULL);
    if (!client) {
        printf("UDP istemci başlatılamadı!\n");
        return 1;
    }
    printf("UDP istemci başlatıldı.\n");
    gnet_udp_client_send(client, "Merhaba UDP Sunucu!", 20);
    gnet_udp_client_start_async(client, on_udp_receive);
    getchar();
    gnet_udp_client_stop_async(client);
    return 0;
} 