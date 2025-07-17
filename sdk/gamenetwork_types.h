#ifndef GAMENETWORK_TYPES_H
#define GAMENETWORK_TYPES_H

#include <stdint.h>
#include <winsock2.h>
#include <windows.h>

// Bağlı istemci bilgisi
typedef struct {
    SOCKET socket;
    struct sockaddr_in addr;
    uint32_t last_active;
    int active;
} gns_client_info_t;

// Sunucu yapısı
struct gns_server_s {
    SOCKET listen_socket;
    gns_client_info_t clients[GNS_MAX_CLIENTS];
    gns_on_receive_cb on_receive;
    void (*on_connect)(int client_id, const struct sockaddr_in* addr);
    void (*on_disconnect)(int client_id);
    HANDLE thread_handle;
    int running;
    // Gelişmiş callback'ler
    gns_on_receive_ex_cb on_receive_ex;
    gns_on_connect_ex_cb on_connect_ex;
    gns_on_disconnect_ex_cb on_disconnect_ex;
    int buffer_size;
    int autobufferassign;
};

// İstemci yapısı (basit)
struct gns_client_s {
    SOCKET socket;
    gns_on_receive_cb on_receive;
    HANDLE thread_handle;
    int running;
    int buffer_size;
    int autobufferassign;
};

// Bağlantı türleri
typedef enum {
    GNS_PROTOCOL_TCP,
    GNS_PROTOCOL_UDP
} gns_protocol_t;

typedef struct gnet_socket_s {
    int id; // Dahili client id
    SOCKET socket; // Gerçek winsock socket
    struct sockaddr_in addr;
    void* user_data; // Kullanıcıya özel veri
} gnet_socket_t;

typedef struct gnet_udp_server_s {
    SOCKET socket;
    struct sockaddr_in addr;
    int running;
    HANDLE thread_handle;
    void (*on_receive)(struct sockaddr_in* from, const char* data, int len, void* user_data);
    void* user_data;
    int buffer_size;
    int autobufferassign;
} gnet_udp_server_t;

typedef struct gnet_udp_client_s {
    SOCKET socket;
    struct sockaddr_in server_addr;
    int running;
    HANDLE thread_handle;
    void* user_data;
    int buffer_size;
    int autobufferassign;
} gnet_udp_client_t;

typedef struct gnet_p2p_peer_info_s {
    int id;
    struct sockaddr_in addr;
    int connected;
    SOCKET socket;
} gnet_p2p_peer_info_t;

#define GNET_P2P_MAX_PEERS 16

typedef struct gnet_p2p_peer_s {
    SOCKET listen_socket;
    gnet_p2p_peer_info_t peers[GNET_P2P_MAX_PEERS];
    int peer_count;
    int running;
    HANDLE thread_handle;
    void (*on_peer_connect)(struct gnet_p2p_peer_s*, gnet_p2p_peer_info_t* peer);
    void (*on_peer_receive)(struct gnet_p2p_peer_s*, gnet_p2p_peer_info_t* peer, const char* data, int len);
    void (*on_peer_disconnect)(struct gnet_p2p_peer_s*, gnet_p2p_peer_info_t* peer);
    void* user_data;
} gnet_p2p_peer_t;

typedef struct gnet_relay_peer_info_s {
    int id;
    struct sockaddr_in addr;
    int connected;
    SOCKET socket;
} gnet_relay_peer_info_t;

#define GNET_RELAY_MAX_PEERS 32

typedef struct gnet_relay_server_s {
    SOCKET tcp_socket;
    SOCKET udp_socket;
    struct sockaddr_in addr;
    gnet_relay_peer_info_t peers[GNET_RELAY_MAX_PEERS];
    int peer_count;
    int running;
    HANDLE thread_handle;
    void (*on_peer_connect)(struct gnet_relay_server_s*, gnet_relay_peer_info_t* peer);
    void (*on_peer_receive)(struct gnet_relay_server_s*, gnet_relay_peer_info_t* from, const char* data, int len);
    void (*on_peer_disconnect)(struct gnet_relay_server_s*, gnet_relay_peer_info_t* peer);
    void* user_data;
} gnet_relay_server_t;

typedef struct gnet_server_client_info_s {
    int id;
    struct sockaddr_in addr;
    int connected;
    SOCKET socket;
} gnet_server_client_info_t;

#define GNET_SERVER_MAX_CLIENTS 64

typedef struct gnet_server_s {
    SOCKET listen_socket;
    gnet_server_client_info_t clients[GNET_SERVER_MAX_CLIENTS];
    int client_count;
    int running;
    HANDLE thread_handle;
    void (*on_client_connect)(struct gnet_server_s*, gnet_server_client_info_t* client);
    void (*on_client_message)(struct gnet_server_s*, gnet_server_client_info_t* from, const char* data, int len);
    void (*on_client_disconnect)(struct gnet_server_s*, gnet_server_client_info_t* client);
    void* user_data;
} gnet_server_t;

#endif // GAMENETWORK_TYPES_H 