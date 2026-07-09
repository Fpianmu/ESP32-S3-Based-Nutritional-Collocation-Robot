#ifndef GPIOWORK_H
#define GPIOWORK_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动一个独立任务，设置GPIO的电平
 * @param gpio_num      GPIO 编号 (如 2)
 * @param level         电平，0 或 1
 * @return esp_err_t ESP_OK 成功
 */
void gpio_set(int gpio_num,int level);

#ifdef __cplusplus
}
#endif

#endif