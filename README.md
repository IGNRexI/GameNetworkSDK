# GameNetworkSDK

Kolay, esnek ve modern TCP/UDP/P2P/Relay oyun ağı SDK'sı

## Özellikler
- Kolay TCP ve UDP sunucu/istemci başlatma
- Otomatik keepalive, timeout, thread/async destekli
- P2P (peer-to-peer) desteği (doğrudan veya relay ile)
- Relay sunucu ile NAT arkasında bile peer-to-peer iletişim
- Buffer yönetimi (otomatik veya manuel)
- Hata yönetimi ve log callback
- Kullanıcıya özel veri/context desteği
- Kimlik doğrulama (token/handshake)
- Otomatik reconnect ve bağlantı sağlığı event callback
- Thread-safe event queue (ağ thread'i ile oyun döngüsü arasında güvenli event aktarımı)
- Mini JSON encode/decode (sıfır bağımlılık)
- Sadece `#include "gamenetwork.h"` ile tüm temel başlıklar otomatik
- Kolay örnekler ve API

## Kurulum
```
# Sadece sdk/ klasörünü projenize ekleyin
# Derlerken -lws2_32 (Windows) veya -lpthread (Linux) ekleyin
```

## Hızlı Başlangıç (TCP Sunucu)
```c
#include "sdk/gamenetwork.h"

gns_server_t* server = gns_start_server(12345, on_receive, on_connect, on_disconnect);
gns_server_set_autobufferassign(server, 1); // Otomatik buffer
// veya gns_server_set_buffer_size(server, 8192);
gns_server_start_async(server);
// ...
gns_server_stop_async(server);
```

## Hızlı Başlangıç (UDP Sunucu)
```c
#include "sdk/gamenetwork.h"

gnet_udp_server_t* udp = gnet_udp_server_start(12345, on_udp_receive, NULL);
gnet_udp_server_set_autobufferassign(udp, 1);
gnet_udp_server_start_async(udp);
// ...
gnet_udp_server_stop_async(udp);
```

## P2P ve Relay Kullanımı
- Relay sunucu başlat: `relay_example.c`
- Peer uygulaması: `p2p_with_relay_example.c` veya `p2p_with_udp_relay_example.c`
- Peer'lar relay'e bağlanır, hedef peer id ve mesajı gönderir, relay yönlendirir.
- Sunucu/relay üzerinden peer-to-peer veri iletimi ve NAT arkasında iletişim mümkündür.

## Peer Listesi ve Relay ile Oyun Lobisi
- `server_peerlist_relay_example.c` ve `client_peerlist_relay_example.c` ile TCP üzerinden peer listesi ve relay
- `udp_server_peerlist_relay_example.c` ve `udp_client_peerlist_relay_example.c` ile UDP üzerinden peer listesi ve relay

## Kimlik Doğrulama (Token/Handshake)
```c
gnet_server_set_token(server, "my_secret_token");
gnet_client_set_token(client, "my_secret_token");
```

## Otomatik Reconnect
```c
gnet_client_set_reconnect(client, 1, 5, 2000); // 5 deneme, 2 sn aralık
gnet_set_reconnect_callback(client, my_reconnect_cb);
```

## Event Queue Kullanımı
```c
gnet_event_queue_t* q = gnet_event_queue_create(32);
gnet_event_t ev;
while (gnet_event_queue_pop(q, &ev) == 0) {
    if (ev.type == 1) printf("Client %d disconnect oldu!\n", ev.client_id);
    // ...
}
gnet_event_queue_destroy(q);
```

## Mini JSON Kullanımı
```c
char json[128] = "";
gnet_json_set_str(json, sizeof(json), "type", "move");
gnet_json_set_int(json, sizeof(json), "x", 42);
char type[32]; int x;
gnet_json_get_str(json, "type", type, sizeof(type));
gnet_json_get_int(json, "x", &x);
```

## SSS
- **Buffer boyutunu nasıl ayarlarım?**
  - Otomatik: `set_autobufferassign(..., 1)`
  - Manuel: `set_buffer_size(..., size)`
- **NAT arkasında peer'lar iletişim kurabilir mi?**
  - Evet, relay sunucu ile mümkün.
- **Ekstra include gerek var mı?**
  - Hayır, sadece `#include "gamenetwork.h"` yeterli.
- **Thread-safe event işleme mümkün mü?**
  - Evet, event queue ile ana threadde güvenli event işleyebilirsiniz.

## Örnekler
- `examples/server_example.c` : TCP sunucu
- `examples/client_example.c` : TCP istemci
- `examples/udp_server_example.c` : UDP sunucu
- `examples/udp_client_example.c` : UDP istemci
- `examples/relay_example.c` : Relay sunucu
- `examples/p2p_with_relay_example.c` : Relay üzerinden P2P
- `examples/p2p_with_udp_relay_example.c` : UDP relay üzerinden P2P
- `examples/server_peerlist_relay_example.c` : TCP peer listesi ve relay
- `examples/client_peerlist_relay_example.c` : TCP peer listesi ve relay client
- `examples/udp_server_peerlist_relay_example.c` : UDP peer listesi ve relay
- `examples/udp_client_peerlist_relay_example.c` : UDP peer listesi ve relay client

## Katkı ve Lisans
MIT Lisansı. Katkılarınızı bekliyoruz! 