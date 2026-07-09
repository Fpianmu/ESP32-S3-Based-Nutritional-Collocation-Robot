#include "spi.h"

static bool spi_initialized = false; // 防止重复初始化

void bsp_spi2_init(void)
{
    if (spi_initialized) return;

    spi_bus_config_t spi_bus_cfg = {
        .mosi_io_num = LCD_MOSI,
        .sclk_io_num = LCD_SCK,
        .miso_io_num = -1,        // ST7789不需要MISO
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 50 * 2, // 一行240像素×50行×2字节
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST, &spi_bus_cfg, SPI_DMA_CH_AUTO));
    spi_initialized = true;
    ESP_LOGI("SPI", "SPI2总线初始化完成");
}