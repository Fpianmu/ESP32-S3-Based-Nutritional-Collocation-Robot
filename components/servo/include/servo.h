#ifndef SERVO_H
#define SERVO_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化舵机
 * @param gpio_num 信号线连接的 GPIO 引脚
 * @return ESP_OK 成功，否则失败
 */
esp_err_t servo_init(int gpio_num);

/**
 * @brief 设置舵机角度
 * @param angle 目标角度 (0 - 180)
 * @return ESP_OK 成功，否则失败
 */
esp_err_t servo_set_angle(int angle);

#ifdef __cplusplus
}
#endif
#endif