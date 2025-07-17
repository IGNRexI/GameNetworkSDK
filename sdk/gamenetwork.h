#ifndef GAMENETWORK_H
#define GAMENETWORK_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _WIN32
// Windows için
#else
#include <arpa/inet.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif

#include "gamenetwork_types.h"

// Sunucu ve istemci tanımları için ileri bildirimler
typedef struct gns_server_s gns_server_t;
typedef struct gns_client_s gns_client_t;

// Callback tipi
typedef void (*gns_on_receive_cb)(const char* data, int len);

// Bağlantı event callback tipleri
typedef void (*gns_on_connect_cb)(int client_id, const struct sockaddr_in* addr);
typedef void (*gns_on_disconnect_cb)(int client_id);

// Gelişmiş callback tipleri
typedef void (*gns_on_connect_ex_cb)(gnet_socket_t* client);
typedef void (*gns_on_receive_ex_cb)(gnet_socket_t* client, const char* data, int len);
typedef void (*gns_on_disconnect_ex_cb)(gnet_socket_t* client);

// Hata kodları
#define GNET_OK 0
#define GNET_ERR_SOCKET -1
#define GNET_ERR_BIND -2
#define GNET_ERR_CONNECT -3
#define GNET_ERR_SEND -4
#define GNET_ERR_RECV -5
#define GNET_ERR_MEMORY -6
#define GNET_ERR_PARAM -7
#define GNET_ERR_UNKNOWN -99

// Hata kodunu açıklamaya çevirir
typedef int gnet_err_t;
const char* gnet_strerror(gnet_err_t code);

// Log callback tipi ve ayarlama fonksiyonu
typedef void (*gnet_log_cb)(const char* msg);
void gnet_set_log_callback(gnet_log_cb cb);
void gnet_log(const char* fmt, ...);

// Sunucu başlatma fonksiyonu (gelişmiş)
gns_server_t* gns_start_server(uint16_t port, gns_on_receive_cb on_receive, gns_on_connect_cb on_connect, gns_on_disconnect_cb on_disconnect);
// Gelişmiş sunucu başlatma fonksiyonu
gns_server_t* gns_start_server_ex(uint16_t port, gns_on_receive_ex_cb on_receive, gns_on_connect_ex_cb on_connect, gns_on_disconnect_ex_cb on_disconnect);

// Belirli bir istemciye mesaj gönderme
int gns_server_send(gns_server_t* server, int client_id, const char* data, int len);

gns_client_t* gns_start_client(const char* host, uint16_t port, gns_on_receive_cb on_receive);
void gns_run(void* instance); // Sunucu/istemci ana döngüsü
void gns_stop(void* instance);

int gns_client_send(gns_client_t* client, const char* data, int len);
void gns_client_run(gns_client_t* client);
void gns_client_stop(gns_client_t* client);

// Sunucu/istemciyi thread ile başlatma
int gns_server_start_async(gns_server_t* server);
int gns_client_start_async(gns_client_t* client);
// Threadli durdurma
void gns_server_stop_async(gns_server_t* server);
void gns_client_stop_async(gns_client_t* client);

// Bağlı istemci sayısını döndürür
int gns_server_client_count(gns_server_t* server);
// Belirli istemciyi atar (disconnect)
void gns_server_kick(gns_server_t* server, int client_id);
// Tüm istemcilere mesaj yayınlar (broadcast)
// Bağlı istemcinin handle'ını döndürür
// id ile veya index ile erişim için
// NOT: Geriye NULL dönebilir
// gnet_socket_t* gns_server_get_client(gns_server_t* server, int client_id);
gnet_socket_t* gns_server_get_client(gns_server_t* server, int client_id);

// Yeni API: client_id yerine gnet_socket_t* ile işlem
int gns_server_send2(gns_server_t* server, gnet_socket_t* client, const char* data, int len);
void gns_server_kick2(gns_server_t* server, gnet_socket_t* client);

// UDP sunucu başlatma
// on_receive: veri geldiğinde çağrılır
// user_data: callback'e iletilir
gnet_udp_server_t* gnet_udp_server_start(uint16_t port, void (*on_receive)(struct sockaddr_in* from, const char* data, int len, void* user_data), void* user_data);
void gnet_udp_server_stop(gnet_udp_server_t* server);
int gnet_udp_server_sendto(gnet_udp_server_t* server, struct sockaddr_in* to, const char* data, int len);

// UDP istemci başlatma
gnet_udp_client_t* gnet_udp_client_start(const char* host, uint16_t port, void* user_data);
void gnet_udp_client_stop(gnet_udp_client_t* client);
int gnet_udp_client_send(gnet_udp_client_t* client, const char* data, int len);

// UDP sunucu/istemci için threadli başlatma ve durdurma
int gnet_udp_server_start_async(gnet_udp_server_t* server);
void gnet_udp_server_stop_async(gnet_udp_server_t* server);
int gnet_udp_client_start_async(gnet_udp_client_t* client, void (*on_receive)(const char* data, int len));
void gnet_udp_client_stop_async(gnet_udp_client_t* client);

// Buffer ayarı fonksiyonları
void gns_server_set_buffer_size(gns_server_t* server, int size);
void gns_server_set_autobufferassign(gns_server_t* server, int enable);
void gns_client_set_buffer_size(gns_client_t* client, int size);
void gns_client_set_autobufferassign(gns_client_t* client, int enable);
void gnet_udp_server_set_buffer_size(gnet_udp_server_t* server, int size);
void gnet_udp_server_set_autobufferassign(gnet_udp_server_t* server, int enable);
void gnet_udp_client_set_buffer_size(gnet_udp_client_t* client, int size);
void gnet_udp_client_set_autobufferassign(gnet_udp_client_t* client, int enable);

// P2P peer başlatma/durdurma
// on_peer_connect/receive/disconnect: event callback'leri
gnet_p2p_peer_t* gnet_p2p_peer_start(uint16_t port,
    void (*on_peer_connect)(gnet_p2p_peer_t*, gnet_p2p_peer_info_t*),
    void (*on_peer_receive)(gnet_p2p_peer_t*, gnet_p2p_peer_info_t*, const char*, int),
    void (*on_peer_disconnect)(gnet_p2p_peer_t*, gnet_p2p_peer_info_t*),
    void* user_data);
void gnet_p2p_peer_stop(gnet_p2p_peer_t* peer);

// Peer'a bağlanma
int gnet_p2p_peer_connect(gnet_p2p_peer_t* self, const char* host, uint16_t port);
// Peer'a veri gönderme
int gnet_p2p_peer_send(gnet_p2p_peer_t* self, int peer_id, const char* data, int len);
// Tüm peer'lara broadcast
typedef int gnet_p2p_peer_broadcast(gnet_p2p_peer_t* self, const char* data, int len);

// Relay sunucu başlatma/durdurma
// on_peer_connect/receive/disconnect: event callback'leri
gnet_relay_server_t* gnet_relay_server_start(uint16_t port,
    void (*on_peer_connect)(gnet_relay_server_t*, gnet_relay_peer_info_t*),
    void (*on_peer_receive)(gnet_relay_server_t*, gnet_relay_peer_info_t*, const char*, int),
    void (*on_peer_disconnect)(gnet_relay_server_t*, gnet_relay_peer_info_t*),
    void* user_data);
void gnet_relay_server_stop(gnet_relay_server_t* server);

// P2P peer başlatma (relay ile)
gnet_p2p_peer_t* gnet_p2p_peer_start_with_relay(uint16_t port,
    const char* relay_host, uint16_t relay_port,
    void (*on_peer_connect)(gnet_p2p_peer_t*, gnet_p2p_peer_info_t*),
    void (*on_peer_receive)(gnet_p2p_peer_t*, gnet_p2p_peer_info_t*, const char*, int),
    void (*on_peer_disconnect)(gnet_p2p_peer_t*, gnet_p2p_peer_info_t*),
    void* user_data);

// Relay sunucuda peer id ile veri yönlendirme ve broadcast
int gnet_relay_server_forward(gnet_relay_server_t* server, int target_peer_id, const char* data, int len);
int gnet_relay_server_broadcast(gnet_relay_server_t* server, int except_peer_id, const char* data, int len);
gnet_relay_peer_info_t* gnet_relay_server_get_peer_by_id(gnet_relay_server_t* server, int peer_id);

// UDP relay forwarding
int gnet_relay_server_forward_udp(gnet_relay_server_t* server, int target_peer_id, const char* data, int len);

// Ping/pong ve bağlantı sağlığı
void gnet_enable_pingpong(void* instance, int enable); // sunucu/istemci/peer
int gnet_get_last_latency(void* instance, int peer_id); // ms cinsinden
// Ping event callback
// void (*on_ping)(void* instance, int peer_id, int latency_ms);

// Sunucu başlatma/durdurma
// on_client_connect/message/disconnect: event callback'leri
gnet_server_t* gnet_server_start(uint16_t port,
    void (*on_client_connect)(gnet_server_t*, gnet_server_client_info_t*),
    void (*on_client_message)(gnet_server_t*, gnet_server_client_info_t*, const char*, int),
    void (*on_client_disconnect)(gnet_server_t*, gnet_server_client_info_t*),
    void* user_data);
void gnet_server_stop(gnet_server_t* server);

// Client'a veri gönderme
int gnet_server_send(gnet_server_t* server, int client_id, const char* data, int len);
// Peer listesi alma
gnet_server_client_info_t* gnet_server_get_client_by_id(gnet_server_t* server, int client_id);
int gnet_server_get_peer_list(gnet_server_t* server, int* out_ids, int max_count);
// Relay: bir client'tan diğerine veri iletme
int gnet_server_relay(gnet_server_t* server, int from_id, int to_id, const char* data, int len);

// Kimlik doğrulama (token/handshake)
void gnet_server_set_token(gnet_server_t* server, const char* token);
void gnet_client_set_token(void* client, const char* token); // client/peer/udp_client

// Otomatik reconnect ve bağlantı sağlığı
void gnet_client_set_reconnect(void* client, int enable, int max_attempts, int interval_ms);
void gnet_set_reconnect_callback(void* client, void (*cb)(void* client, int status));

// Mini JSON encode/decode (sıfır bağımlılık)
int gnet_json_set_str(char* json, int maxlen, const char* key, const char* value);
int gnet_json_set_int(char* json, int maxlen, const char* key, int value);
int gnet_json_get_str(const char* json, const char* key, char* out, int maxlen);
int gnet_json_get_int(const char* json, const char* key, int* out);

// Event queue (thread-safe)
typedef struct gnet_event_queue_s gnet_event_queue_t;
typedef struct {
    int type; // 0: message, 1: disconnect, 2: reconnect, 3: timeout, ...
    int client_id;
    char data[256];
    int data_len;
} gnet_event_t;
gnet_event_queue_t* gnet_event_queue_create(int max_events);
void gnet_event_queue_destroy(gnet_event_queue_t* q);
int gnet_event_queue_push(gnet_event_queue_t* q, const gnet_event_t* ev);
int gnet_event_queue_pop(gnet_event_queue_t* q, gnet_event_t* out);
int gnet_event_queue_size(gnet_event_queue_t* q);

// Server event callback (bağlantı sağlığı)
typedef void (*gnet_on_client_event_cb)(gnet_server_t*, int client_id, int event_type);

#ifdef __cplusplus
}
#endif

#endif // GAMENETWORK_H 