#include "lcd.h"
#include "spi.h"
#include "esp_lcd_panel_io.h"
//#include "esp_lcd_panel_st7789.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static esp_lcd_panel_handle_t lcd_panel = NULL;
static esp_lcd_panel_io_handle_t lcd_io_handle = NULL;

void bsp_lcd_init(void)
{
    // 1. 先初始化SPI总线（只初始化一次）
    bsp_spi2_init();

    // 2. 配置SPI通信接口
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = LCD_CS,
        .dc_gpio_num = LCD_DC,
        .spi_mode = 0,
        .pclk_hz = 20 * 1000 * 1000, // 20MHz稳定时钟
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI_HOST, &io_cfg, &lcd_io_handle));

    // 3. 配置ST7789V面板
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(lcd_io_handle, &panel_cfg, &lcd_panel));

    // 4. 复位并初始化屏幕
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_panel));
    vTaskDelay(pdMS_TO_TICKS(200)); // 等待屏幕稳定
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_panel));
    esp_lcd_panel_mirror(lcd_panel, true, true);
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd_panel, true));

    // 5. 开启背光
    gpio_set_direction(LCD_BL, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_BL, 1);

    ESP_LOGI("LCD", "ST7789V屏幕初始化完成");
}

void bsp_lcd_fill_color(uint16_t color)
{
    if (!lcd_panel) return;

    static uint16_t line_buf[LCD_HOR_RES];
    for (int i = 0; i < LCD_HOR_RES; i++) {
        line_buf[i] = color;
    }
    for (int y = 0; y < LCD_VER_RES; y++) {
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
            lcd_panel,
            0, y,
            LCD_HOR_RES, y + 1,
            line_buf
        ));
    }
}

void bsp_lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (!lcd_panel) return;

    // 裁剪到屏幕边界
    if (x >= LCD_HOR_RES || y >= LCD_VER_RES) return;
    if (x + w > LCD_HOR_RES) w = LCD_HOR_RES - x;
    if (y + h > LCD_VER_RES) h = LCD_VER_RES - y;

    static uint16_t line_buf[LCD_HOR_RES];
    for (int i = 0; i < w; i++) {
        line_buf[i] = color;
    }
    for (int row = 0; row < h; row++) {
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
            lcd_panel,
            x, y + row,
            x + w, y + row + 1,
            line_buf
        ));
    }
}

esp_lcd_panel_handle_t bsp_lcd_get_panel_handle(void)
{
    return lcd_panel;
}

esp_lcd_panel_io_handle_t bsp_lcd_get_io_handle(void)
{
    return lcd_io_handle;
}