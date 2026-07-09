#include "iic.h"
#include "esp_log.h"

static const char *TAG = "IIC";

// 全局句柄，和iic.h里的extern对应
i2c_master_bus_handle_t touch_i2c_bus = NULL;

void iic_touch_bus_init(void)
{
    // 防重复初始化：如果总线已经创建，直接跳过
    if (touch_i2c_bus != NULL) {
        ESP_LOGW(TAG, "触摸I2C总线已初始化，跳过");
        return;
    }

    // 配置触摸I2C总线（标准ESP-IDF v5.x写法）
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = TOUCH_I2C_NUM,
        .sda_io_num = CTP_SDA,
        .scl_io_num = CTP_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true, // 开启内部上拉，不用外接电阻
    };

    // 创建I2C主总线
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &touch_i2c_bus));
    ESP_LOGI(TAG, "✅ 触摸专用I2C总线初始化完成（总线号：%d）", TOUCH_I2C_NUM);
}