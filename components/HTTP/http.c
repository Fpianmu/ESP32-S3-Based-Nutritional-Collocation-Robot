/**
 * @file    http.c
 * @brief   HTTP 服务器 — 网页 + REST API
 *
 * CMake EMBED_FILES 将 http.html 嵌入固件, 运行时直接发送二进制数据.
 * /api/start 从 JSON body 解析 weight 参数, 创建独立 FreeRTOS 任务执行电机控制.
 * /api/stop 销毁电机任务 + 调用 motor_emergency_stop().
 */

#include "http.h"
#include "function.h"
#include "ctl_mutex.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "HTTP";

/** 嵌入式网页二进制数据 (由 CMake EMBED_FILES 自动生成) */
extern const unsigned char _binary_http_html_start[] asm("_binary_http_html_start");
extern const unsigned char _binary_http_html_end[]   asm("_binary_http_html_end");

static const char *g_motor_state = "idle";
static TaskHandle_t g_motor_task_handle = NULL;
static uint32_t g_current_weight = 0;

/* ---- 电机任务：调用 motor_coordinated_control_ui_http ---- */
static void motor_task(void *pvParameters)
{
    weight_work(g_current_weight);
    g_motor_state = "idle";
    g_motor_task_handle = NULL;
    ESP_LOGI(TAG, "Motor task completed, back to idle");
    vTaskDelete(NULL);
}

/* ---- GET / ---- */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    size_t html_size = _binary_http_html_end - _binary_http_html_start;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)_binary_http_html_start, html_size);
    return ESP_OK;
}

/* ---- POST /api/start — 传入 weight 启动电机 ---- */
static esp_err_t api_start_post_handler(httpd_req_t *req)
{
    char buf[128];
    int recv_len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (recv_len <= 0) {
        const char *resp = "{\"status\":\"error\",\"msg\":\"empty body\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }
    buf[recv_len] = '\0';

    /* 简单解析 {"weight":500} */
    uint32_t weight = 200;  // 默认值
    char *p = strstr(buf, "\"weight\"");
    if (p) {
        p = strstr(p, ":");
        if (p) {
            weight = (uint32_t)strtoul(p + 1, NULL, 10);
        }
    }
    if (weight < 50) weight = 50;

    char resp[128];
    if (!ctl_try_acquire(CTL_HTTP)) {
        const char *resp = "{\"status\":\"busy\",\"msg\":\"system locked\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }
    if (strcmp(g_motor_state, "idle") == 0) {
        g_current_weight = weight;
        g_motor_state = "kneading";
        BaseType_t ret = xTaskCreate(motor_task, "motor_task", 8192,
                                      NULL, 5, &g_motor_task_handle);
        if (ret != pdPASS) {
            g_motor_state = "idle";
            snprintf(resp, sizeof(resp),
                     "{\"status\":\"error\",\"msg\":\"task failed\"}");
        } else {
            snprintf(resp, sizeof(resp),
                     "{\"status\":\"ok\",\"state\":\"kneading\",\"weight\":%lu}",
                     (unsigned long)weight);
        }
    } else {
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"busy\",\"state\":\"%s\"}", g_motor_state);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

/* ---- GET /api/status ---- */
static esp_err_t api_status_get_handler(httpd_req_t *req)
{
    char resp[64];
    snprintf(resp, sizeof(resp), "{\"state\":\"%s\"}", g_motor_state);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

/* ---- POST /api/stop — 紧急停止 ---- */
static esp_err_t api_stop_post_handler(httpd_req_t *req)
{
    if (g_motor_task_handle != NULL) {
        vTaskDelete(g_motor_task_handle);
        g_motor_task_handle = NULL;
    }
    fstop();
    ctl_release(CTL_HTTP);
    g_motor_state = "idle";
    ESP_LOGI(TAG, "Emergency stop");

    const char *resp = "{\"status\":\"ok\",\"state\":\"idle\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

/* ---- 注册 URI ---- */
static void register_handlers(httpd_handle_t server)
{
    httpd_uri_t handlers[] = {
        { .uri = "/",          .method = HTTP_GET,  .handler = root_get_handler },
        { .uri = "/api/start", .method = HTTP_POST, .handler = api_start_post_handler },
        { .uri = "/api/status",.method = HTTP_GET,  .handler = api_status_get_handler },
        { .uri = "/api/stop",  .method = HTTP_POST, .handler = api_stop_post_handler },
    };
    for (int i = 0; i < sizeof(handlers) / sizeof(handlers[0]); i++) {
        httpd_register_uri_handler(server, &handlers[i]);
    }
}

/* ---- 启动 HTTP 服务器 ---- */
void http_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 8;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        register_handlers(server);
        ESP_LOGI(TAG, "HTTP server started on port 80");
    } else {
        ESP_LOGE(TAG, "HTTP server failed to start");
    }
}
