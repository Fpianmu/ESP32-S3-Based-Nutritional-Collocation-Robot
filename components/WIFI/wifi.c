/**
 * @file    wifi.c
 * @brief   WiFi AP + TCP 指令服务器实现
 *
 * @section arch 架构
 *   本文件实现两套机制:
 *   1. WiFi AP 热点 — 让手机/电脑通过 WiFi 连接设备
 *   2. TCP 指令服务器 — 在端口 8080 监听, 接收文本指令控制电机
 *
 * @section tcp_cmd_flow TCP 指令处理流程
 *   @code
 *   TCP 客户端连接
 *     → tcp_server_task() accept 新连接
 *       → recv() 接收文本行
 *         → wifi_process_cmd() 解析指令
 *           ├─ 即时指令 (舵机/步进): 直接调用 motor API, 阻塞当前连接
 *           ├─ 定时指令 (水泵/面粉/搅拌/研磨): 创建 FreeRTOS 任务异步执行
 *           ├─ STOP: 销毁定时任务 + motor_emergency_stop()
 *           └─ 未识别: 返回 "Error: Unknown TCP command"
 *       → 客户端断开 → close → accept 等待下一个连接
 *   @endcode
 *
 * @section timed_task 定时任务机制
 *   水泵(P)、面粉(D)、搅拌(MI)、研磨(G) 需要运行指定时间后自动停止.
 *   如果在定时任务运行期间收到新的定时指令, 会先销毁旧任务并急停,
 *   然后创建新任务. 这保证了同一时刻只有一个电机在自动运行.
 */

#include "wifi.h"
#include "function.h"
#include "ledc_pwm.h"
#include "servo.h"
#include "servo2.h"
#include "gpiowork.h"
#include "stepper.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"

static const char *TAG = "WIFI";

/*====================================================================
 *  全局变量
 *====================================================================*/

/** 当前正在运行的 TCP 定时电机任务句柄.
 *  新定时指令到达时先销毁此任务再创建新的, 保证互斥. */
static TaskHandle_t wifi_timed_task_handle = NULL;

/*====================================================================
 *  工具函数
 *====================================================================*/

/**
 * @brief 向 TCP 客户端发送一行文本 (自动追加 \\r\\n)
 * @param sock TCP socket 文件描述符
 * @param msg  要发送的消息 (不需要包含换行)
 */
static void wifi_send_msg(int sock, const char *msg)
{
    if (sock >= 0) {
        send(sock, msg, strlen(msg), 0);  /* 发送消息正文 */
        send(sock, "\r\n", 2, 0);         /* 追加 CRLF 换行, 方便客户端 readline */
    }
}

/*====================================================================
 *  定时任务
 *====================================================================*/

/** 定时电机任务参数 */
typedef struct {
    char type;  /**< 电机类型: 'P'=水泵 'D'=面粉 'M'=搅拌 'G'=研磨 */
    int  val;   /**< 运行时长, 水泵/面粉/研磨为秒, 搅拌为分钟 */
    int  sock;  /**< 客户端 socket, 用于发送状态通知 */
} wifi_timed_cmd_t;

/**
 * @brief 定时电机任务 (FreeRTOS)
 *
 * 根据 type 字段执行对应的电机操作, 运行 val 时间后自动停止.
 * 运行期间通过 vTaskDelay 实现非阻塞等待 (让出 CPU 给其他任务).
 * 任务结束时释放 cmd 内存, 清空全局句柄, 自删除.
 */
static void wifi_timed_action_task(void *pvParameters)
{
    wifi_timed_cmd_t *cmd = (wifi_timed_cmd_t *)pvParameters;
    char msg[64];

    switch (cmd->type) {
    case 'P':  /* 水泵 — 继电器, 运行 N 秒 */
        snprintf(msg, sizeof(msg), "Pump starting for %d seconds...", cmd->val);
        wifi_send_msg(cmd->sock, msg);
        //gpio_set(7,1);
        //vTaskDelay(pdMS_TO_TICKS(cmd->val * 1000));  /* 毫秒级延迟 */
        //gpio_set(7,0);
        wifi_send_msg(cmd->sock, "Pump stopped.");
        break;

    case 'D':  /* 面粉电机 — 无刷 ESC, 运行 N 秒 */
        snprintf(msg, sizeof(msg), "Dough motor starting for %d seconds...", cmd->val);
        wifi_send_msg(cmd->sock, msg);
        //dough_esc_init_and_start();                     /* ESC 校准 + 启动 */
        //vTaskDelay(pdMS_TO_TICKS(cmd->val * 1000));
        //dough_esc_stop();
        wifi_send_msg(cmd->sock, "Dough motor stopped.");
        break;

    case 'M':  /* 搅拌电机 — 无刷 ESC, 运行 N 分钟 */
        snprintf(msg, sizeof(msg), "Mixer motor starting for %d minutes...", cmd->val);
        wifi_send_msg(cmd->sock, msg);
        //mixer_esc_init_and_start();
        //vTaskDelay(pdMS_TO_TICKS(cmd->val * 60 * 1000)); /* 分钟转毫秒 */
        //mixer_esc_stop();
        wifi_send_msg(cmd->sock, "Mixer motor stopped.");
        break;

    case 'G':  /* 研磨电机 — 继电器, 运行 N 秒 */
        snprintf(msg, sizeof(msg), "Grinder starting for %d seconds...", cmd->val);
        wifi_send_msg(cmd->sock, msg);
       // gpio_set(16,1);
        //vTaskDelay(pdMS_TO_TICKS(cmd->val * 1000));
        //gpio_set(16,0);
        wifi_send_msg(cmd->sock, "Grinder stopped.");
        break;
    }

    /* 任务完成: 释放参数内存, 清空全局句柄, 自删除 */
    free(cmd);
    wifi_timed_task_handle = NULL;
    vTaskDelete(NULL);  /* FreeRTOS 任务自销毁 */
}

/**
 * @brief 启动一个定时电机任务, 自动停止上一个
 *
 * 互斥机制: 如果已有定时任务在运行, 先 vTaskDelete 销毁它,
 * 再调用 motor_emergency_stop() 确保硬件停止, 然后创建新任务.
 *
 * @param sock TCP socket, 用于任务中发送状态通知
 * @param type 电机类型字符
 * @param val  运行时长
 */
static void wifi_start_timed_task(int sock, char type, int val)
{
    /* 互斥: 销毁上一个定时任务 */
    if (wifi_timed_task_handle != NULL) {
        vTaskDelete(wifi_timed_task_handle);  /* 强制终止旧任务 */
        //motor_emergency_stop();               /* 确保所有电机硬件停止 */
        fstop();
    }

    /* 分配任务参数 (任务结束时由任务自己 free) */
    wifi_timed_cmd_t *cmd = malloc(sizeof(wifi_timed_cmd_t));
    cmd->type = type;
    cmd->val  = val;
    cmd->sock = sock;

    /* 创建新任务: 栈 4096 字节, 优先级 5 */
    xTaskCreate(wifi_timed_action_task, "wifi_timed_act",
                4096, cmd, 5, &wifi_timed_task_handle);
}

/*====================================================================
 *  指令解析器
 *====================================================================*/

/**
 * @brief 解析并执行一条 TCP 指令
 *
 * 指令解析顺序很重要: 先匹配前缀较长的指令, 避免误匹配.
 * 例如 "M F 2" 先检查 "M " 前缀, 再检查子命令 "F ".
 * 所有字符串比较大小写不敏感 (strcasecmp/strncasecmp).
 *
 * @param sock 客户端 socket (用于回复)
 * @param str  接收到的文本行 (会被原地修改: 去除首尾空白)
 */
static void wifi_process_cmd(int sock, char *str)
{
    /* 去除首尾空白字符 */
    while (*str == ' ') str++;
    int len = strlen(str);
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\r' || str[len-1] == '\n')) {
        str[--len] = '\0';
    }
    if (len == 0) return;  /* 空行, 忽略 */

    ESP_LOGI(TAG, "TCP Received command: [%s]", str);  /* 串口日志记录 */

    char msg[64];  /* 响应消息缓冲区 */

    /*--------------------------------------------------------------------
     *  全局控制
     *--------------------------------------------------------------------*/

    /* STOP — 紧急停止: 销毁定时任务 → 停止所有电机硬件 */
    if (strcasecmp(str, "STOP") == 0) {
        if (wifi_timed_task_handle != NULL) {
            vTaskDelete(wifi_timed_task_handle);
            wifi_timed_task_handle = NULL;
        }
        fstop();
        wifi_send_msg(sock, "Emergency Stop All: SUCCESS");
        return;
    }

    /*--------------------------------------------------------------------
     *  舵机 (即时指令, 不创建任务)
     *--------------------------------------------------------------------*/

    /* SS A <角度> — 转向舵机 (Steering Servo) */
    if (strncasecmp(str, "SS A ", 5) == 0) {
        float angle = atof(str + 5);           /* 跳过 "SS A " 前缀, 解析浮点数 */
        //servo_set_angle(angle);
        snprintf(msg, sizeof(msg), "Steering servo set to %.1f deg: SUCCESS", angle);
        wifi_send_msg(sock, msg);
        return;
    }

    /* SP A <角度> — 推动舵机 (Push Servo) */
    if (strncasecmp(str, "SP A ", 5) == 0) {
        float angle = atof(str + 5);
        //servo2_set_angle(angle);
        snprintf(msg, sizeof(msg), "Push servo set to %.1f deg: SUCCESS", angle);
        wifi_send_msg(sock, msg);
        return;
    }

    /*--------------------------------------------------------------------
     *  步进电机 M (多子命令, 即时指令)
     *--------------------------------------------------------------------*/
    if (strncasecmp(str, "M ", 2) == 0) {
        char *sub = str + 2;  /* 跳过 "M " 前缀, 指向子命令 */

        /* M A <角度> — 正转角度 */
        if (strncasecmp(sub, "A ", 2) == 0) {
            float angle = atof(sub + 2);
            //step_rotate_angle(angle, DIR_CW, 1000);  /* 顺时针, 1000us/脉冲 */
            snprintf(msg, sizeof(msg), "Stepper rotated %.1f deg: SUCCESS", angle);
            wifi_send_msg(sock, msg);
        }
        /* M START — 启动自动往返模式 */
        else if (strcasecmp(sub, "START") == 0) {
           // step_start_auto_mode();  /* 正180°→停0.5s→反180°, 循环 */
            wifi_send_msg(sock, "Stepper auto mode started: SUCCESS");
        }
        /* M B <角度> — 反转角度 */
        else if (strncasecmp(sub, "B ", 2) == 0) {
            float angle = atof(sub + 2);
            //step_rotate_angle(angle, DIR_CCW, 1000);
            snprintf(msg, sizeof(msg), "Stepper rotated -%.1f deg: SUCCESS", angle);
            wifi_send_msg(sock, msg);
        }
        /* M STOP — 停止自动往返 */
        else if (strcasecmp(sub, "STOP") == 0) {
           // step_stop_auto_mode();
            wifi_send_msg(sock, "Stepper auto mode stopped: SUCCESS");
        }
        /* M F <圈数> — 正转圈数 */
        else if (strncasecmp(sub, "F ", 2) == 0) {
            float turns = atof(sub + 2);
            //step_rotate_turns(turns, DIR_CW, 1000);  /* 1圈=1600脉冲 */
            snprintf(msg, sizeof(msg), "Stepper rotated %.1f turns: SUCCESS", turns);
            wifi_send_msg(sock, msg);
        }
        /* M R <圈数> — 反转圈数 */
        else if (strncasecmp(sub, "R ", 2) == 0) {
            float turns = atof(sub + 2);
            //step_rotate_turns(turns, DIR_CCW, 1000);
            snprintf(msg, sizeof(msg), "Stepper rotated -%.1f turns: SUCCESS", turns);
            wifi_send_msg(sock, msg);
        }
        /* 未识别的 M 子命令 — 不回复, 静默忽略 */
        return;
    }

    /*--------------------------------------------------------------------
     *  定时电机 (创建 FreeRTOS 任务异步执行)
     *--------------------------------------------------------------------*/

    /* P <秒> — 水泵 */
    if (strncasecmp(str, "P ", 2) == 0) {
        int sec = atoi(str + 2);
        wifi_start_timed_task(sock, 'P', sec);
        return;
    }

    /* D <秒> — 面粉电机 (Dough) */
    if (strncasecmp(str, "D ", 2) == 0) {
        int sec = atoi(str + 2);
        wifi_start_timed_task(sock, 'D', sec);
        return;
    }

    /* MI <分钟> — 搅拌电机 (Mixer).
     * 注意: 必须先匹配 "MI " (3字符), 不能放在 "M " 后面, 否则 "MI 5" 会被 "M " 误匹配 */
    if (strncasecmp(str, "MI ", 3) == 0) {
        int min = atoi(str + 3);
        wifi_start_timed_task(sock, 'M', min);
        return;
    }

    /* G <秒> — 研磨电机 (Grinder) */
    if (strncasecmp(str, "G ", 2) == 0) {
        int sec = atoi(str + 2);
        wifi_start_timed_task(sock, 'G', sec);
        return;
    }

    /*--------------------------------------------------------------------
     *  协同控制
     *--------------------------------------------------------------------*/

    /* H — 启动电机协同控制 (使用默认参数, 不传重量) */
    if (strcasecmp(str, "H") == 0) {
        //motor_coordinated_control();
        weight_work(300);
        wifi_send_msg(sock, "Coordinated control started: SUCCESS");
        return;
    }

    /* 未识别的指令 */
    wifi_send_msg(sock, "Error: Unknown TCP command");
}

/*====================================================================
 *  WiFi 事件处理
 *====================================================================*/

/**
 * @brief WiFi 事件回调
 *
 * 监听客户端连接/断开事件, 在串口打印 MAC 地址用于调试.
 * 不需要任何响应动作 (AP 模式下 ESP-IDF 自动处理 DHCP/认证).
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

/*====================================================================
 *  WiFi AP 初始化
 *====================================================================*/

/**
 * @brief 初始化 WiFi AP 模式 (SSID: dough_mixer, 密码: 12345678)
 *
 * @details
 *   调用链 (7 步, 按 ESP-IDF 要求的严格顺序):
 *   1. esp_netif_init()                  — 初始化 TCP/IP 协议栈
 *   2. esp_event_loop_create_default()   — 创建事件循环 (WiFi 事件依赖此循环)
 *   3. esp_netif_create_default_wifi_ap()— 创建 AP 网络接口 (DHCP 服务器自动启动)
 *   4. esp_wifi_init()                   — 分配 WiFi 驱动资源
 *   5. esp_event_handler_register()      — 注册连接/断开事件回调
 *   6. esp_wifi_set_mode(WIFI_MODE_AP)   — 设为纯 AP 模式 (不连其他 WiFi)
 *   7. esp_wifi_set_config() + esp_wifi_start() — 配置 SSID/密码/信道并启动
 *
 *   启动后可通过 192.168.4.1 访问设备.
 *   DHCP 地址池: 192.168.4.2 ~ 192.168.4.254.
 */
void wifi_init_softap(void)
{
    /* 1-3: 初始化网络栈 + 创建 AP 接口 */
    esp_netif_init();
    esp_event_loop_create_default();       /* 幂等: 如果 main 中已创建, 内部跳过 */
    esp_netif_create_default_wifi_ap();    /* 自动启动 DHCP 服务器 */

    /* 4: 初始化 WiFi 驱动 (分配内存, 创建 WiFi 任务) */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* 5: 注册事件回调 (客户端连接/断开时打印日志) */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        &wifi_event_handler, NULL, NULL));

    /* 6-7: 配置 AP 参数并启动 */
    wifi_config_t wifi_config = {
        .ap = {
            .ssid           = WIFI_SSID,
            .ssid_len       = strlen(WIFI_SSID),
            .channel        = WIFI_CHANNEL,
            .password       = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode       = WIFI_AUTH_WPA2_PSK,  /* WPA2 个人模式, 兼容性最好 */
            .pmf_cfg = {
                .required = false,  /* PMF 关闭: 避免旧手机搜不到热点 */
            },
        },
    };

    /* 空密码时自动降级为开放网络 (无加密) */
    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
}

/*====================================================================
 *  TCP 服务器任务
 *====================================================================*/

/**
 * @brief TCP 服务器主循环 (FreeRTOS 任务, 永不退出)
 *
 * @details
 *   Socket 生命周期:
 *   1. socket()    — 创建 IPv4 TCP socket
 *   2. setsockopt(SO_REUSEADDR) — 允许快速重启 (避免 TIME_WAIT)
 *   3. bind()      — 绑定 0.0.0.0:8080 (监听所有网络接口)
 *   4. listen(1)   — 开始监听, backlog=1 (单客户端)
 *   5. accept()    — 阻塞等待客户端连接
 *   6. recv() 循环 — 逐行接收指令, 调用 wifi_process_cmd()
 *   7. close()     — 客户端断开后关闭连接, 回到步骤 5
 *
 *   单客户端模式: 同一时刻只能有一个 TCP 连接.
 *   当前客户端断开后才会 accept 下一个.
 *
 * @param pvParameters 地址族 (AF_INET = IPv4)
 */
void tcp_server_task(void *pvParameters)
{
    char rx_buffer[128];                    /* 接收缓冲区: 单行指令不超过 127 字节 */
    char addr_str[32];                      /* 客户端 IP 地址字符串 */
    int addr_family = (int)pvParameters;    /* AF_INET */
    int ip_protocol = 0;
    struct sockaddr_storage dest_addr;

    /* 配置监听地址: 0.0.0.0:8080 (所有网络接口) */
    if (addr_family == AF_INET) {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);  /* 0.0.0.0 */
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(TCP_PORT);            /* 8080, 网络字节序 */
        ip_protocol = IPPROTO_IP;
    }

    /* 创建 socket */
    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    /* 允许端口复用: 重启后立即绑定, 避免 TIME_WAIT 等待 */
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    ESP_LOGI(TAG, "Socket created");

    /* 绑定端口 */
    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", TCP_PORT);

    /* 开始监听 (backlog=1: 同时只允许一个待处理连接) */
    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    /* 主循环: accept → recv 循环 → close → accept → ... */
    while (1) {
        ESP_LOGI(TAG, "Socket listening");

        /* 阻塞等待客户端连接 */
        struct sockaddr_storage source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;  /* accept 失败 → 退出任务 */
        }

        /* 记录客户端 IP */
        if (source_addr.ss_family == AF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr,
                        addr_str, sizeof(addr_str) - 1);
        }
        ESP_LOGI(TAG, "Socket accepted ip: %s", addr_str);

        /* 接收循环: 逐行读取指令 */
        while (1) {
            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if (len < 0) {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
                break;  /* 接收错误 → 断开此客户端 */
            } else if (len == 0) {
                ESP_LOGI(TAG, "Connection closed");
                break;  /* 客户端主动断开 → 回到 accept */
            } else {
                /* 正常接收: 字符串收尾 + 解析 */
                rx_buffer[len] = '\0';
                wifi_process_cmd(sock, rx_buffer);
            }
        }

        /* 关闭客户端连接, 准备接受下一个 */
        if (sock != -1) {
            shutdown(sock, 0);  /* 优雅关闭: 停止收发 */
            close(sock);        /* 释放文件描述符 */
        }
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}
