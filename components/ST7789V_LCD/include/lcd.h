/**
 * @file    lcd.h
 * @brief   ST7789V LCD 面板驱动 — SPI 接口, 240×320 竖屏
 */
#ifndef __ST7789V_LCD_H
#define __ST7789V_LCD_H

#include <stdint.h>
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_lcd_panel_io.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// 屏幕分辨率 240×320 竖屏
#define LCD_HOR_RES 240
#define LCD_VER_RES 320

/**
 * @brief 初始化ST7789V屏幕
 * @note 自动调用SPI初始化，无需手动调用
 */
void bsp_lcd_init(void);

/**
 * @brief 全屏填充指定RGB565颜色
 * @param color RGB565颜色值（如0xF800=红, 0x07E0=绿, 0x001F=蓝）
 */
void bsp_lcd_fill_color(uint16_t color);

/**
 * @brief 填充指定矩形区域
 * @param x 起始X坐标
 * @param y 起始Y坐标
 * @param w 矩形宽度(像素)
 * @param h 矩形高度(像素)
 * @param color RGB565颜色值
 */
void bsp_lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief 获取LCD面板句柄（供LVGL等框架使用）
 */
esp_lcd_panel_handle_t bsp_lcd_get_panel_handle(void);

/**
 * @brief 获取LCD面板IO句柄（供LVGL等框架使用）
 */
esp_lcd_panel_io_handle_t bsp_lcd_get_io_handle(void);

#ifdef __cplusplus
}
#endif

#endif // __ST7789V_LCD_H