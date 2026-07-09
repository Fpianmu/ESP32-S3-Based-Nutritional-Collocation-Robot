#ifndef XIAOZHI_H
#define XIAOZHI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * @brief 初始化小智语音控制模块
 *
 * 启动音频管线 (I2S: BCLK=14 WS=16 DIN=17 DOUT=7),
 * 连接 WebSocket 服务器, 加载唤醒词模型.
 * 需要在 WiFi AP 启动后调用.
 */
void xz_init(void);

/**
 * @brief FreeRTOS 任务包装 — 直接传给 xTaskCreate
 */
static inline void xz_init_wrapper(void *arg) {
    (void)arg;
    xz_init();
}

/**
 * @brief 查询小智是否正在说话 (TTS 播放中)
 */
bool xz_is_speaking(void);

/**
 * @brief 查询小智是否处于录音/识别状态
 */
bool xz_is_listening(void);

#ifdef __cplusplus
}
#endif

#endif
