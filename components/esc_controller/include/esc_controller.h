#ifndef ESC_CONTROLLER_H
#define ESC_CONTROLLER_H

#include "esp_err.h"
#include "driver/ledc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 电调控制句柄
 */
typedef struct {
    int gpio_num;               // 信号线连接的GPIO
    ledc_channel_t channel;     // 占用的LEDC通道
    bool calibrated;            // 是否已完成校准
} esc_handle_t;

/**
 * @brief 初始化一个电调实例，并完成油门行程校准
 * @param esc 电调句柄指针
 * @param gpio_num 信号线连接的GPIO引脚
 * @param channel LEDC通道编号（如 LEDC_CHANNEL_6）
 * @return ESP_OK 成功，否则失败
 */
esp_err_t esc_init(esc_handle_t *esc, int gpio_num, ledc_channel_t channel);

/**
 * @brief 设置电调油门
 * @param esc 电调句柄指针
 * @param throttle 油门值 (0.0 ~ 100.0)，0%停止，100%全速
 * @return ESP_OK 成功，否则失败
 */
esp_err_t esc_set_throttle(esc_handle_t *esc, float throttle);

#ifdef __cplusplus
}
#endif

#endif