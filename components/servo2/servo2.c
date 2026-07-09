#include "servo2.h"
#include "driver/ledc.h"
#include "esp_log.h"

#define SERVO_PWM_FREQ     50          // 50 Hz
#define SERVO_PWM_RES      LEDC_TIMER_14_BIT // 14位分辨率
#define SERVO_MAX_DUTY     16383       // 2^16 - 1

// 根据 0.5ms - 2.5ms 脉宽计算出的占空比边界
#define DUTY_MIN_US        500         // 0 度对应的高电平微秒数
#define DUTY_MAX_US        2500        // 180 度对应的高电平微秒数
#define DUTY_MIN           ((DUTY_MIN_US * SERVO_MAX_DUTY) / 20000)  // ≈ 410
#define DUTY_MAX           ((DUTY_MAX_US * SERVO_MAX_DUTY) / 20000)  // ≈ 2048

static const char *TAG = "servo2";
static ledc_mode_t speed_mode = LEDC_LOW_SPEED_MODE;
static ledc_channel_t channel = LEDC_CHANNEL_2;

esp_err_t servo2_init(int gpio_num)
{
    // 1. 配置 LEDC 定时器
    ledc_timer_config_t timer_cfg = {
        .speed_mode = speed_mode,
        .timer_num = LEDC_TIMER_2,
        .duty_resolution = SERVO_PWM_RES,
        .freq_hz = SERVO_PWM_FREQ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    esp_err_t ret = ledc_timer_config(&timer_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Timer config failed");
        return ret;
    }

    // 2. 配置 LEDC 通道并绑定 GPIO
    ledc_channel_config_t ch_cfg = {
        .gpio_num = gpio_num,
        .speed_mode = speed_mode,
        .channel = channel,
        .timer_sel = LEDC_TIMER_2,
        .duty = 0,              // 初始关闭
        .hpoint = 0
    };
    ret = ledc_channel_config(&ch_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Channel config failed");
        return ret;
    }

    ESP_LOGI(TAG, "Servo initialized on GPIO %d", gpio_num);
    return ESP_OK;
}

esp_err_t servo2_set_angle(int angle)
{
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    // 将角度线性映射到占空比
    uint32_t duty = DUTY_MIN + (DUTY_MAX - DUTY_MIN) * angle / 180;

    esp_err_t ret = ledc_set_duty(speed_mode, channel, duty);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = ledc_update_duty(speed_mode, channel);
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "Angle set to %d°, duty = %lu", angle, duty);
    return ESP_OK;
}