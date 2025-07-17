#include "../sdk/gamenetwork.h"

// Komut: udp_client_peerlist_relay_example <server_ip> <server_port> <my_id> <target_id>

int main(int argc, char** argv) {
    if (argc < 5) {
        printf("Kullanım: %s <server_ip> <server_port> <my_id> <target_id>\n", argv[0]);
        return 1;
    }
    const char* server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int my_id = atoi(argv[3]);
    int target_id = atoi(argv[4]);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    // Peer listesi isteği
    sendto(sock, "PEERLIST", 8, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    char buf[512];
    struct sockaddr_in from; int fromlen = sizeof(from);
    int len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromlen);
    if (len > 0) {
        printf("Peer listesi: %.*s\n", len, buf);
    }

    // Mesaj gönder (paket: [hedef_id][payload])
    char msg[256];
    snprintf(msg, sizeof(msg), "Merhaba UDP Peer %d!", target_id);
    char packet[260];
    memcpy(packet, &target_id, 4);
    strcpy(packet+4, msg);
    sendto(sock, packet, 4+(int)strlen(msg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    printf("Peer %d'ye UDP ile mesaj gönderildi: %s\n", target_id, msg);

    // Cevap bekle
    len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromlen);
    if (len > 0) {
        printf("Peer'dan UDP cevap: %.*s\n", len, buf);
    }
    closesocket(sock);
    return 0;
} 