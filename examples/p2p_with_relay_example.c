#include "../sdk/gamenetwork.h"

// Peer uygulaması: relay üzerinden başka bir peer'a mesaj gönderir ve alır
// Komut satırında: p2p_with_relay_example <relay_ip> <relay_port> <my_id> <target_id>

int main(int argc, char** argv) {
    if (argc < 5) {
        printf("Kullanım: %s <relay_ip> <relay_port> <my_id> <target_id>\n", argv[0]);
        return 1;
    }
    const char* relay_ip = argv[1];
    int relay_port = atoi(argv[2]);
    int my_id = atoi(argv[3]);
    int target_id = atoi(argv[4]);

    // Relay sunucuya TCP ile bağlan
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in relay_addr = {0};
    relay_addr.sin_family = AF_INET;
    relay_addr.sin_port = htons(relay_port);
    inet_pton(AF_INET, relay_ip, &relay_addr.sin_addr);
    if (connect(sock, (struct sockaddr*)&relay_addr, sizeof(relay_addr)) < 0) {
        printf("Relay sunucuya bağlanılamadı!\n");
        return 1;
    }
    printf("Relay sunucuya bağlanıldı. Peer id: %d\n", my_id);

    // Mesaj gönder (paket: [hedef_id][payload])
    char msg[256];
    snprintf(msg, sizeof(msg), "Merhaba Peer %d!", target_id);
    char packet[260];
    memcpy(packet, &target_id, 4);
    strcpy(packet+4, msg);
    send(sock, packet, 4+(int)strlen(msg), 0);
    printf("Peer %d'ye mesaj gönderildi: %s\n", target_id, msg);

    // Cevap bekle
    char buf[512];
    int len = recv(sock, buf, sizeof(buf), 0);
    if (len > 0) {
        printf("Peer'dan cevap: %.*s\n", len, buf);
    }
    closesocket(sock);
    return 0;
} 