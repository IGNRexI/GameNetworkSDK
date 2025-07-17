#include "gamenetwork.h"
#include "gamenetwork_types.h"
#include "gamenetwork_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>
#include <windows.h>

struct gnet_event_queue_s {
    gnet_event_t* events;
    int max_events;
    int head, tail, count;
    CRITICAL_SECTION lock;
};

gnet_event_queue_t* gnet_event_queue_create(int max_events) {
    gnet_event_queue_t* q = (gnet_event_queue_t*)calloc(1, sizeof(gnet_event_queue_t));
    q->events = (gnet_event_t*)calloc(max_events, sizeof(gnet_event_t));
    q->max_events = max_events;
    q->head = q->tail = q->count = 0;
    InitializeCriticalSection(&q->lock);
    return q;
}
void gnet_event_queue_destroy(gnet_event_queue_t* q) {
    if (!q) return;
    DeleteCriticalSection(&q->lock);
    free(q->events);
    free(q);
}
int gnet_event_queue_push(gnet_event_queue_t* q, const gnet_event_t* ev) {
    int ret = -1;
    EnterCriticalSection(&q->lock);
    if (q->count < q->max_events) {
        q->events[q->tail] = *ev;
        q->tail = (q->tail + 1) % q->max_events;
        q->count++;
        ret = 0;
    }
    LeaveCriticalSection(&q->lock);
    return ret;
}
int gnet_event_queue_pop(gnet_event_queue_t* q, gnet_event_t* out) {
    int ret = -1;
    EnterCriticalSection(&q->lock);
    if (q->count > 0) {
        *out = q->events[q->head];
        q->head = (q->head + 1) % q->max_events;
        q->count--;
        ret = 0;
    }
    LeaveCriticalSection(&q->lock);
    return ret;
}
int gnet_event_queue_size(gnet_event_queue_t* q) {
    int n;
    EnterCriticalSection(&q->lock);
    n = q->count;
    LeaveCriticalSection(&q->lock);
    return n;
}

#define GNET_PING_MAGIC 0x50494E47 // 'PING'
#define GNET_PONG_MAGIC 0x504F4E47 // 'PONG'
#define GNET_PING_INTERVAL 5 // saniye

// Her peer için latency ve ping zamanı
static int gnet_last_latency[GNET_P2P_MAX_PEERS] = {0};
static time_t gnet_last_ping_sent[GNET_P2P_MAX_PEERS] = {0};
static int gnet_pingpong_enabled = 0;

void gnet_enable_pingpong(void* instance, int enable) {
    gnet_pingpong_enabled = enable;
}

int gnet_get_last_latency(void* instance, int peer_id) {
    if (peer_id < 0 || peer_id >= GNET_P2P_MAX_PEERS) return -1;
    return gnet_last_latency[peer_id];
}

// Ping/pong mesajı gönderme ve latency ölçümü (örnek TCP sunucu/peer için)
// Bu kod, ana döngüde veya threadde çağrılmalı
void gnet_pingpong_tick(gnet_p2p_peer_t* peer) {
    if (!gnet_pingpong_enabled) return;
    time_t now = time(NULL);
    for (int i = 0; i < GNET_P2P_MAX_PEERS; ++i) {
        if (peer->peers[i].connected) {
            if (now - gnet_last_ping_sent[i] >= GNET_PING_INTERVAL) {
                uint32_t magic = GNET_PING_MAGIC;
                uint64_t t = (uint64_t)now;
                char pkt[16];
                memcpy(pkt, &magic, 4);
                memcpy(pkt+4, &t, 8);
                gnet_last_ping_sent[i] = now;
                send(peer->peers[i].socket, pkt, 12, 0);
            }
        }
    }
}

// Veri alımında ping/pong kontrolü (örnek)
int gnet_pingpong_handle_packet(gnet_p2p_peer_t* peer, int peer_idx, const char* data, int len) {
    if (len >= 12) {
        uint32_t magic = 0;
        memcpy(&magic, data, 4);
        if (magic == GNET_PING_MAGIC) {
            // Pong ile cevapla
            uint32_t pong = GNET_PONG_MAGIC;
            send(peer->peers[peer_idx].socket, (char*)&pong, 4, 0);
            return 1;
        } else if (magic == GNET_PONG_MAGIC) {
            // Latency ölç
            time_t now = time(NULL);
            int latency = (int)(now - gnet_last_ping_sent[peer_idx]) * 1000;
            gnet_last_latency[peer_idx] = latency;
            return 1;
        }
    }
    return 0;
}

// Otomatik reconnect ve bağlantı sağlığı
static int gnet_reconnect_enabled = 0;
static int gnet_reconnect_max_attempts = 3;
static int gnet_reconnect_interval_ms = 2000;
static void (*gnet_reconnect_callback)(void* client, int status) = NULL;

void gnet_client_set_reconnect(void* client, int enable, int max_attempts, int interval_ms) {
    gnet_reconnect_enabled = enable;
    if (max_attempts > 0) gnet_reconnect_max_attempts = max_attempts;
    if (interval_ms > 0) gnet_reconnect_interval_ms = interval_ms;
}
void gnet_set_reconnect_callback(void* client, void (*cb)(void* client, int status)) {
    gnet_reconnect_callback = cb;
}

// Bağlantı kopunca otomatik reconnect (örnek TCP için, thread içinde çağrılmalı)
int gnet_client_reconnect(SOCKET* sock, struct sockaddr_in* addr) {
    if (!gnet_reconnect_enabled) return -1;
    int attempts = 0;
    while (attempts < gnet_reconnect_max_attempts) {
        if (gnet_reconnect_callback) gnet_reconnect_callback(sock, 0); // reconnecting
        closesocket(*sock);
        *sock = socket(AF_INET, SOCK_STREAM, 0);
        if (*sock != INVALID_SOCKET && connect(*sock, (struct sockaddr*)addr, sizeof(*addr)) == 0) {
            if (gnet_reconnect_callback) gnet_reconnect_callback(sock, 1); // success
            return 0;
        }
        attempts++;
        Sleep(gnet_reconnect_interval_ms);
    }
    if (gnet_reconnect_callback) gnet_reconnect_callback(sock, -1); // failed
    return -1;
}

static int gns_winsock_initialized = 0;

static void gns_init_winsock() {
    if (!gns_winsock_initialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
            fprintf(stderr, "WSAStartup failed\n");
            exit(1);
        }
        gns_winsock_initialized = 1;
    }
}

static void gns_cleanup_winsock() {
    if (gns_winsock_initialized) {
        WSACleanup();
        gns_winsock_initialized = 0;
    }
}

gns_server_t* gns_start_server(uint16_t port, gns_on_receive_cb on_receive, gns_on_connect_cb on_connect, gns_on_disconnect_cb on_disconnect) {
    gns_init_winsock();
    gns_server_t* server = (gns_server_t*)calloc(1, sizeof(gns_server_t));
    server->on_receive = on_receive;
    server->on_connect = on_connect;
    server->on_disconnect = on_disconnect;

    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET) {
        free(server);
        return NULL;
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        closesocket(listen_sock);
        free(server);
        return NULL;
    }
    listen(listen_sock, GNS_MAX_CLIENTS);
    server->listen_socket = listen_sock;
    for (int i = 0; i < GNS_MAX_CLIENTS; ++i) server->clients[i].active = 0;
    return server;
}

gns_server_t* gns_start_server_ex(uint16_t port, gns_on_receive_ex_cb on_receive, gns_on_connect_ex_cb on_connect, gns_on_disconnect_ex_cb on_disconnect) {
    gns_init_winsock();
    gns_server_t* server = (gns_server_t*)calloc(1, sizeof(gns_server_t));
    server->on_receive = NULL; // Eski callback kullanılmaz
    server->on_connect = NULL;
    server->on_disconnect = NULL;
    // Gelişmiş callback'ler için özel alanlar
    server->on_receive_ex = on_receive;
    server->on_connect_ex = on_connect;
    server->on_disconnect_ex = on_disconnect;

    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET) {
        free(server);
        return NULL;
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        closesocket(listen_sock);
        free(server);
        return NULL;
    }
    listen(listen_sock, GNS_MAX_CLIENTS);
    server->listen_socket = listen_sock;
    for (int i = 0; i < GNS_MAX_CLIENTS; ++i) {
        server->clients[i].active = 0;
        server->clients[i].user_data = NULL;
    }
    return server;
}

void gns_run(void* instance) {
    gns_server_t* server = (gns_server_t*)instance;
    fd_set readfds;
    int maxfd;
    time_t last_keepalive = time(NULL);
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server->listen_socket, &readfds);
        maxfd = (int)server->listen_socket;
        for (int i = 0; i < GNS_MAX_CLIENTS; ++i) {
            if (server->clients[i].active) {
                FD_SET(server->clients[i].socket, &readfds);
                if ((int)server->clients[i].socket > maxfd) maxfd = (int)server->clients[i].socket;
            }
        }
        struct timeval tv;
        tv.tv_sec = 1; tv.tv_usec = 0;
        int activity = select(maxfd+1, &readfds, NULL, NULL, &tv);
        if (activity < 0) continue;
        // Yeni bağlantı
        if (FD_ISSET(server->listen_socket, &readfds)) {
            struct sockaddr_in cli_addr;
            int addrlen = sizeof(cli_addr);
            SOCKET new_sock = accept(server->listen_socket, (struct sockaddr*)&cli_addr, &addrlen);
            if (new_sock != INVALID_SOCKET) {
                int found = 0;
                for (int i = 0; i < GNS_MAX_CLIENTS; ++i) {
                    if (!server->clients[i].active) {
                        server->clients[i].socket = new_sock;
                        server->clients[i].addr = cli_addr;
                        server->clients[i].last_active = (uint32_t)time(NULL);
                        server->clients[i].active = 1;
                        server->clients[i].user_data = NULL;
                        if (server->on_connect) server->on_connect(i, &cli_addr);
                        if (server->on_connect_ex) {
                            static gnet_socket_t gnet_client;
                            gnet_client.id = i;
                            gnet_client.socket = new_sock;
                            gnet_client.addr = cli_addr;
                            gnet_client.user_data = NULL;
                            server->on_connect_ex(&gnet_client);
                        }
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    closesocket(new_sock);
                }
            }
        }
        // Veri alımı ve bağlantı kopma
        for (int i = 0; i < GNS_MAX_CLIENTS; ++i) {
            if (server->clients[i].active && FD_ISSET(server->clients[i].socket, &readfds)) {
                char buf[1024];
                int len = recv(server->clients[i].socket, buf, sizeof(buf), 0);
                if (len > 0) {
                    server->clients[i].last_active = (uint32_t)time(NULL);
                    if (len == GNS_KEEPALIVE_MESSAGE_LEN && memcmp(buf, GNS_KEEPALIVE_MESSAGE, GNS_KEEPALIVE_MESSAGE_LEN) == 0) {
                        continue;
                    }
                    if (server->on_receive) server->on_receive(buf, len);
                    if (server->on_receive_ex) {
                        static gnet_socket_t gnet_client;
                        gnet_client.id = i;
                        gnet_client.socket = server->clients[i].socket;
                        gnet_client.addr = server->clients[i].addr;
                        gnet_client.user_data = server->clients[i].user_data;
                        server->on_receive_ex(&gnet_client, buf, len);
                    }
                } else {
                    closesocket(server->clients[i].socket);
                    server->clients[i].active = 0;
                    if (server->on_disconnect) server->on_disconnect(i);
                    if (server->on_disconnect_ex) {
                        static gnet_socket_t gnet_client;
                        gnet_client.id = i;
                        gnet_client.socket = server->clients[i].socket;
                        gnet_client.addr = server->clients[i].addr;
                        gnet_client.user_data = server->clients[i].user_data;
                        server->on_disconnect_ex(&gnet_client);
                    }
                }
            }
        }
    }
}

int gns_server_send(gns_server_t* server, int client_id, const char* data, int len) {
    if (!server || client_id < 0 || client_id >= GNS_MAX_CLIENTS) return -1;
    if (!server->clients[client_id].active) return -2;
    return send(server->clients[client_id].socket, data, len, 0);
}

void gns_stop(void* instance) {
    gns_server_t* server = (gns_server_t*)instance;
    for (int i = 0; i < GNS_MAX_CLIENTS; ++i) {
        if (server->clients[i].active) {
            closesocket(server->clients[i].socket);
            server->clients[i].active = 0;
        }
    }
    closesocket(server->listen_socket);
    gns_cleanup_winsock();
    free(server);
}

gns_client_t* gns_start_client(const char* host, uint16_t port, gns_on_receive_cb on_receive) {
    gns_init_winsock();
    gns_client_t* client = (gns_client_t*)calloc(1, sizeof(gns_client_t));
    client->on_receive = on_receive;
    client->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socket == INVALID_SOCKET) {
        free(client);
        return NULL;
    }
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &server_addr.sin_addr);
    if (connect(client->socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        closesocket(client->socket);
        free(client);
        return NULL;
    }
    return client;
}

int gns_client_send(gns_client_t* client, const char* data, int len) {
    if (!client) return -1;
    return send(client->socket, data, len, 0);
}

void gns_client_run(gns_client_t* client) {
    char buf[1024];
    while (1) {
        int len = recv(client->socket, buf, sizeof(buf), 0);
        if (len > 0) {
            if (client->on_receive) client->on_receive(buf, len);
        } else {
            break;
        }
    }
}

void gns_client_stop(gns_client_t* client) {
    if (!client) return;
    closesocket(client->socket);
    gns_cleanup_winsock();
    free(client);
}

// Thread fonksiyonları
static DWORD WINAPI gns_server_thread_func(LPVOID param) {
    gns_server_t* server = (gns_server_t*)param;
    server->running = 1;
    gns_run(server);
    server->running = 0;
    return 0;
}

int gns_server_start_async(gns_server_t* server) {
    if (!server) return -1;
    if (server->thread_handle) return -2;
    server->thread_handle = CreateThread(NULL, 0, gns_server_thread_func, server, 0, NULL);
    return server->thread_handle ? 0 : -3;
}

void gns_server_stop_async(gns_server_t* server) {
    if (!server) return;
    if (server->thread_handle) {
        TerminateThread(server->thread_handle, 0);
        CloseHandle(server->thread_handle);
        server->thread_handle = NULL;
    }
    gns_stop(server);
}

static DWORD WINAPI gns_client_thread_func(LPVOID param) {
    gns_client_t* client = (gns_client_t*)param;
    client->running = 1;
    gns_client_run(client);
    client->running = 0;
    return 0;
}

int gns_client_start_async(gns_client_t* client) {
    if (!client) return -1;
    if (client->thread_handle) return -2;
    client->thread_handle = CreateThread(NULL, 0, gns_client_thread_func, client, 0, NULL);
    return client->thread_handle ? 0 : -3;
}

void gns_client_stop_async(gns_client_t* client) {
    if (!client) return;
    if (client->thread_handle) {
        TerminateThread(client->thread_handle, 0);
        CloseHandle(client->thread_handle);
        client->thread_handle = NULL;
    }
    gns_client_stop(client);
}

int gns_server_client_count(gns_server_t* server) {
    if (!server) return 0;
    int count = 0;
    for (int i = 0; i < GNS_MAX_CLIENTS; ++i) {
        if (server->clients[i].active) count++;
    }
    return count;
}

void gns_server_kick(gns_server_t* server, int client_id) {
    if (!server || client_id < 0 || client_id >= GNS_MAX_CLIENTS) return;
    if (server->clients[client_id].active) {
        closesocket(server->clients[client_id].socket);
        server->clients[client_id].active = 0;
        if (server->on_disconnect) server->on_disconnect(client_id);
    }
}

int gns_server_broadcast(gns_server_t* server, const char* data, int len) {
    if (!server) return 0;
    int sent = 0;
    for (int i = 0; i < GNS_MAX_CLIENTS; ++i) {
        if (server->clients[i].active) {
            if (send(server->clients[i].socket, data, len, 0) == len) sent++;
        }
    }
    return sent;
} 

gnet_socket_t* gns_server_get_client(gns_server_t* server, int client_id) {
    if (!server || client_id < 0 || client_id >= GNS_MAX_CLIENTS) return NULL;
    if (!server->clients[client_id].active) return NULL;
    static gnet_socket_t gnet_client;
    gnet_client.id = client_id;
    gnet_client.socket = server->clients[client_id].socket;
    gnet_client.addr = server->clients[client_id].addr;
    return &gnet_client;
}

int gns_server_send2(gns_server_t* server, gnet_socket_t* client, const char* data, int len) {
    if (!server || !client) return -1;
    return send(client->socket, data, len, 0);
}

void gns_server_kick2(gns_server_t* server, gnet_socket_t* client) {
    if (!server || !client) return;
    int id = client->id;
    if (server->clients[id].active) {
        closesocket(server->clients[id].socket);
        server->clients[id].active = 0;
        if (server->on_disconnect) server->on_disconnect(id);
    }
} 

// UDP sunucu için threadli başlatma
int gnet_udp_server_start_async(gnet_udp_server_t* server) {
    if (!server || server->thread_handle) return -1;
    server->running = 1;
    server->thread_handle = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)[](void* arg) -> unsigned {
        gnet_udp_server_t* s = (gnet_udp_server_t*)arg;
        char buf[2048];
        while (s->running) {
            struct sockaddr_in from; int fromlen = sizeof(from);
            int len = recvfrom(s->socket, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromlen);
            if (len > 0 && s->on_receive) s->on_receive(&from, buf, len, s->user_data);
        }
        return 0;
    }, server, 0, NULL);
    return server->thread_handle ? 0 : -2;
}

void gnet_udp_server_stop_async(gnet_udp_server_t* server) {
    if (!server) return;
    server->running = 0;
    if (server->thread_handle) {
        WaitForSingleObject(server->thread_handle, 1000);
        CloseHandle(server->thread_handle);
        server->thread_handle = NULL;
    }
    closesocket(server->socket);
    free(server);
}

// UDP istemci için threadli başlatma
int gnet_udp_client_start_async(gnet_udp_client_t* client, void (*on_receive)(const char* data, int len)) {
    if (!client || client->thread_handle) return -1;
    client->running = 1;
    client->thread_handle = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)[](void* arg) -> unsigned {
        gnet_udp_client_t* c = (gnet_udp_client_t*)arg;
        char buf[2048];
        struct sockaddr_in from; int fromlen = sizeof(from);
        while (c->running) {
            int len = recvfrom(c->socket, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromlen);
            if (len > 0 && on_receive) on_receive(buf, len);
        }
        return 0;
    }, client, 0, NULL);
    return client->thread_handle ? 0 : -2;
}

void gnet_udp_client_stop_async(gnet_udp_client_t* client) {
    if (!client) return;
    client->running = 0;
    if (client->thread_handle) {
        WaitForSingleObject(client->thread_handle, 1000);
        CloseHandle(client->thread_handle);
        client->thread_handle = NULL;
    }
    closesocket(client->socket);
    free(client);
} 

// Buffer ayarı fonksiyonları
void gns_server_set_buffer_size(gns_server_t* server, int size) {
    if (!server) return;
    server->buffer_size = size;
    setsockopt(server->listen_socket, SOL_SOCKET, SO_RCVBUF, (char*)&size, sizeof(size));
    setsockopt(server->listen_socket, SOL_SOCKET, SO_SNDBUF, (char*)&size, sizeof(size));
}
void gns_server_set_autobufferassign(gns_server_t* server, int enable) {
    if (!server) return;
    server->autobufferassign = enable;
    if (enable) {
        int max = 65536;
        setsockopt(server->listen_socket, SOL_SOCKET, SO_RCVBUF, (char*)&max, sizeof(max));
        setsockopt(server->listen_socket, SOL_SOCKET, SO_SNDBUF, (char*)&max, sizeof(max));
        server->buffer_size = max;
    }
}
void gns_client_set_buffer_size(gns_client_t* client, int size) {
    if (!client) return;
    client->buffer_size = size;
    setsockopt(client->socket, SOL_SOCKET, SO_RCVBUF, (char*)&size, sizeof(size));
    setsockopt(client->socket, SOL_SOCKET, SO_SNDBUF, (char*)&size, sizeof(size));
}
void gns_client_set_autobufferassign(gns_client_t* client, int enable) {
    if (!client) return;
    client->autobufferassign = enable;
    if (enable) {
        int max = 65536;
        setsockopt(client->socket, SOL_SOCKET, SO_RCVBUF, (char*)&max, sizeof(max));
        setsockopt(client->socket, SOL_SOCKET, SO_SNDBUF, (char*)&max, sizeof(max));
        client->buffer_size = max;
    }
}
void gnet_udp_server_set_buffer_size(gnet_udp_server_t* server, int size) {
    if (!server) return;
    server->buffer_size = size;
    setsockopt(server->socket, SOL_SOCKET, SO_RCVBUF, (char*)&size, sizeof(size));
    setsockopt(server->socket, SOL_SOCKET, SO_SNDBUF, (char*)&size, sizeof(size));
}
void gnet_udp_server_set_autobufferassign(gnet_udp_server_t* server, int enable) {
    if (!server) return;
    server->autobufferassign = enable;
    if (enable) {
        int max = 65536;
        setsockopt(server->socket, SOL_SOCKET, SO_RCVBUF, (char*)&max, sizeof(max));
        setsockopt(server->socket, SOL_SOCKET, SO_SNDBUF, (char*)&max, sizeof(max));
        server->buffer_size = max;
    }
}
void gnet_udp_client_set_buffer_size(gnet_udp_client_t* client, int size) {
    if (!client) return;
    client->buffer_size = size;
    setsockopt(client->socket, SOL_SOCKET, SO_RCVBUF, (char*)&size, sizeof(size));
    setsockopt(client->socket, SOL_SOCKET, SO_SNDBUF, (char*)&size, sizeof(size));
}
void gnet_udp_client_set_autobufferassign(gnet_udp_client_t* client, int enable) {
    if (!client) return;
    client->autobufferassign = enable;
    if (enable) {
        int max = 65536;
        setsockopt(client->socket, SOL_SOCKET, SO_RCVBUF, (char*)&max, sizeof(max));
        setsockopt(client->socket, SOL_SOCKET, SO_SNDBUF, (char*)&max, sizeof(max));
        client->buffer_size = max;
    }
} 

// Log callback
static gnet_log_cb gnet_log_callback = NULL;
void gnet_set_log_callback(gnet_log_cb cb) { gnet_log_callback = cb; }
void gnet_log(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (gnet_log_callback) gnet_log_callback(buf);
    else fprintf(stderr, "%s\n", buf);
}

// Hata kodu açıklama
const char* gnet_strerror(gnet_err_t code) {
    switch (code) {
        case GNET_OK: return "Başarılı";
        case GNET_ERR_SOCKET: return "Socket oluşturulamadı";
        case GNET_ERR_BIND: return "Bind başarısız";
        case GNET_ERR_CONNECT: return "Bağlantı başarısız";
        case GNET_ERR_SEND: return "Veri gönderilemedi";
        case GNET_ERR_RECV: return "Veri alınamadı";
        case GNET_ERR_MEMORY: return "Bellek yetersiz";
        case GNET_ERR_PARAM: return "Geçersiz parametre";
        default: return "Bilinmeyen hata";
    }
} 

// Relay sunucuda peer id ile veri yönlendirme
int gnet_relay_server_forward(gnet_relay_server_t* server, int target_peer_id, const char* data, int len) {
    if (!server) return GNET_ERR_PARAM;
    gnet_relay_peer_info_t* peer = NULL;
    for (int i = 0; i < GNET_RELAY_MAX_PEERS; ++i) {
        if (server->peers[i].connected && server->peers[i].id == target_peer_id) {
            peer = &server->peers[i];
            break;
        }
    }
    if (!peer) return GNET_ERR_PARAM;
    int sent = send(peer->socket, data, len, 0);
    if (sent != len) return GNET_ERR_SEND;
    return GNET_OK;
}

// Relay sunucuda broadcast (bir peer hariç)
int gnet_relay_server_broadcast(gnet_relay_server_t* server, int except_peer_id, const char* data, int len) {
    if (!server) return GNET_ERR_PARAM;
    int count = 0;
    for (int i = 0; i < GNET_RELAY_MAX_PEERS; ++i) {
        if (server->peers[i].connected && server->peers[i].id != except_peer_id) {
            int sent = send(server->peers[i].socket, data, len, 0);
            if (sent == len) count++;
        }
    }
    return count;
}

gnet_relay_peer_info_t* gnet_relay_server_get_peer_by_id(gnet_relay_server_t* server, int peer_id) {
    if (!server) return NULL;
    for (int i = 0; i < GNET_RELAY_MAX_PEERS; ++i) {
        if (server->peers[i].connected && server->peers[i].id == peer_id) {
            return &server->peers[i];
        }
    }
    return NULL;
} 

// UDP relay forwarding
int gnet_relay_server_forward_udp(gnet_relay_server_t* server, int target_peer_id, const char* data, int len) {
    if (!server) return GNET_ERR_PARAM;
    gnet_relay_peer_info_t* peer = NULL;
    for (int i = 0; i < GNET_RELAY_MAX_PEERS; ++i) {
        if (server->peers[i].connected && server->peers[i].id == target_peer_id) {
            peer = &server->peers[i];
            break;
        }
    }
    if (!peer) return GNET_ERR_PARAM;
    int sent = sendto(server->udp_socket, data, len, 0, (struct sockaddr*)&peer->addr, sizeof(peer->addr));
    if (sent != len) return GNET_ERR_SEND;
    return GNET_OK;
} 

// Mini JSON encode/decode (sıfır bağımlılık, sadece string ve int)
int gnet_json_set_str(char* json, int maxlen, const char* key, const char* value) {
    int len = (int)strlen(json);
    if (len > 1 && json[len-1] == '}') json[len-1] = 0, len--;
    int n = snprintf(json+len, maxlen-len, "%s\"%s\":\"%s\"}", (len>1?",":"{"), key, value);
    return (n > 0) ? 0 : -1;
}
int gnet_json_set_int(char* json, int maxlen, const char* key, int value) {
    int len = (int)strlen(json);
    if (len > 1 && json[len-1] == '}') json[len-1] = 0, len--;
    int n = snprintf(json+len, maxlen-len, "%s\"%s\":%d}", (len>1?",":"{"), key, value);
    return (n > 0) ? 0 : -1;
}
int gnet_json_get_str(const char* json, const char* key, char* out, int maxlen) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char* p = strstr(json, pat);
    if (!p) return -1;
    p += strlen(pat);
    const char* q = strchr(p, '"');
    if (!q) return -1;
    int n = (int)(q-p);
    if (n >= maxlen) n = maxlen-1;
    strncpy(out, p, n); out[n] = 0;
    return 0;
}
int gnet_json_get_int(const char* json, const char* key, int* out) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char* p = strstr(json, pat);
    if (!p) return -1;
    p += strlen(pat);
    *out = atoi(p);
    return 0;
}

// Kimlik doğrulama (token/handshake)
static char gnet_server_token[64] = {0};
static char gnet_client_token[64] = {0};

void gnet_server_set_token(gnet_server_t* server, const char* token) {
    if (token) strncpy(gnet_server_token, token, sizeof(gnet_server_token)-1);
}
void gnet_client_set_token(void* client, const char* token) {
    if (token) strncpy(gnet_client_token, token, sizeof(gnet_client_token)-1);
}

// Bağlantı başında client token gönderir, sunucu kontrol eder (örnek TCP için)
// Sunucu bağlantı kabulünde ilk gelen veri token ile eşleşmiyorsa bağlantı kapatılır
// Client bağlantı başında token'ı gönderir 