# ESP32 Zephyr — WiFi + HTTP

Full flow: connect to WiFi → get IP → ping → DNS lookup → TCP connect → HTTP GET.

---

## Architecture Overview

```
main()
 ├── Register event callbacks (WiFi + IPv4)
 ├── wifi_connect()
 │    └── waits on k_sem: wifi_connected
 ├── wifi_status()         ← print SSID, band, channel, RSSI
 │    └── waits on k_sem: ipv4_address_obtained
 ├── ping("8.8.8.8", 4)
 ├── nslookup(hostname)
 ├── connect_socket()
 └── http_get()
```

---

## 1. Event System — Two Semaphores

```c
K_SEM_DEFINE(wifi_connected, 0, 1);        // given when WiFi handshake succeeds
K_SEM_DEFINE(ipv4_address_obtained, 0, 1); // given when DHCP assigns an IP
```

Both semaphores start at `0` so `main()` **blocks** on `k_sem_take()` until the network events fire.

A single handler `wifi_mgmt_event_handler()` receives all three events and dispatches them:

| Event | Handler called | Action |
|---|---|---|
| `NET_EVENT_WIFI_CONNECT_RESULT` | `handle_wifi_connect_result` | gives `wifi_connected` sem |
| `NET_EVENT_WIFI_DISCONNECT_RESULT` | `handle_wifi_disconnect_result` | takes `wifi_connected` sem back |
| `NET_EVENT_IPV4_ADDR_ADD` | `handle_ipv4_result` | prints IP/subnet/gateway, gives `ipv4_address_obtained` sem |

---

## 2. WiFi Connection — `wifi_connect()`

```c
struct wifi_connect_req_params wifi_params = {
    .ssid        = SSID,
    .psk         = PSK,
    .channel     = WIFI_CHANNEL_ANY,
    .security    = WIFI_SECURITY_TYPE_PSK,
    .band        = WIFI_FREQ_BAND_2_4_GHZ,
    .mfp         = WIFI_MFP_OPTIONAL,   // Management Frame Protection
};
net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params, sizeof(wifi_params));
```

`net_mgmt()` is non-blocking — it just submits the request. The result arrives asynchronously via the event callback, which then gives the semaphore.

---

## 3. WiFi Status — `wifi_status()`

```c
net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status, sizeof(status));
```

Prints SSID, frequency band, channel, security type, and RSSI signal strength. Called after `wifi_connected` semaphore fires.

---

## 4. Ping — `ping(char *ipv4_addr, uint8_t count)`

Uses Zephyr's ICMP API (not a raw socket).

**Steps:**
1. `net_addr_pton()` — parse the IP string into a `sockaddr_in`
2. `net_icmp_init_ctx()` — register a listener for `NET_ICMPV4_ECHO_REPLY` (ICMP type 0)
3. Loop `count` times:
   - `net_icmp_send_echo_request()` — sends the ping packet
   - `k_sleep(K_SECONDS(2))` — waits for reply (callback fires asynchronously)
4. `net_icmp_cleanup_ctx()` — unregister the listener

```c
// Reply handler — fires automatically when a reply packet arrives
enum net_verdict icmp_echo_reply_handler(...){
    printk("Ping reply received!\n");
    return NET_OK;
}
```

---

## 5. DNS Lookup — `nslookup()`

```c
struct zsock_addrinfo hints = {
    .ai_family   = AF_UNSPEC,     // accept IPv4 or IPv6
    .ai_socktype = SOCK_STREAM,
};
zsock_getaddrinfo(hostname, NULL, &hints, &results);
```

Returns a **linked list** of `zsock_addrinfo` structs. Each node has an `ai_addr` field with either a `sockaddr_in` (IPv4) or `sockaddr_in6` (IPv6).

`print_addrinfo_results()` iterates the list and converts addresses to strings using `zsock_inet_ntop()`.

---

## 6. TCP Connect — `connect_socket()`

Tries IPv6 first, falls back to IPv4 automatically:

```
1. Create AF_INET6 socket
2. Loop through results → try zsock_connect() on each IPv6 address
   → if success: return socket fd
3. Close IPv6 socket
4. Create AF_INET socket
5. Loop through results → try zsock_connect() on each IPv4 address
   → if success: return socket fd
```

The port is injected into the address struct before connecting:
```c
sa->sin_port = htons(port);   // e.g. htons(80) for HTTP
```

---

## 7. HTTP GET — `http_get(int sock, char *hostname, char *url)`

```c
struct http_request req = {
    .method       = HTTP_GET,
    .url          = url,          // e.g. "/LoremIpsum.txt"
    .host         = hostname,
    .protocol     = "HTTP/1.1",
    .response     = http_response_cb,
    .recv_buf     = recv_buf,     // 512 byte static buffer
    .recv_buf_len = sizeof(recv_buf),
};
http_client_req(sock, &req, 5000, NULL);  // 5 second timeout
```

The response callback `http_response_cb` is called multiple times as chunks arrive:

| `final_data` value | Meaning |
|---|---|
| `HTTP_DATA_MORE` | Partial chunk, more coming |
| `HTTP_DATA_FINAL` | Last chunk, transfer complete |

Each call prints `rsp->recv_buf` up to `rsp->data_len` bytes.

---

## Full Execution Flow

```
Boot
 │
 ├─ Register wifi_cb  (connect/disconnect events)
 ├─ Register ipv4_cb  (DHCP address event)
 │
 ├─ wifi_connect()  ──────────────────────► [async] WiFi handshake
 ├─ k_sem_take(wifi_connected) BLOCKS      [event fires] → sem given
 ├─ wifi_status()   prints connection info
 ├─ k_sem_take(ipv4_obtained)  BLOCKS      [DHCP fires]  → sem given
 │
 ├─ ping("8.8.8.8", 4)         4x ICMP echo → reply callback
 │
 ├─ nslookup("iot.beyondlogic.org")
 ├─ print_addrinfo_results()   prints resolved IPs
 │
 ├─ connect_socket(&res, 80)   tries IPv6 → falls back to IPv4
 ├─ http_get(sock, host, url)  sends GET / receives response
 └─ zsock_close(sock)
```

---

## Key Zephyr APIs Used

| API | Purpose |
|---|---|
| `net_mgmt()` | Send WiFi requests (connect, disconnect, status) |
| `net_mgmt_init_event_callback()` | Register async event listener |
| `K_SEM_DEFINE` / `k_sem_take/give` | Synchronize main thread with async events |
| `zsock_getaddrinfo()` | DNS resolution |
| `zsock_socket/connect/close()` | TCP socket lifecycle |
| `http_client_req()` | Send HTTP request over open socket |
| `net_icmp_init_ctx()` | Register ICMP reply listener |
| `net_icmp_send_echo_request()` | Send ping packet |

---

## Build & Flash for ESP32

### Source 
```sh
source /home/ehab/zephyrproject/zephyr/.venv/bin/activate
```

### Build
``` sh
west build -p always -b esp32_devkitc/esp32/procpu . --extra-dtc-overlay board/esp32.overlay -DPython3_EXECUTABLE=/home/ehab/zephyrproject/.venv/bin/python3
```

### Flash
``` sh
west flash --esp-device /dev/ttyUSB0
```

### Monitor
``` sh
pip install esp-idf-monitor
python -m esp_idf_monitor --port /dev/ttyUSB0 --baud 115200 build/zephyr/zephyr.elf
# to terminate
# ctrl + ]
```

### Output
```sh
WiFi Example
Board: esp32_devkitc
Connecting to SSID: ITI_Students
IPv4 address: 10.145.1.12
Subnet: 255.255.0.0
Router: 10.145.0.1
Connected

SSID: ITI_Students                    
Band: 2.4GHz
Channel: 1
Security: WPA2-PSK
RSSI: -78
Ready...

Looking up google...
IPv4: 172.217.169.238
IPv6: 2a00:1450:4002:409::200e
Sending Ping #1 to 172.217.169.238...
Ping reply received!
Sending Ping #2 to 172.217.169.238...
Ping reply received!
Sending Ping #3 to 172.217.169.238...
Ping reply received!
Sending Ping #4 to 172.217.169.238...
Ping reply received!
Connecting to 2a00:1450:4002:409::200e:80 Failure (-1)
Connecting to 172.217.169.238:80 Success
HTTP/1.1 301 Moved Permanently
Location: http://www.google.com/
Content-Type: text/html; charset=UTF-8
Content-Security-Policy-Report-Only: object-src 'none';base-uri 'self';script-src 'nonce--vZ7MxmCyRdveY4-vI0fQw' 'strict-dynamic' 'report-sample' 'unsafe-eval' 'unsafe-inline' https: http:;report-uri https://csp.withgoogle.com/csp/gws/other-hp
Date: Wed, 15 Apr 2026 11:42:22 GMT
Expires: Fri, 15 May 2026 11:42:22 GMT
Cache-Control: public, max-age=2592000
Server: gws
Content-Length: 219
X-XSS-Protection: 0
X-Frame-Options: SAMEORIGIN

<HTML><HEAD><meta http-equiv="content-type" content="text/html;charset=utf-8">
<TITLE>301 Moved</TITLE></HEAD><BODY>
<H1>301 Moved</H1>
The document has moved
<A HREF="http://www.google.com/">here</A>.
</BODY></HTML>
```
