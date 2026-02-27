#ifndef _WIFI_STA_H_
#define _WIFI_STA_H_
#include "esp_err.h"

/*
 * 简易 WiFi STA 接口
 * 提供：
 * - 事件枚举 `WIFI_EV_e`：上层用于判断连接状态
 * - 回调类型 `wifi_event_cb`：当连接状态变化（例如获取到 IP）时通知上层
 * - 初始化函数 `wifi_sta_init()`：启动并连接到预配置的 AP
 */

/**
 * WiFi 事件枚举
 * - WIFI_DISCONNECTED: STA 未连接或断开
 * - WIFI_CONNECTED:    已成功获取 IP，网络可用
 */
typedef enum
{
    WIFI_DISCONNECTED,      // wifi 未连接或已断开
    WIFI_CONNECTED,         // wifi 已连接并获取到 IP
}WIFI_EV_e;

/**
 * 上层回调类型
 * 当 WiFi 状态发生变化（目前仅在获取到 IP 时调用 WIFI_CONNECTED）
 * 参数：事件类型（WIFI_EV_e）
 */
typedef void (*wifi_event_cb)(WIFI_EV_e);

/**
 * 初始化 WiFi STA 模式并开始连接
 * 参数：
 *  - f: 上层注册的事件回调，非必需（可传 NULL）
 * 返回：ESP_OK 表示初始化函数执行成功（并不代表立即连上网络）
 */
esp_err_t wifi_sta_init(wifi_event_cb f);

#endif
