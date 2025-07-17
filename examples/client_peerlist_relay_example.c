#include "../sdk/gamenetwork.h"

// Komut: client_peerlist_relay_example <server_ip> <server_port> <my_id> <target_id>

void on_reconnect(void* client, int status) {
    if (status == 0) printf("[CLIENT] Yeniden bağlanılıyor...\n");
    else if (status == 1) printf("[CLIENT] Yeniden bağlantı BAŞARILI!\n");
    else if (status == -1) printf("[CLIENT] Yeniden bağlantı BAŞARISIZ!\n");
}

int main(int argc, char** argv) {
    if (argc < 5) {
        printf("Kullanım: %s <server_ip> <server_port> <my_id> <target_id>\n", argv[0]);
        return 1;
    }
    const char* server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int my_id = atoi(argv[3]);
    int target_id = atoi(argv[4]);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Sunucuya bağlanılamadı!\n");
        return 1;
    }
    printf("Sunucuya bağlanıldı. Peer id: %d\n", my_id);

    // Otomatik reconnect ayarla
    gnet_client_set_reconnect(&sock, 1, 5, 2000); // 5 deneme, 2 sn aralık
    gnet_set_reconnect_callback(&sock, on_reconnect);

    // Peer listesi al
    char buf[512];
    int len = recv(sock, buf, sizeof(buf), 0);
    if (len > 0) {
        printf("Peer listesi: %.*s\n", len, buf);
    }

    // Mesaj gönder (paket: [hedef_id][payload])
    char msg[256];
    snprintf(msg, sizeof(msg), "Merhaba Peer %d!", target_id);
    char packet[260];
    memcpy(packet, &target_id, 4);
    strcpy(packet+4, msg);
    send(sock, packet, 4+(int)strlen(msg), 0);
    printf("Peer %d'ye mesaj gönderildi: %s\n", target_id, msg);

    // Cevap bekle
    len = recv(sock, buf, sizeof(buf), 0);
    if (len > 0) {
        printf("Peer'dan cevap: %.*s\n", len, buf);
    }

    // Simüle bağlantı kopması (test için)
    printf("Bağlantı kopartılıyor (test için)...\n");
    closesocket(sock);
    // Otomatik reconnect denemesi
    gnet_client_reconnect(&sock, &server_addr);

    closesocket(sock);
    return 0;
} 