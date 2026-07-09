/**
 * @file    vars.cpp
 * @brief   EEZ UI 全局变量实现 (用户编写, 不会被 EEZ Studio 覆盖)
 *
 * 管理 dough_weight / motor_running 变量的 getter/setter
 */
#include "vars.h"
#include "ui.h"
#include "esp_log.h"

using namespace eez;

int32_t dough_weight = 50; // 默认50克
bool    motor_running = false;

extern "C" {

int32_t get_var_dough_weight() {
    return dough_weight;
}

void set_var_dough_weight(int32_t value) {
    dough_weight = value;
    ESP_LOGI("VARS", "dough_weight updated to %ld g", (long)dough_weight);
}

bool get_var_motor_running() {
    return motor_running;
}

void set_var_motor_running(bool value) {
    motor_running = value;
}

/* EEZ Studio 生成代码仍引用, 保留空桩 */
int32_t get_var_timer_sec() { return 0; }
void set_var_timer_sec(int32_t value) { (void)value; }
int32_t get_var_timer_min() { return 0; }
void set_var_timer_min(int32_t value) { (void)value; }

void vars_init() {
    dough_weight = 200;
    motor_running = false;
    ESP_LOGI("VARS", "Variables initialized, dough_weight=%ld g", (long)dough_weight);
}

} /* extern "C" */
