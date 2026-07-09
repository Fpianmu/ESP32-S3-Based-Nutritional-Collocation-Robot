/**
 * @file    touch.h
 * @brief   FT6336U 电容触摸驱动 — I2C 接口, 兼容 FT5x06 协议
 */
#ifndef __FT6336U_TOUCH_H
#define __FT6336U_TOUCH_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_ft5x06.h"
#include "iic.h"

#ifdef __cplusplus
extern "C" {
#endif

// ===================== 触摸引脚 =====================
#define CTP_INT         GPIO_NUM_21    // 中断引脚
#define CTP_RST         GPIO_NUM_48    // 复位引脚

// 屏幕分辨率 240×320 竖屏
#define LCD_WIDTH       240
#define LCD_HEIGHT      320

// 触摸点结构体
typedef struct {
    uint16_t x;
    uint16_t y;
    bool pressed;
} touch_point_t;

// 触摸设备全局句柄
extern esp_lcd_touch_handle_t ft6336u_touch_handle;

/**
 * @brief  初始化FT6336U触摸芯片（适配esp_lcd_touch_ft5x06 v1.1.0）
 */
void ft6336u_touch_init(void);

/**
 * @brief  读取触摸点坐标
 * @param  tp: 触摸点结构体指针
 */
void ft6336u_read_touch_point(touch_point_t *tp);

#ifdef __cplusplus
}
#endif

#endif // __FT6336U_TOUCH_H
