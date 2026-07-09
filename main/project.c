#include "ledc_pwm.h"
#include "gpiowork.h"
#include "servo.h"
#include "servo2.h"
#include "esc_controller.h"
#include "stepper.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "function.h"
#include "esp_lvgl_port.h"
#include "esp_event.h"
#include "lcd.h"
#include "touch.h"
#include "lvgl_ui.h"
#include "ui.h"
#include "vars.h"
#include <stdio.h>

static const char *TAG = "main";

static int loop_count = 0;

void app_main(void)
{
    init_all();
    while (1) 
    {
        if (loop_count++ % 100 == 0) 
        {
            ESP_LOGI("DEBUG", "LVGL loop running, free heap: %"PRIu32"", esp_get_free_heap_size());
        }
        //fwork(10000,5000,500,5000,3000);
        lvgl_port_lock(0);
        ui_tick();     /* EEZ Flow 步进: 处理动画/事件/属性绑定 */
        lvgl_port_unlock();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}