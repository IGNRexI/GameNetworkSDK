#include "../sdk/gamenetwork.h"

// gnet_socket_t* ile sunucuya özel mesaj gönderme fonksiyonu
gnet_socket_t my_socket; // Sadece örnek için, gerçek istemci için kullanılmaz

void send_custom_message(gns_client_t* client, const char* msg) {
    gns_client_send(client, msg, (int)strlen(msg));
}

void on_receive(const char* data, int len) {
    printf("[CLIENT] Veri alındı: %.*s\n", len, data);
    // Sunucudan gelen özel mesajı ayırt et
    if (len > 10 && memcmp(data, "[SERVER]", 8) == 0) {
        printf("[CLIENT] Sunucudan özel mesaj geldi!\n");
    }
}

int main() {
    gns_client_t* client = gns_start_client("127.0.0.1", 12345, on_receive);
    if (!client) {
        printf("Bağlanılamadı!\n");
        return 1;
    }
    printf("Sunucuya bağlanıldı.\n");
    send_custom_message(client, "Merhaba Sunucu! Ben özel bir mesajım.");
    gns_client_start_async(client);
    getchar();
    gns_client_stop_async(client);
    return 0;
} 