#ifndef LEDC_PWM_H
#define LEDC_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 两路 PWM 开关控制（适配 DRV8870 电机驱动模块）
 *
 *        通道0: GPIO 12, 开启时 50% 占空比
 *        通道1: GPIO 13, 始终 0% 占空比
 *        PWM 频率: 50kHz (DRV8870 最佳工作频率)
 *
 * @param state  0 = 关闭两路输出 (占空比均为 0%)
 *               1 = 开启 (通道0=50%, 通道1=0%)
 */
void ledc_pwm_set_state(int state);

#ifdef __cplusplus
}
#endif

#endif
