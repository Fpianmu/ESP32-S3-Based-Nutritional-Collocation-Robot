/**
 * @file    lvgl_ui.h
 * @brief   LVGL 初始化 — 注册 LCD 显示 + 触摸输入
 */

#ifndef __LVGL_UI_H__
#define __LVGL_UI_H__

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LVGL 内核 + 注册 LCD + 触摸 (不含测试界面)
 *
 * 内部调用:
 *   lvgl_port_init() → lvgl_port_add_disp() → lvgl_port_add_touch()
 */
void lvgl_ui_init(void);

/**
 * @brief 创建触摸坐标测试界面 (调试用)
 * @note  仅用于验证触摸驱动是否正常, 正式使用 EEZ UI 时不调用此函数
 */
void lvgl_ui_test(void);

/**
 * @brief 纯硬件初始化 (不含任何界面, 配合 EEZ UI 使用)
 *        = lvgl_ui_init() 去掉测试界面部分
 */
void display_init(void);

#ifdef __cplusplus
}
#endif

#endif
