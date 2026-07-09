#include "touch.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "driver/i2c_master.h"

static const char *TAG = "TOUCH";

esp_lcd_touch_handle_t ft6336u_touch_handle = NULL;

void ft6336u_touch_init(void)
{
    iic_touch_bus_init();

    // ---- 第1步：I2C探测 ----
    esp_err_t probe_ret = i2c_master_probe(touch_i2c_bus, ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS, 200);
    if (probe_ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ I2C探测失败 addr=0x%02X err=%s",
                 ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS, esp_err_to_name(probe_ret));
        ESP_LOGE(TAG, "   请检查: SDA(GPIO%2d) SCL(GPIO%2d) 接线和触摸芯片供电", CTP_SDA, CTP_SCL);
        return;
    }
    ESP_LOGI(TAG, "✅ I2C探测成功 0x%02X", ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS);

    // ---- 第2步：创建面板IO ----
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    io_config.scl_speed_hz = 100000;  // 宏默认未设时钟频率，必须手动指定
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_err_t ret = esp_lcd_new_panel_io_i2c(touch_i2c_bus, &io_config, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ 创建面板IO失败: %s", esp_err_to_name(ret));
        return;
    }

    // ---- 第3步：初始化芯片 ----
    esp_lcd_touch_config_t touch_cfg = {
        .x_max = LCD_WIDTH,
        .y_max = LCD_HEIGHT,
        .rst_gpio_num = CTP_RST,
        .int_gpio_num = CTP_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    ret = esp_lcd_touch_new_i2c_ft5x06(io_handle, &touch_cfg, &ft6336u_touch_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ FT6336U 初始化失败: %s", esp_err_to_name(ret));
        esp_lcd_panel_io_del(io_handle);
        return;
    }

    ESP_LOGI(TAG, "✅ FT6336U 触摸初始化成功");
}

void ft6336u_read_touch_point(touch_point_t *tp)
{
    if (!tp) return;
    tp->pressed = false;

    if (!ft6336u_touch_handle) return;

    esp_err_t ret = esp_lcd_touch_read_data(ft6336u_touch_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取数据失败: %s", esp_err_to_name(ret));
        return;
    }

    esp_lcd_touch_point_data_t point_data[1];
    uint8_t touch_cnt = 0;
    ret = esp_lcd_touch_get_data(ft6336u_touch_handle, point_data, &touch_cnt, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取坐标失败: %s", esp_err_to_name(ret));
        return;
    }

    if (touch_cnt > 0) {
        tp->x = point_data[0].x;
        tp->y = point_data[0].y;
        tp->pressed = true;
    }
}
