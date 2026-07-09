#include "wss_client.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_tls.h"
#include "esp_mac.h"
#include "esp_crt_bundle.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static const char* TAG = "wss";
static esp_tls_t* g_tls = NULL;
static int g_sock = -1;
static bool g_ok = false;
static bool g_use_tls = false;
static wss_text_cb g_tcb = NULL;
static wss_bin_cb g_bcb = NULL;

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static void b64e(const uint8_t* in, int len, char* out) {
    int i, j = 0;
    for (i = 0; i < len; i += 3) {
        int v = (in[i] << 16) | ((i+1 < len ? in[i+1] : 0) << 8) | (i+2 < len ? in[i+2] : 0);
        out[j++] = B64[(v >> 18) & 0x3F]; out[j++] = B64[(v >> 12) & 0x3F];
        out[j++] = (i+1 < len) ? B64[(v >> 6) & 0x3F] : '=';
        out[j++] = (i+2 < len) ? B64[v & 0x3F] : '=';
    }
    out[j] = 0;
}

static int ws_read(uint8_t* buf, int max) {
    if (g_use_tls) return esp_tls_conn_read(g_tls, buf, max);
    return recv(g_sock, buf, max, 0);
}

static int ws_write(const uint8_t* d, int len) {
    if (g_use_tls) { size_t wb; return esp_tls_conn_write(g_tls, d, len) == (size_t)len ? len : -1; }
    return send(g_sock, d, len, 0);
}

bool wss_connect(const char* host, int port, const char* path) {
    wss_close();

    if (port == 443) {
        g_use_tls = true;
        esp_tls_cfg_t cfg = {};
        cfg.crt_bundle_attach = esp_crt_bundle_attach;
        cfg.skip_common_name = true;
        g_tls = esp_tls_init();
        if (!g_tls || esp_tls_conn_new_sync(host, strlen(host), port, &cfg, g_tls) != 1) {
            ESP_LOGE(TAG, "TLS fail"); wss_close(); return false;
        }
        ESP_LOGI(TAG, "TLS OK");
    } else {
        g_use_tls = false;
        struct hostent* he = gethostbyname(host);
        if (!he) { ESP_LOGE(TAG, "DNS fail"); return false; }
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET; addr.sin_port = htons(port);
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
        g_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (g_sock < 0 || connect(g_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            ESP_LOGE(TAG, "TCP fail"); wss_close(); return false;
        }
        ESP_LOGI(TAG, "TCP OK");
    }

    // WebSocket handshake with device-id header
    uint8_t rk[16]; char key[32];
    for (int i = 0; i < 16; i++) rk[i] = esp_random() & 0xFF;
    b64e(rk, 16, key);

    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    char device_id[32];
    snprintf(device_id, sizeof(device_id), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    char req[512];
    int rlen = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n"
        "device-id: %s\r\nclient-id: esp32s3\r\n\r\n",
        path, host, key, device_id);
    ws_write((const uint8_t*)req, rlen);

    char buf[1024] = {0};
    int ret = ws_read((uint8_t*)buf, sizeof(buf) - 1);
    if (ret <= 0 || !strstr(buf, "101")) {
        buf[ret>0?ret:0] = 0; ESP_LOGE(TAG, "WS fail: %s", buf); wss_close(); return false;
    }
    g_ok = true;
    ESP_LOGI(TAG, "WS connected");
    return true;
}

void wss_close(void) {
    g_ok = false;
    if (g_use_tls && g_tls) { esp_tls_conn_destroy(g_tls); g_tls = NULL; }
    if (!g_use_tls && g_sock >= 0) { close(g_sock); g_sock = -1; }
}

bool wss_is_connected(void) { return g_ok; }
void wss_on_text(wss_text_cb cb) { g_tcb = cb; }
void wss_on_bin(wss_bin_cb cb) { g_bcb = cb; }

// WebSocket frame with MASKING (required for client-to-server)
static bool send_frame(int op, const uint8_t* d, size_t n) {
    if (!g_ok) return false;
    uint8_t f[4096]; size_t p = 0;
    f[p++] = 0x80 | op;                    // FIN + opcode
    if (n < 126) {
        f[p++] = 0x80 | (uint8_t)n;        // MASK bit set + length
    } else if (n < 65536) {
        f[p++] = 0x80 | 126;               // MASK + extended 16-bit
        f[p++] = (n >> 8) & 0xFF;
        f[p++] = n & 0xFF;
    }
    uint8_t mk[4];
    for (int i = 0; i < 4; i++) mk[i] = esp_random() & 0xFF;
    memcpy(f + p, mk, 4); p += 4;
    for (size_t i = 0; i < n; i++) f[p++] = d[i] ^ mk[i % 4];
    return ws_write(f, p) == (int)p;
}

bool wss_send_text(const char* d, size_t n) { return send_frame(0x01, (const uint8_t*)d, n); }
bool wss_send_bin(const uint8_t* d, size_t n) { return send_frame(0x02, d, n); }

void wss_poll(void) {
    if (!g_ok) return;
    uint8_t buf[4096];
    int n = ws_read(buf, sizeof(buf));
    if (n == 0) { ESP_LOGI(TAG, "TCP closed by server"); wss_close(); return; }
    if (n <= 0) return;
    size_t pos = 0;
    while (pos < (size_t)n) {
        int op = buf[pos] & 0x0F; pos++;
        bool masked = buf[pos] & 0x80;
        uint64_t pl = buf[pos] & 0x7F; pos++;
        if (pl == 126) { pl = (buf[pos]<<8)|buf[pos+1]; pos += 2; }
        else if (pl == 127) { pos += 8; continue; }
        uint8_t mk[4] = {0};
        if (masked) { memcpy(mk, buf+pos, 4); pos += 4; }
        uint8_t* py = buf + pos;
        if (masked) for (size_t i = 0; i < pl; i++) py[i] ^= mk[i%4];
        if (op == 0x01 && g_tcb) g_tcb((const char*)py, pl);
        else if (op == 0x02 && g_bcb) g_bcb(py, pl);
        else if (op == 0x08) { ESP_LOGI(TAG, "Server closed"); wss_close(); return; }
        else if (op == 0x09) {
            ESP_LOGI(TAG, "Ping (len=%llu), sending masked pong", pl);
            send_frame(0x0A, py, pl);
        }
        pos += pl;
    }
}
