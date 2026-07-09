#include "ctl_mutex.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "ctl_mutex";

static SemaphoreHandle_t s_mutex = NULL;
static ctl_source_t      s_owner = CTL_NONE;

bool ctl_try_acquire(ctl_source_t src)
{
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
    }

    /* 非阻塞获取互斥锁 */
    if (xSemaphoreTake(s_mutex, 0) != pdTRUE) {
        ESP_LOGW(TAG, "acquire(%d) failed: locked by %d", (int)src, (int)s_owner);
        return false;
    }

    /* 如果已有其他源占用，释放锁并返回失败 */
    if (s_owner != CTL_NONE && s_owner != src) {
        xSemaphoreGive(s_mutex);
        ESP_LOGW(TAG, "acquire(%d) failed: owner=%d", (int)src, (int)s_owner);
        return false;
    }

    s_owner = src;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "acquire(%d) OK", (int)src);
    return true;
}

void ctl_release(ctl_source_t src)
{
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (s_owner == src) {
        s_owner = CTL_NONE;
        ESP_LOGI(TAG, "release(%d) OK", (int)src);
    } else {
        ESP_LOGW(TAG, "release(%d) mismatch: owner=%d", (int)src, (int)s_owner);
    }

    xSemaphoreGive(s_mutex);
}

ctl_source_t ctl_get_owner(void)
{
    if (s_mutex == NULL) {
        return CTL_NONE;
    }

    ctl_source_t owner;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    owner = s_owner;
    xSemaphoreGive(s_mutex);
    return owner;
}
