#include "ledc_pwm.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "ledc_pwm";

/* servo 用 50Hz/14bit, stepper 用 1kHz/10bit, 我们这里用 5kHz/10bit */
#define PWM_FREQ_HZ       5000
#define PWM_RES           LEDC_TIMER_10_BIT
#define DUTY_MAX          ((1 << LEDC_TIMER_10_BIT) - 1)  /* 1023 */
#define DUTY_50_PERCENT   (DUTY_MAX / 2)                  /* 512  */
#define DUTY_OFF          0

#define CH0_GPIO          12
#define CH1_GPIO          13

static ledc_mode_t    s_mode   = LEDC_LOW_SPEED_MODE;
static ledc_channel_t s_ch0    = LEDC_CHANNEL_3;  /* CH0-2 被 servo/stepper 占用 */
static ledc_channel_t s_ch1    = LEDC_CHANNEL_4;
static bool           s_inited = false;

void ledc_pwm_set_state(int state)
{
    /* ---- 初次调用时一次性初始化 ---- */
    if (!s_inited) {
        /* 1. 配置定时器（和 servo/stepper 完全一样的调用方式）*/
        ledc_timer_config_t timer_cfg = {
            .speed_mode      = s_mode,
            .timer_num       = LEDC_TIMER_0,
            .duty_resolution = PWM_RES,
            .freq_hz         = PWM_FREQ_HZ,
            .clk_cfg         = LEDC_AUTO_CLK,
        };
        esp_err_t ret = ledc_timer_config(&timer_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ledc_timer_config failed: %d", ret);
            return;
        }

        /* 2. 通道0 */
        ledc_channel_config_t ch0 = {
            .gpio_num   = CH0_GPIO,
            .speed_mode = s_mode,
            .channel    = s_ch0,
            .timer_sel  = LEDC_TIMER_0,
            .duty       = DUTY_OFF,
            .hpoint     = 0,
        };
        ret = ledc_channel_config(&ch0);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ledc_channel_config CH0 failed: %d", ret);
            return;
        }

        /* 3. 通道1 */
        ledc_channel_config_t ch1 = {
            .gpio_num   = CH1_GPIO,
            .speed_mode = s_mode,
            .channel    = s_ch1,
            .timer_sel  = LEDC_TIMER_0,
            .duty       = DUTY_OFF,
            .hpoint     = 0,
        };
        ret = ledc_channel_config(&ch1);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ledc_channel_config CH1 failed: %d", ret);
            return;
        }

        s_inited = true;
        ESP_LOGI(TAG, "INIT OK: GPIO%d/CH0 GPIO%d/CH1 Timer0 %dHz %dbit",
                 CH0_GPIO, CH1_GPIO, PWM_FREQ_HZ, LEDC_TIMER_10_BIT);
    }

    /* ---- 每次调用都执行：更新占空比 ---- */
    uint32_t duty = (state == 0) ? DUTY_OFF : DUTY_50_PERCENT;

    esp_err_t r1 = ledc_set_duty(s_mode, s_ch0, duty);
    esp_err_t r2 = ledc_update_duty(s_mode, s_ch0);

    ESP_LOGW(TAG, ">>> set_state(%d) set=%d upd=%d duty=%lu <<<",
             state, (int)r1, (int)r2, (unsigned long)duty);
}
