/**
 * @file    action.cpp
 * @brief   EEZ UI Action 函数实现 (用户编写, 不会被 EEZ Studio 覆盖)
 *
 * 功能: 面团重量加减、预设、启动电机(独立任务)、紧急停止(销毁任务)
 * 注意: 所有函数用 extern "C" 包裹以匹配 actions.h 的 C 链接声明
 */
#include "actions.h"
#include "vars.h"
#include "function.h"
#include "ui.h"
#include "ctl_mutex.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

using namespace eez;
using namespace eez::flow;

static const char *TAG = "ACTIONS";
static TaskHandle_t g_motor_task_handle = NULL;

static void motor_task(void *pvParameters)
{
    uint32_t weight = (uint32_t)(uintptr_t)pvParameters;
    weight_work(weight);
    g_motor_task_handle = NULL;
    vTaskDelete(NULL);
}

extern "C" {

void action_dough_sub(lv_event_t *e)
{
    int32_t w = get_var_dough_weight();
    if (w > 50) {
        set_var_dough_weight(w - 50);
    }
    ESP_LOGI(TAG, "Dough sub: %ld -> %ld g", (long)w, (long)get_var_dough_weight());
}

void action_dough_add(lv_event_t *e)
{
    int32_t w = get_var_dough_weight();
    set_var_dough_weight(w + 50);
    ESP_LOGI(TAG, "Dough add: %ld -> %ld g", (long)w, (long)get_var_dough_weight());
}

void action_set_200g(lv_event_t *e)
{
    set_var_dough_weight(200);
    ESP_LOGI(TAG, "Preset: 200g");
}

void action_set_300g(lv_event_t *e)
{
    set_var_dough_weight(300);
    ESP_LOGI(TAG, "Preset: 300g");
}

void action_set_500g(lv_event_t *e)
{
    set_var_dough_weight(500);
    ESP_LOGI(TAG, "Preset: 500g");
}

void action_set_1000g(lv_event_t *e)
{
    set_var_dough_weight(1000);
    ESP_LOGI(TAG, "Preset: 1000g");
}

void action_star_mixer(lv_event_t *e)
{
    int32_t w = get_var_dough_weight();
    if (!ctl_try_acquire(CTL_TOUCH)) {
        ESP_LOGW(TAG, "System busy by %d, cannot start", (int)ctl_get_owner());
        return;
    }
    ESP_LOGI(TAG, "Start mixer, weight=%ldg", (long)w);
    set_var_motor_running(true);
    xTaskCreate(motor_task, "motor_task", 8192,
                (void *)(uintptr_t)w, 5, &g_motor_task_handle);
}

void action_stop(lv_event_t *e)
{
    ESP_LOGI(TAG, "Emergency stop");
    if (g_motor_task_handle != NULL) {
        vTaskDelete(g_motor_task_handle);
        g_motor_task_handle = NULL;
    }
    fstop();
    ctl_release(CTL_TOUCH);
    set_var_motor_running(false);
}

/** 步进电机推出 (CCW), 2.9 圈 */
void action_step_out(lv_event_t *e)
{
    ESP_LOGI(TAG, "Step out: CCW 2.9 turns");
    push_and_out(1);
}

/** 步进电机退回 (CW) */
void action_step_back(lv_event_t *e)
{
    ESP_LOGI(TAG, "Step back: CW");
    push_and_out(0);
}

} /* extern "C" */
