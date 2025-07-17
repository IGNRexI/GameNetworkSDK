#include "../sdk/gamenetwork.h"

gnet_relay_server_t* relay = NULL;

void on_peer_connect(gnet_relay_server_t* server, gnet_relay_peer_info_t* peer) {
    gnet_log("[RELAY] Peer %d bağlandı.", peer->id);
}

void on_peer_receive(gnet_relay_server_t* server, gnet_relay_peer_info_t* from, const char* data, int len) {
    // Paket: [hedef_id][payload]
    if (len < 4) return;
    int target_id = 0;
    memcpy(&target_id, data, 4);
    gnet_log("[RELAY] Peer %d -> Peer %d (%d byte)", from->id, target_id, len-4);
    gnet_relay_server_forward(server, target_id, data+4, len-4);
}

void on_peer_disconnect(gnet_relay_server_t* server, gnet_relay_peer_info_t* peer) {
    gnet_log("[RELAY] Peer %d ayrıldı.", peer->id);
}

int main() {
    relay = gnet_relay_server_start(12345, on_peer_connect, on_peer_receive, on_peer_disconnect, NULL);
    if (!relay) {
        gnet_log("Relay sunucu başlatılamadı!");
        return 1;
    }
    gnet_log("Relay sunucu başlatıldı. Port: 12345");
    getchar();
    gnet_relay_server_stop(relay);
    return 0;
} 