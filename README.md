# ESP32-S3 智能全自动和面机

> 基于营养推荐的杂粮配比系统 —— 全国大学生嵌入式芯片与系统设计竞赛作品

## 操作指南

### 硬件准备

1. ESP32-S3 开发板烧录固件：`idf.py flash`
2. 连接 DRV8870 水泵（GPIO12/13）、ESC 面粉电调（GPIO15）、ESC 搅拌电调（GPIO9）
3. 连接双舵机（旋转 GPIO8、击打 GPIO3）、步进电机（GPIO4/5/6）
4. 连接 ST7789V LCD 触控屏、INMP441 麦克风、MAX98357A 喇叭功放
5. 连接 YF-S201 流量计（GPIO37，需 5V→3.3V 分压）
6. 上电后自动创建 WiFi 热点 `dough_mixer`（密码 `12345678`）

### 三种控制方式

| 方式 | 操作 |
|------|------|
| **触控屏** | 选择面团重量（200/300/500/1000g），点击"启动" |
| **网页控制** | 手机连接 WiFi → 浏览器打开 `http://192.168.4.1` → 输入重量点 Start |
| **微信小程序 + AI 语音** | 小程序描述饮食 → AI 推荐杂粮配比 → 确认后下发执行 |

### 启动服务端（微信小程序/AI 语音功能需要）

```bash
# 小智 AI 服务端（WebSocket，端口 8000）
cd server/main/xiaozhi-server
pip install -r requirements.txt
python app.py

# 营养推荐引擎（FastAPI，端口 8100）
cd server/nutrition-service
uvicorn nutrition_service.api:app --host 0.0.0.0 --port 8100
```

### 语音唤醒

说出唤醒词 **"你好小智"** 进入语音交互模式。

---

## 系统架构

```
用户交互层：触控屏(LVGL) + 网页(HTTP REST) + 微信小程序(小智AI)
    ↕ ctl_mutex 三端互斥锁
智能服务层：小智 AI 服务端(ASR/LLM/TTS) + 营养推荐引擎(nutrition-service)
    ↕ WebSocket / MCP 协议
主控决策层：ESP32-S3 + FreeRTOS + 核心工作流引擎(function.c)
    ↕ LEDC / I2S / GPIO / SPI
硬件执行层：DRV8870水泵 + ESC无刷电调 + 双舵机 + 步进电机 + 流量计
```

### 和面流程（四阶段）

1. **Task1 面粉加入**：ESC1 40% 油门 + 磁铁阀 GPIO18 打开，延时后关闭
2. **Task2 水泵加水**：DRV8870 50% PWM 开启，流量计脉冲计数达标后关闭
3. **Task3 研磨 + 加料**：研磨电机开，双舵机按酵母/盐循环次数交替投料
4. **Task4 搅拌**：ESC2 40% 油门，延时后关闭 → 完成

---

## 目录结构

```
├── main/project.c             # 入口函数 app_main()
├── components/
│   ├── function/              # 核心工作流引擎
│   ├── ctl_mutex/             # 三端互斥锁
│   ├── ledc_pwm/              # DRV8870 水泵 PWM (5kHz)
│   ├── servo/servo2/          # 双舵机 PWM (50Hz)
│   ├── esc_controller/        # 无刷电调 ESC (50Hz)
│   ├── stepper/               # 步进电机脉冲控制
│   ├── xiaozhi/               # 小智 AI 语音控制 (I2S+Opus+ESP-SR+WebSocket)
│   ├── ui/                    # EEZ Studio 图形界面 + LVGL
│   ├── WIFI/                  # WiFi AP + TCP 指令服务器
│   ├── HTTP/                  # HTTP 服务器 + REST API
│   └── opus/json/esp-*/       # 音频/语音/编解码组件
├── server/
│   ├── main/xiaozhi-server/   # 小智 AI 服务端
│   └── nutrition-service/     # 营养推荐引擎 (FastAPI + SQLite)
├── spiffs_model/              # ESP-SR 唤醒词模型
├── lv_conf.h                  # LVGL 配置文件
├── partitions_custom.csv      # 分区表 (工厂 3MB)
└── sdkconfig                  # ESP-IDF 配置
```

## 编译

```bash
source ~/esp/v601/esp-idf/export.sh
idf.py build
idf.py -p PORT flash
parttool.py write_partition --partition-name=model --input=spiffs_model/
```

## 关键技术

- **LEDC 资源 8/8 通道全满**，4 定时器无空闲，7 路独立 PWM
- **I2S 全双工**：4 GPIO 同时驱动麦克风输入和喇叭输出
- **流量计闭环控制**：YF-S201 脉冲计数，精度 ±2~5%（传统定时 ±20%）
- **三端互斥锁**：FreeRTOS 非阻塞信号量，触控/网页/AI 零冲突
- **唤醒词引擎**：ESP-SR MultiNet，Opus 16kHz 编码
- **云端营养推荐**：DeepSeek/ChatGLM + 规则引擎，动态计算杂粮配比

## 引脚分配

| GPIO | 功能 | 类型 |
|------|------|------|
| 3,8 | 双舵机 | PWM 50Hz |
| 4,5,6 | 步进电机 | 脉冲/DIR/EN |
| 7,14,16,17 | I2S 音频 | 全双工 |
| 9 | 搅拌 ESC | PWM 50Hz |
| 12,13 | 水泵 DRV8870 | PWM 5kHz |
| 15 | 面粉 ESC | PWM 50Hz |
| 18 | 面粉磁铁阀 | 继电器 |
| 37 | 流量计 | 脉冲中断 |
| 38,47 | I2C 触摸 | SCL/SDA |
| 40,41,42 | LCD SPI | SCK/MOSI/DC |
| 46 | 研磨电机 | 继电器 |

## License

MIT
