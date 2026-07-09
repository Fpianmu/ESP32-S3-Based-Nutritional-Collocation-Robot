#include "esp_log.h"
#include "esp_err.h"
#include "ledc_pwm.h"
#include "gpiowork.h"
#include "servo.h"
#include "servo2.h"
#include "esc_controller.h"
#include "stepper.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lvgl_port.h"
#include "esp_event.h"
#include "lcd.h"
#include "function.h"
#include "touch.h"
#include "lvgl_ui.h"
#include "ui.h"
#include "vars.h"
#include "nvs_flash.h"
#include "wifi.h"
#include "http.h"
#include "xiaozhi.h"
#include "ctl_mutex.h"
#include <stdio.h>

static const char *TAG = "function";

esc_handle_t esc1,esc2;

const float k_flour = 120/267.5; //120/267.5
const float k_water = 65/267.5;
const float k_grain = 80/267.5;
const float k_yeast = 2/267.5;
const float k_salt = 0.5/267.5;

int flour,water,grain,yeast,salt;

void init_all()
{
    // 启动 GPIO  
    ledc_pwm_set_state(0); //水泵 (PWM)
    gpio_set(7,0);
    gpio_set(46,0); //研磨
    gpio_set(18,0); //面粉磁铁
    //初始化舵机
    servo_init(8); //旋转
    servo2_init(3); //击打
    //步进电机初始化
    stepper_config_t config = 
    {
        .en_pin = 6,
        .dir_pin = 5,
        .stp_pin = 4,
        .freq_hz = 1000,
        .duration_ms = 5000   // 仅用于 stepper_start 的默认值
    };
    stepper_init(&config);
    //电调初始化
     // 初始化电调1：GPIO 15，使用通道 6
    esc_init(&esc1, 15, LEDC_CHANNEL_6);  //面粉
    // 初始化电调2：GPIO 9，使用通道 7
    esc_init(&esc2, 9, LEDC_CHANNEL_7);  //搅拌

    // 注意：必须在校准过程中（esc_init 函数内延时3秒时）
    // 手动为对应的电调上电，否则校准失败！

    vTaskDelay(pdMS_TO_TICKS(1000));

     /* ---- 系统基础 ---- */
    nvs_flash_init();                  /* NVS 存储 (WiFi 配置) */
    esp_event_loop_create_default();   /* 事件循环 (WiFi + HTTP 依赖) */

    /* ---- WiFi AP + TCP 服务器 (8080) ---- */
    wifi_init_softap();
    xTaskCreate(tcp_server_task, "tcp_server", 4096, (void*)AF_INET, 5, NULL);

    /* ---- HTTP 服务器 (80, 网页控制面板) ---- */
    http_server_start();

    /* ---- 小智语音控制 (I2S 音频 + WebSocket) ---- */
    xTaskCreate((TaskFunction_t)xz_init_wrapper, "xz_init", 4096 * 3, NULL, 3, NULL);

    /* ---- LCD ---- */
    bsp_lcd_init();
    ESP_LOGI(TAG, "LCD init done");

    /* ---- 触摸 ---- */
    ft6336u_touch_init();

    /* ---- LVGL 内核 + 注册显示/触摸 ---- */
    lvgl_ui_init();

    /* ---- EEZ Studio UI ---- */
    lvgl_port_lock(0);
    vars_init();       /* 初始化全局变量 (dough_weight 默认 200g) */
    ui_init();         /* EEZ Flow 初始化 + 创建 6 屏幕 + 加载首页 */
    lvgl_port_unlock();

    ESP_LOGI(TAG, "System started, WiFi: dough_mixer / 12345678");
}
void fstop(void)
{
    esc_set_throttle(&esc1, 0.0); 
    esc_set_throttle(&esc2, 0.0); 
    ledc_pwm_set_state(0);
    gpio_set(7,0);
    gpio_set(46,0);
    gpio_set(18,0);
    stepper_stop();
}

void push_and_out(int direction)
{
    stepper_move_for(4800, 1000, direction);
}

void weight_work(uint32_t weight)
{
    flour =(int)((weight*k_flour*1000)/2.8324);
    water =(int)((weight*k_water*1000)/28.9);
    grain =(int)((weight*k_grain*1000*60)/4.43);
    yeast =(int)((weight*k_yeast)/0.3);
    salt =(int)((weight*k_salt)/0.3);
    if (weight > 50)
    {
        fwork(flour,water,500,grain,400000);
    }
    else
    {
        yeast = 4;
        salt = 2;
        fwork(20000,1500,500,40000,30000);
    }
}

void fwork(int duration1,int duration2,int duration3,int duration4,int duration5)
{
    //ledc_pwm_set_state(1);
    /*
    初始化全部完成
    开始工作
    */
   //Task1 加入面粉
    if (duration1 > 10000)
    {
        int n = duration1/10000;
        int time_left = duration1 % 10000;
        for (int i = 1;i <= n;i++)
        {
            esc_set_throttle(&esc1, 40.0); // 电调1 40%油门
            gpio_set(18,1);
            vTaskDelay(pdMS_TO_TICKS(10000));
            esc_set_throttle(&esc1, 0.0); // 电调1 熄火
            gpio_set(18,0);
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
        esc_set_throttle(&esc1, 40.0); // 电调1 40%油门
        gpio_set(18,1);
        vTaskDelay(pdMS_TO_TICKS(time_left));
        esc_set_throttle(&esc1, 0.0); // 电调1 熄火
        gpio_set(18,0);
    }
    else
    {
         esc_set_throttle(&esc1, 40.0); // 电调1 40%油门
         gpio_set(18,1);
         vTaskDelay(pdMS_TO_TICKS(duration1));
         esc_set_throttle(&esc1, 0.0); // 电调1 熄火
         gpio_set(18,0);
    }
    //Task2 水泵工作
    ledc_pwm_set_state(1); //水泵 (PWM)
    gpio_set(7,1);
    vTaskDelay(pdMS_TO_TICKS(duration2));
    ledc_pwm_set_state(0); //水泵 (PWM)
    gpio_set(7,0);
    //Task3 研磨电机与加料舵机同时工作
   
    for (int i=1;i<=yeast;i++)
    {
        //加料1
        servo_set_angle(145);
        vTaskDelay(pdMS_TO_TICKS(500));
        servo2_set_angle(65);
        vTaskDelay(pdMS_TO_TICKS(duration3));
        servo2_set_angle(0);
        vTaskDelay(pdMS_TO_TICKS(200));
        
    }
    for (int i=1;i<=salt;i++)
    {
        //加料2
        servo_set_angle(55);
        vTaskDelay(pdMS_TO_TICKS(500));
        servo2_set_angle(60);
        vTaskDelay(pdMS_TO_TICKS(duration3));
        servo2_set_angle(0);
    } 
    int num = duration4 / 60000 ;
    if (duration4 > 60000)
    {
        for (int i=1;i<=num;i++)
        {
            gpio_set(46,1); //研磨
            vTaskDelay(pdMS_TO_TICKS(60000));
            //研磨停止
            gpio_set(46,0); //研磨
            vTaskDelay(pdMS_TO_TICKS(30000));
        }
        vTaskDelay(pdMS_TO_TICKS(duration4%60000));
    }
    else
    {
        gpio_set(46,1); //研磨
        vTaskDelay(pdMS_TO_TICKS(duration4));
        //研磨停止
        gpio_set(46,0); //研磨
    }

    //Task4 关盖子搅拌
    // 方式1：使用动态函数，正转 3 秒，频率 1000Hz
    //stepper_move_for(1500, 1000, 0);
    //vTaskDelay(pdMS_TO_TICKS(3000)); // 等待运动结束

    esc_set_throttle(&esc2, 40.0); // 电调2 40%油门
    vTaskDelay(pdMS_TO_TICKS(duration5));
    esc_set_throttle(&esc2, 0.0); // 电调2 熄火

    // 方式2：反转 2 秒，频率 1000Hz
    //stepper_move_for(1500, 1000, 1);
    //vTaskDelay(pdMS_TO_TICKS(3000));
}
