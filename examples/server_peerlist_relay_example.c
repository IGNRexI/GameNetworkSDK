#include "../sdk/gamenetwork.h"

gnet_server_t* server = NULL;

void on_client_connect(gnet_server_t* server, gnet_server_client_info_t* client) {
    gnet_log("[SERVER] Client %d bağlandı.", client->id);
    // Peer listesi hazırla ve yeni client'a gönder
    int ids[GNET_SERVER_MAX_CLIENTS];
    int count = gnet_server_get_peer_list(server, ids, GNET_SERVER_MAX_CLIENTS);
    char msg[256];
    int offset = 0;
    offset += sprintf(msg+offset, "PEERLIST:");
    for (int i = 0; i < count; ++i) offset += sprintf(msg+offset, "%d,", ids[i]);
    gnet_server_send(server, client->id, msg, (int)strlen(msg));
}

void on_client_message(gnet_server_t* server, gnet_server_client_info_t* from, const char* data, int len) {
    // Paket: [hedef_id][payload]
    if (len < 4) return;
    int to_id = 0;
    memcpy(&to_id, data, 4);
    gnet_log("[SERVER] Client %d -> Client %d (%d byte)", from->id, to_id, len-4);
    gnet_server_relay(server, from->id, to_id, data+4, len-4);
}

void on_client_disconnect(gnet_server_t* server, gnet_server_client_info_t* client) {
    gnet_log("[SERVER] Client %d ayrıldı.", client->id);
}

int main() {
    server = gnet_server_start(12345, on_client_connect, on_client_message, on_client_disconnect, NULL);
    if (!server) {
        gnet_log("Sunucu başlatılamadı!");
        return 1;
    }
    gnet_log("Sunucu başlatıldı. Port: 12345");
    getchar();
    gnet_server_stop(server);
    return 0;
} 