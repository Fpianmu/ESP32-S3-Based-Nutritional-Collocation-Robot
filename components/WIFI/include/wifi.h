/**
 * @file    wifi.h
 * @brief   WiFi AP 模式 + TCP 命令行服务器
 *
 * @section wifi_ap WiFi 热点
 *   启动一个 WPA2-PSK 加密的 WiFi 热点供手机/电脑连接.
 *   同时运行 DHCP 服务器, 自动分配 IP (默认网关 192.168.4.1).
 *
 * @section tcp_server TCP 调试服务器 (端口 8080)
 *   接收纯文本指令, 格式与 UART 串口完全一致.
 *   每条指令以回车结束, 大小写不敏感, 支持以下命令:
 *
 *   | 指令               | 说明                        | 示例          |
 *   |--------------------|-----------------------------|---------------|
 *   | `STOP`             | 紧急停止所有电机             | `STOP`        |
 *   | `H`                | 协同控制 (默认参数)          | `H`           |
 *   | `M A <angle>`      | 步进电机正转角度 (度)        | `M A 90`      |
 *   | `M B <angle>`      | 步进电机反转角度 (度)        | `M B 45`      |
 *   | `M F <turns>`      | 步进电机正转圈数             | `M F 2.5`     |
 *   | `M R <turns>`      | 步进电机反转圈数             | `M R 1.0`     |
 *   | `M START`          | 步进自动往返模式             | `M START`     |
 *   | `M STOP`           | 停止自动往返                 | `M STOP`      |
 *   | `SS A <angle>`     | 转向舵机设置角度 (0~180)     | `SS A 90`     |
 *   | `SP A <angle>`     | 推动舵机设置角度 (0~180)     | `SP A 45`     |
 *   | `P <seconds>`      | 水泵运行秒数                 | `P 10`        |
 *   | `D <seconds>`      | 面粉电机运行秒数             | `D 15`        |
 *   | `G <seconds>`      | 研磨电机运行秒数             | `G 20`        |
 *   | `MI <minutes>`     | 搅拌电机运行分钟数           | `MI 5`        |
 *
 * @section tcp_usage 使用方式
 *   @code
 *   // 电脑终端
 *   nc 192.168.4.1 8080
 *
 *   // Android: TCP Client 等 App
 *   // iOS:     TCP Telnet, Mocha Telnet 等 App
 *   // 输入指令后按回车发送
 *   @endcode
 *
 * @section wifi_flow 运行流程
 *   1. main 中调用 wifi_init_softap() 启动热点
 *   2. main 中创建 tcp_server_task 开始监听
 *   3. 客户端连接 → 接收指令 → wifi_process_cmd() 解析
 *   4. 定时指令创建独立 FreeRTOS 任务异步执行
 *   5. STOP 指令销毁任务 + 调用 motor_emergency_stop()
 */

#ifndef __WIFI_H__
#define __WIFI_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "lwip/err.h"
#include "lwip/sockets.h"

/*====================================================================
 *  WiFi AP 配置
 *====================================================================*/

/** WiFi 热点名称 (SSID) */
#define WIFI_SSID      "dough_mixer"

/** WiFi 热点密码 (WPA2-PSK 加密, 至少 8 位) */
#define WIFI_PASS      "12345678"

/** WiFi 信道 (1~13, 选 1 兼容性最好) */
#define WIFI_CHANNEL   1

/** 最大同时连接的客户端数量 */
#define MAX_STA_CONN   4

/*====================================================================
 *  TCP 服务器配置
 *====================================================================*/

/** TCP 调试端口 (手机/电脑 TCP 客户端连接此端口发送指令) */
#define TCP_PORT       8080

/*====================================================================
 *  API
 *====================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化并启动 WiFi AP 模式
 *
 * 内部调用链:
 *   1. esp_netif_init()          — 初始化 TCP/IP 协议栈
 *   2. esp_event_loop_create()   — 创建系统事件循环 (WiFi 事件依赖)
 *   3. esp_netif_create_default_wifi_ap() — 创建默认 AP 网络接口
 *   4. esp_wifi_init()           — 初始化 WiFi 驱动
 *   5. esp_wifi_set_mode(WIFI_MODE_AP)    — 设为 AP 模式
 *   6. esp_wifi_set_config()     — 配置 SSID/密码/信道/加密
 *   7. esp_wifi_start()          — 启动 WiFi
 *
 * @note 如果 main 中已调用 nvs_flash_init + esp_event_loop_create_default,
 *       本函数内部调用会安全地重复初始化 (ESP-IDF 内部做幂等处理).
 *       热点启动后可通过 192.168.4.1 访问.
 */
void wifi_init_softap(void);

/**
 * @brief TCP 服务器任务 (FreeRTOS, 永不退出)
 *
 * 工作流程:
 *   1. 创建 TCP socket, 绑定 0.0.0.0:8080
 *   2. listen + accept 循环: 每次接受一个客户端连接
 *   3. recv 循环: 接收文本行, 调用 wifi_process_cmd() 处理
 *   4. 客户端断开 → 关闭 socket → 回到 accept 等待下一个连接
 *
 * @param pvParameters 地址族, 传入 AF_INET (IPv4)
 *
 * @note 任务栈大小 4096 字节, 优先级 5.
 *       每次只能处理一个 TCP 连接 (单客户端模式),
 *       当前客户端断开后才会接受下一个.
 */
void tcp_server_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* __WIFI_H__ */
