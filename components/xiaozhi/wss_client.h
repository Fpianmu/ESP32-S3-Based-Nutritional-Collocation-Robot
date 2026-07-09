#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*wss_text_cb)(const char* d, size_t n);
typedef void (*wss_bin_cb)(const uint8_t* d, size_t n);
bool wss_connect(const char* host, int port, const char* path);
void wss_close(void);
bool wss_is_connected(void);
bool wss_send_text(const char* d, size_t n);
bool wss_send_bin(const uint8_t* d, size_t n);
void wss_on_text(wss_text_cb cb);
void wss_on_bin(wss_bin_cb cb);
void wss_poll(void);
#ifdef __cplusplus
}
#endif
