/**
 * @file    http.h
 * @brief   HTTP 服务器 — 网页控制面板 + REST API
 *
 * 端点:
 *   GET  /           返回嵌入式网页 (http.html)
 *   POST /api/start  启动电机 (body: {"weight": 500})
 *   GET  /api/status 查询电机状态 ({"state":"idle"|"kneading"})
 *   POST /api/stop   紧急停止
 */

#ifndef __HTTP_H__
#define __HTTP_H__

#ifdef __cplusplus
extern "C" {
#endif

/** 启动 HTTP 服务器 (端口 80) */
void http_server_start(void);

#ifdef __cplusplus
}
#endif

#endif
