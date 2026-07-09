/**
 * @file    xiaozhi.cc
 * @brief   小智语音控制 — 集成 xiaozhi-esp32 音频管线 + MCP 指令映射
 *
 * 架构:
 *   麦克风(I2S) → Opus编码 → WebSocket → 小智服务器(ASR/NLP)
 *   小智服务器 → MCP指令 / TTS音频 → WebSocket → 喇叭(I2S)
 *
 * MCP 指令映射:
 *   start_mixer {weight} → weight_work(weight)  [需获取互斥锁]
 *   stop_mixer           → fstop()
 *   push_out             → push_and_out(1)
 *   push_back            → push_and_out(0)
 */
#include "xiaozhi.h"
#include "ctl_mutex.h"
#include "function.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <cJSON.h>

#include "wss_client.h"
#include "no_audio_codec.h"
#include "audio_service.h"

static const char *TAG = "XZ";

/* ---- I2S 引脚 (与迭代规划一致) ---- */
#define I2S_BCLK  GPIO_NUM_14
#define I2S_WS    GPIO_NUM_16
#define I2S_DIN   GPIO_NUM_17
#define I2S_DOUT  GPIO_NUM_7

/* ---- 服务器地址 (电脑连接 ESP AP 后通常为 192.168.4.2) ---- */
#define DEFAULT_WS_URL  "ws://192.168.4.2:8000/xiaozhi/v1/"

/* ---- 全局状态 ---- */
static AudioService g_audio;
static bool  g_ws_ok      = false;
static char  g_session_id[64] = {0};
static uint32_t g_timestamp  = 0;
static int   g_state      = 0;   /* 0=IDLE 1=WAKED 2=REC 3=SPEAKING */
static char  g_ws_url[256] = DEFAULT_WS_URL;

/* ---- 前向声明 ---- */
static void send_hello();
static void send_listen(const char *state);
static void send_listen_detect(const char *text);
static void handle_mcp_command(cJSON *mcp);
static bool connect_ws();

/* ---- JSON 发送 ---- */
static void ws_send_json(cJSON *obj)
{
    char *j = cJSON_PrintUnformatted(obj);
    ESP_LOGI(TAG, "TX[%d]: %s", g_state, j);
    wss_send_text(j, strlen(j));
    free(j);
    cJSON_Delete(obj);
}

static void add_session(cJSON *o)
{
    if (g_session_id[0])
        cJSON_AddStringToObject(o, "session_id", g_session_id);
}

static void send_hello()
{
    cJSON *h = cJSON_CreateObject();
    cJSON_AddStringToObject(h, "type", "hello");
    cJSON_AddNumberToObject(h, "version", 1);
    cJSON_AddStringToObject(h, "transport", "websocket");

    cJSON *a = cJSON_CreateObject();
    cJSON_AddStringToObject(a, "format", "opus");
    cJSON_AddNumberToObject(a, "sample_rate", 16000);
    cJSON_AddNumberToObject(a, "channels", 1);
    cJSON_AddNumberToObject(a, "frame_duration", 60);
    cJSON_AddItemToObject(h, "audio_params", a);

    cJSON *f = cJSON_CreateObject();
    cJSON_AddBoolToObject(f, "mcp", true);   /* 开启 MCP 设备控制 */
    cJSON_AddBoolToObject(f, "emoji", false);
    cJSON_AddItemToObject(h, "features", f);

    ws_send_json(h);
}

static void send_listen(const char *state)
{
    cJSON *l = cJSON_CreateObject();
    cJSON_AddStringToObject(l, "type", "listen");
    cJSON_AddStringToObject(l, "state", state);
    cJSON_AddStringToObject(l, "mode", "auto");
    add_session(l);
    ws_send_json(l);
}

static void send_listen_detect(const char *text)
{
    cJSON *l = cJSON_CreateObject();
    cJSON_AddStringToObject(l, "type", "listen");
    cJSON_AddStringToObject(l, "state", "detect");
    cJSON_AddStringToObject(l, "text", text);
    add_session(l);
    ws_send_json(l);
}

/* ---- 音频帧发送 ---- */
static void send_audio_frame(const uint8_t *opus, size_t len)
{
    uint8_t f[528];
    memset(f, 0, 16);
    uint32_t ts = g_timestamp++;
    f[8]  = (ts >> 24) & 0xFF;
    f[9]  = (ts >> 16) & 0xFF;
    f[10] = (ts >> 8)  & 0xFF;
    f[11] =  ts        & 0xFF;
    f[12] = (len >> 24) & 0xFF;
    f[13] = (len >> 16) & 0xFF;
    f[14] = (len >> 8)  & 0xFF;
    f[15] =  len        & 0xFF;
    memcpy(f + 16, opus, len);
    wss_send_bin(f, 16 + len);
}

/* ---- MCP 指令处理 ---- */
static void handle_mcp_command(cJSON *mcp)
{
    auto *tool = cJSON_GetObjectItem(mcp, "tool");
    if (!tool || !tool->valuestring) return;

    const char *name = tool->valuestring;
    ESP_LOGI(TAG, "MCP tool: %s", name);

    if (strcmp(name, "stop_mixer") == 0 || strcmp(name, "stop") == 0) {
        fstop();
        ctl_release(CTL_XIAOZHI);
        return;
    }

    if (strcmp(name, "start_mixer") == 0) {
        auto *args = cJSON_GetObjectItem(mcp, "args");
        uint32_t weight = 300;
        if (args) {
            auto *w = cJSON_GetObjectItem(args, "weight");
            if (w) weight = (uint32_t)w->valueint;
        }
        if (ctl_try_acquire(CTL_XIAOZHI)) {
            weight_work(weight);
            /* weight_work 内部调用 fwork 会阻塞直到完成 */
            ctl_release(CTL_XIAOZHI);
        } else {
            ESP_LOGW(TAG, "Cannot start: locked by %d", (int)ctl_get_owner());
        }
        return;
    }

    if (strcmp(name, "push_out") == 0) {
        push_and_out(1);
        return;
    }

    if (strcmp(name, "push_back") == 0) {
        push_and_out(0);
        return;
    }
}

/* ---- WebSocket 消息处理 ---- */
static void handle_text(const char *d, size_t n)
{
    ESP_LOGI(TAG, "RX[%d]: %.*s", g_state, (int)n, d);
    auto *r = cJSON_ParseWithLength(d, n);
    if (!r) return;

    auto *t = cJSON_GetObjectItem(r, "type");
    if (!t) { cJSON_Delete(r); return; }

    if (strcmp(t->valuestring, "hello") == 0) {
        auto *sid = cJSON_GetObjectItem(r, "session_id");
        if (sid) {
            strncpy(g_session_id, sid->valuestring, 63);
            ESP_LOGI(TAG, "SID: %s", g_session_id);
        }
    } else if (strcmp(t->valuestring, "tts") == 0) {
        auto *s = cJSON_GetObjectItem(r, "state");
        if (!s) { cJSON_Delete(r); return; }
        const char *sv = s->valuestring;
        ESP_LOGI(TAG, "TTS[%d]: %s", g_state, sv);
        if (strcmp(sv, "start") == 0) {
            g_state = 3;
        } else if (strcmp(sv, "stop") == 0) {
            if (g_state == 1 || g_state == 3) {
                g_state = 2;
                g_timestamp = 0;
                send_listen("start");
                g_audio.EnableVoiceProcessing(true);
            } else if (g_state == 2) {
                g_state = 0;
                send_listen("stop");
                g_audio.EnableVoiceProcessing(false);
            }
        }
    } else if (strcmp(t->valuestring, "mcp") == 0) {
        handle_mcp_command(r);
    }

    cJSON_Delete(r);
}

static void handle_bin(const uint8_t *d, size_t n)
{
    auto p = std::make_unique<AudioStreamPacket>(AudioStreamPacket{
        .sample_rate = 24000,
        .frame_duration = 60,
        .timestamp = 0,
        .payload = std::vector<uint8_t>(d, d + n)
    });
    g_audio.PushPacketToDecodeQueue(std::move(p));
}

/* ---- 唤醒词回调 ---- */
static void on_wake_word(const std::string &word)
{
    ESP_LOGI(TAG, "WAKE[%d]: %s", g_state, word.c_str());
    if (g_state != 0 || !g_ws_ok) return;
    g_state = 1;
    g_timestamp = 0;
    g_audio.EnableVoiceProcessing(true);
    send_listen("start");
    send_listen_detect(word.c_str());
}

/* ---- 麦克风发送任务 ---- */
static void mic_task(void *arg)
{
    while (true) {
        if ((g_state == 1 || g_state == 2) && g_ws_ok) {
            auto p = g_audio.PopPacketFromSendQueue();
            if (p)
                send_audio_frame(p->payload.data(), p->payload.size());
        } else if (g_state == 0 || g_state == 3) {
            g_audio.PopPacketFromSendQueue();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* ---- WebSocket 连接 ---- */
static bool connect_ws()
{
    g_ws_ok = false;
    memset(g_session_id, 0, sizeof(g_session_id));
    g_state = 0;

    wss_on_text(handle_text);
    wss_on_bin(handle_bin);

    /* 直接使用 DEFAULT_WS_URL (电脑连 ESP AP 后 IP 通常为 192.168.4.2) */
    const char *p = g_ws_url;
    int port = 80;
    if (!strncmp(p, "wss://", 6)) { p += 6; port = 443; }
    else if (!strncmp(p, "ws://", 5)) p += 5;

    char host[128] = {0}, path[256] = "/";
    const char *sl = strchr(p, '/'), *co = strchr(p, ':');
    if (co && (!sl || co < sl)) {
        memcpy(host, p, co - p);
        port = atoi(co + 1);
    } else if (sl) {
        memcpy(host, p, sl - p);
    } else {
        strncpy(host, p, sizeof(host) - 1);
    }
    if (sl) strncpy(path, sl, sizeof(path) - 1);

    g_ws_ok = wss_connect(host, port, path);
    if (!g_ws_ok) return false;

    send_hello();
    return true;
}

/* ---- WebSocket 保活任务 ---- */
static void ws_keepalive_task(void *arg)
{
    while (true) {
        wss_poll();
        if (!wss_is_connected()) {
            ESP_LOGW(TAG, "WS disconnected, reconnect in 10s...");
            vTaskDelay(pdMS_TO_TICKS(10000));
            connect_ws();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ---- 对外 API ---- */
void xz_init(void)
{
    ESP_LOGI(TAG, "=== XiaoZhi Voice Init ===");

    /* 创建 I2S 音频编解码器 (无外置芯片, 纯 I2S) */
    static NoAudioCodecDuplex *ac = nullptr;
    ac = new NoAudioCodecDuplex(16000, 16000, I2S_BCLK, I2S_WS, I2S_DOUT, I2S_DIN);
    g_audio.Initialize(ac);
    ac->EnableInput(true);
    ac->EnableOutput(true);
    ac->SetInputGain(3.0f);

    /* 加载唤醒词模型 */
    srmodel_list_t *models = esp_srmodel_init("model");
    if (models) g_audio.SetModelsList(models);

    /* 注册唤醒词回调 */
    AudioServiceCallbacks cbs = {};
    cbs.on_wake_word_detected = on_wake_word;
    g_audio.SetCallbacks(cbs);

    /* 启动音频管线 */
    g_audio.Start();
    g_audio.EnableWakeWordDetection(true);

    /* 连接 WebSocket 并启动任务 */
    if (connect_ws()) {
        ESP_LOGI(TAG, "WebSocket connected");
    } else {
        ESP_LOGW(TAG, "WebSocket not available, will retry in background");
    }
    xTaskCreate(mic_task, "xz_mic", 4096 * 2, NULL, 4, NULL);
    xTaskCreate(ws_keepalive_task, "xz_ws", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "XiaoZhi ready");
}

bool xz_is_speaking(void)
{
    return g_state == 3;
}

bool xz_is_listening(void)
{
    return g_state == 1 || g_state == 2;
}
