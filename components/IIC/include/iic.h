/** @file iic.h — I2C1 总线引脚定义 (触摸专用) */
#ifndef __IIC_H
#define __IIC_H

#include "driver/i2c.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"


#ifdef __cplusplus
extern "C" {
#endif

// 电容触摸 CTP 接口引脚（I2C 通信 + 控制）

#define CTP_SCL         GPIO_NUM_38    // 电容触摸 I2C 时钟引脚
#define CTP_SDA         GPIO_NUM_47    // 电容触摸 I2C 数据引脚
#define TOUCH_I2C_NUM   I2C_NUM_1      // 触摸专用I2C总线（和其他设备不冲突）

// 触摸I2C总线句柄（全局，供touch.c直接调用）
extern i2c_master_bus_handle_t touch_i2c_bus;

/**
 * @brief  初始化触摸专用I2C总线
 * @note   带防重复初始化判断，只初始化一次，可被复用
 */
void iic_touch_bus_init(void);

#ifdef __cplusplus
}
#endif

#endif
