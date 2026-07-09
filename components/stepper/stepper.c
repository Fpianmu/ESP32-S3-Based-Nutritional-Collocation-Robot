#include "stepper.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_log.h"

static const char *TAG = "stepper";

// 静态变量，保存引脚和当前状态
static int stp_pin = -1;
static int en_pin = -1;
static int dir_pin = -1;
static uint32_t default_duration_ms = 0; // 默认运行时间
static stepper_state_t current_state = STEPPER_STATE_STOPPED;

// 软件定时器句柄
static TimerHandle_t stop_timer = NULL;
static ledc_mode_t speed_mode = LEDC_LOW_SPEED_MODE;

// 停止定时器的回调函数
static void stepper_stop_timer_callback(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "Duration timer expired, stopping motor...");
    stepper_stop();
}

// 内部辅助函数：停止定时器并删除
static void timer_stop_and_reset(void) {
    if (stop_timer != NULL) {
        xTimerStop(stop_timer, 0);
        xTimerDelete(stop_timer, 0);
        stop_timer = NULL;
    }
}

esp_err_t stepper_init(const stepper_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    stp_pin = config->stp_pin;
    en_pin = config->en_pin;
    dir_pin = config->dir_pin;
    default_duration_ms = config->duration_ms; // 保存默认运行时间

    ESP_LOGI(TAG, "Initializing stepper: EN:%d, DIR:%d, STP:%d, default Freq:%lu Hz, default duration:%lu ms",
             en_pin, dir_pin, stp_pin, config->freq_hz, default_duration_ms);

    // ---- GPIO 初始化 ----
    // EN 引脚：低电平有效，初始高电平（失能）
    gpio_reset_pin(en_pin);
    gpio_set_direction(en_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(en_pin, 1);

    // DIR 引脚：默认低电平
    gpio_reset_pin(dir_pin);
    gpio_set_direction(dir_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(dir_pin, 0);

    // ---- LEDC 初始化 ----
    ledc_timer_config_t timer_cfg = {
        .speed_mode = speed_mode,
        .timer_num = LEDC_TIMER_1,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = config->freq_hz,
        .clk_cfg = LEDC_AUTO_CLK
    };
    esp_err_t ret = ledc_timer_config(&timer_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed");
        return ret;
    }

    ledc_channel_config_t ch_cfg = {
        .gpio_num   = stp_pin,
        .speed_mode = speed_mode,
        .channel    = LEDC_CHANNEL_1,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_1,
        .duty       = 0,
        .hpoint     = 0
    };
    ret = ledc_channel_config(&ch_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel config failed");
        return ret;
    }

    default_duration_ms = config->duration_ms; // 保存默认运行时间
    current_state = STEPPER_STATE_STOPPED;
    ESP_LOGI(TAG, "Stepper initialized successfully");
    return ESP_OK;
}
esp_err_t stepper_start(void) {
    if (stp_pin == -1) {
        ESP_LOGE(TAG, "Stepper not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (current_state == STEPPER_STATE_RUNNING) {
        ESP_LOGW(TAG, "Motor is already running");
        return ESP_OK;
    }
    if (default_duration_ms == 0) {
        ESP_LOGE(TAG, "Default duration not set in init config");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting motor with default settings (%lu ms)...", default_duration_ms);

    // 设置方向：正转（高电平）
    gpio_set_level(dir_pin, 1);

    // 使能电机（高电平有效）
    gpio_set_level(en_pin, 0);

    // 输出 50% 占空比脉冲
    ledc_set_duty(speed_mode, LEDC_CHANNEL_1, 512);
    ledc_update_duty(speed_mode, LEDC_CHANNEL_1);

    // 启动运行时长定时器
    timer_stop_and_reset();
    stop_timer = xTimerCreate("stepper_stop_timer",
                              pdMS_TO_TICKS(default_duration_ms),
                              pdFALSE,
                              (void *)0,
                              stepper_stop_timer_callback);
    if (stop_timer == NULL) {
        return ESP_ERR_NO_MEM;
    }
    xTimerStart(stop_timer, 0);

    current_state = STEPPER_STATE_RUNNING;
    return ESP_OK;
}

esp_err_t stepper_move_for(uint32_t duration_ms, uint32_t freq_hz, int direction) {
    if (stp_pin == -1) {
        ESP_LOGE(TAG, "Stepper not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 如果正在运行，先停止
    if (current_state == STEPPER_STATE_RUNNING) {
        ESP_LOGI(TAG, "Motor is running, stopping first...");
        stepper_stop();
        vTaskDelay(pdMS_TO_TICKS(10)); // 等待停止完成
    }

    ESP_LOGI(TAG, "Move for %lu ms, freq=%lu Hz, dir=%d", duration_ms, freq_hz, direction);

    // 重新设置 LEDC 频率
    esp_err_t ret = ledc_set_freq(speed_mode, LEDC_TIMER_1, freq_hz);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set frequency");
        return ret;
    }

    // 设置方向引脚
    gpio_set_level(dir_pin, direction ? 1 : 0);

    // 使能电机（低电平有效）
    gpio_set_level(en_pin, 0);

    // 输出脉冲（50% 占空比）
    ledc_set_duty(speed_mode, LEDC_CHANNEL_1, 512);
    ledc_update_duty(speed_mode, LEDC_CHANNEL_1);

    // 启动运行时长定时器
    timer_stop_and_reset();
    stop_timer = xTimerCreate("stepper_stop_timer",
                              pdMS_TO_TICKS(duration_ms),
                              pdFALSE,
                              (void *)0,
                              stepper_stop_timer_callback);
    if (stop_timer == NULL) {
        return ESP_ERR_NO_MEM;
    }
    xTimerStart(stop_timer, 0);

    current_state = STEPPER_STATE_RUNNING;
    return ESP_OK;
}

esp_err_t stepper_stop(void) {
    if (stp_pin == -1) {
        ESP_LOGE(TAG, "Stepper not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Stopping motor...");

    // 失能电机（低电平有效时拉高）
    gpio_set_level(en_pin, 1);

    // 关闭脉冲
    ledc_set_duty(speed_mode, LEDC_CHANNEL_1, 0);
    ledc_update_duty(speed_mode, LEDC_CHANNEL_1);

    // 停止并删除定时器
    timer_stop_and_reset();

    current_state = STEPPER_STATE_STOPPED;
    ESP_LOGI(TAG, "Motor stopped");
    return ESP_OK;
}

stepper_state_t stepper_get_state(void) {
    return current_state;
}