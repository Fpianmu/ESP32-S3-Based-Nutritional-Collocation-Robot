#include "esc_controller.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

#define ESC_FREQ        50
#define ESC_RES         LEDC_TIMER_14_BIT
#define ESC_MAX_DUTY    16383       // 2^14 - 1
#define DUTY_MIN_US     1000
#define DUTY_MAX_US     2000
#define DUTY_MIN        ((DUTY_MIN_US * ESC_MAX_DUTY) / 20000)   // ≈ 820
#define DUTY_MAX        ((DUTY_MAX_US * ESC_MAX_DUTY) / 20000)   // ≈ 1638

static const char *TAG = "esc_controller";
static bool timer_configured = false;

esp_err_t esc_init(esc_handle_t *esc, int gpio_num, ledc_channel_t channel)
{
    if (esc == NULL) return ESP_ERR_INVALID_ARG;

    esc->gpio_num = gpio_num;
    esc->channel = channel;
    esc->calibrated = false;

    // 1. 配置LEDC定时器（只做一次）
    if (!timer_configured) {
        ledc_timer_config_t timer_cfg = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .timer_num = LEDC_TIMER_3,
            .duty_resolution = ESC_RES,
            .freq_hz = ESC_FREQ,
            .clk_cfg = LEDC_AUTO_CLK
        };
        esp_err_t ret = ledc_timer_config(&timer_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Timer config failed: %s", esp_err_to_name(ret));
            return ret;
        }
        timer_configured = true;
        ESP_LOGI(TAG, "LEDC timer 3 configured (50Hz, 14bit)");
    }

    // 2. 配置通道
    ledc_channel_config_t ch_cfg = {
        .gpio_num = gpio_num,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = channel,
        .timer_sel = LEDC_TIMER_3,
        .duty = DUTY_MIN,
        .hpoint = 0
    };
    esp_err_t ret = ledc_channel_config(&ch_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Channel %d config failed: %s", channel, esp_err_to_name(ret));
        return ret;
    }

    // 3. 油门行程校准
    ESP_LOGI(TAG, "Starting calibration for ESC on GPIO %d (channel %d)...", gpio_num, channel);
    // 发送最大油门
    ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, DUTY_MAX);
    if (ret != ESP_OK) return ret;
    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
    if (ret != ESP_OK) return ret;
    ESP_LOGI(TAG, "Sending MAX throttle (100%%), please power ESC now.");
    vTaskDelay(pdMS_TO_TICKS(3000));

    // 发送最小油门
    ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, DUTY_MIN);
    if (ret != ESP_OK) return ret;
    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
    if (ret != ESP_OK) return ret;
    ESP_LOGI(TAG, "Sending MIN throttle (0%%), waiting for confirmation beeps.");
    vTaskDelay(pdMS_TO_TICKS(3000));

    esc->calibrated = true;
    ESP_LOGI(TAG, "ESC on GPIO %d calibrated successfully.", gpio_num);
    return ESP_OK;
}

esp_err_t esc_set_throttle(esc_handle_t *esc, float throttle)
{
    if (esc == NULL || !esc->calibrated) return ESP_ERR_INVALID_STATE;
    if (throttle < 0.0f) throttle = 0.0f;
    if (throttle > 100.0f) throttle = 100.0f;

    uint32_t duty = DUTY_MIN + (uint32_t)((DUTY_MAX - DUTY_MIN) * throttle / 100.0f);
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, esc->channel, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "set duty failed");
        return ret;
    }
    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, esc->channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "update duty failed");
        return ret;
    }
    return ESP_OK;
}