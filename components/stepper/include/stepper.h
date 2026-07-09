#ifndef STEPPER_H
#define STEPPER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 步进电机运行状态
 */
typedef enum {
    STEPPER_STATE_STOPPED = 0,  // 停止
    STEPPER_STATE_RUNNING,      // 运行中
} stepper_state_t;

/**
 * @brief 步进电机配置结构体（初始化时使用）
 */
typedef struct {
    int en_pin;          // EN 引脚，高电平有效使能
    int dir_pin;         // DIR 方向引脚
    int stp_pin;         // STP 脉冲引脚
    uint32_t freq_hz;    // 默认脉冲频率（Hz）
    uint32_t duration_ms; // 默认运行时间（ms），start() 函数使用
} stepper_config_t;

/**
 * @brief 初始化步进电机组件
 * @param config 指向配置结构体的指针
 * @return ESP_OK 成功，否则失败
 */
esp_err_t stepper_init(const stepper_config_t *config);

/**
 * @brief 按照初始化时设置的时间和频率启动电机（方向固定为正转）
 * @return ESP_OK 成功，否则失败
 */
esp_err_t stepper_start(void);

/**
 * @brief 动态启动电机，运行指定时间后自动停止
 * @param duration_ms 运行时间（毫秒）
 * @param freq_hz     脉冲频率（Hz），决定电机转速
 * @param direction   方向：1 为正转（DIR高电平），0 为反转（DIR低电平）
 * @return ESP_OK 成功，否则失败
 */
esp_err_t stepper_move_for(uint32_t duration_ms, uint32_t freq_hz, int direction);

/**
 * @brief 立即停止电机（失能并关闭脉冲）
 * @return ESP_OK 成功，否则失败
 */
esp_err_t stepper_stop(void);

/**
 * @brief 获取电机当前状态
 * @return 当前状态，见 stepper_state_t 枚举
 */
stepper_state_t stepper_get_state(void);

#ifdef __cplusplus
}
#endif

#endif