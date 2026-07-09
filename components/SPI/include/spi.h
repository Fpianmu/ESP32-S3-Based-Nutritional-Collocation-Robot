/** @file spi.h — SPI2 总线引脚定义 (LCD 专用) */
#ifndef __SPI_H
#define __SPI_H

#include <string.h>
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// 屏幕 SPI 接口引脚
#define LCD_CS      GPIO_NUM_1  // SPI 片选引脚，低电平选中屏幕
#define LCD_RST     GPIO_NUM_2  // 屏幕硬件复位引脚，低电平复位
#define LCD_DC      GPIO_NUM_42  // 数据/命令切换引脚，高=数据，低=命令
#define LCD_MOSI    GPIO_NUM_41  // SPI 主机发从机收数据引脚（SDI）
#define LCD_SCK     GPIO_NUM_40  // SPI 时钟信号引脚
#define LCD_BL      GPIO_NUM_39  // 屏幕背光控制引脚，高电平点亮背光

#define SPI_HOST    SPI2_HOST    // 使用SPI2控制器

/**
 * @brief 初始化SPI2总线（LCD专用）
 * @note 只初始化一次，支持其他外设复用
 */
void bsp_spi2_init(void);

#ifdef __cplusplus
}
#endif

#endif // __SPI_H
