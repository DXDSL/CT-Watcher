#include "Wifi_STA.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

// 请根据实际环境修改为目标 WiFi 的 SSID 和密码用于连接测试
// 例如：
// #define DEFAULT_WIFI_SSID "MySSID"
// #define DEFAULT_WIFI_PASSWORD "MyPassword"
#define DEFAULT_WIFI_SSID "DSL"
#define DEFAULT_WIFI_PASSWORD "13342564606"

// 日志标签
static const char *TAG = "Wifi";

// 调用方提供的事件回调 (用于上层接收 WIFI_CONNECTED 等状态)
static wifi_event_cb wifi_cb = NULL;

/** 事件回调函数
 * @param arg   用户传递的参数
 * @param event_base    事件类别
 * @param event_id      事件ID
 * @param event_data    事件携带的数据
 * @return 无
 */
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    // 处理 WiFi 相关事件
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START: // STA 模式启动
            // 启动连接流程：尝试连接配置的 AP
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_CONNECTED: // 已连接到路由器（链路层）
            // 仅表示已完成链路层连接，尚未获取 IP
            ESP_LOGI(TAG, "connected to AP");
            break;
        case WIFI_EVENT_STA_DISCONNECTED: // 与路由器断开
            // 断开后自动重连，保持连接可用性
            esp_wifi_connect();
            ESP_LOGI(TAG, "connect to the AP fail, retry now");
            break;
        default:
            break;
        }
    }

    // 处理 IP 层事件（例如获取到 DHCP 分配的 IP）
    if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP: // 获取到路由器分配的 IP
            // 当获取到 IP 时，认为网络已可用，通知上层
            if (wifi_cb)
                wifi_cb(WIFI_CONNECTED);
            ESP_LOGI(TAG, "got ip address");
            break;
        default:
            break;
        }
    }
}

// WIFI STA初始化
esp_err_t wifi_sta_init(wifi_event_cb f)
{
    // 1) 初始化底层网络接口和事件循环
    ESP_ERROR_CHECK(esp_netif_init());                // 初始化 TCP/IP 协议栈
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // 创建默认事件循环以便注册事件回调

    // 2) 创建一个默认的 WiFi STA netif（网络接口）
    esp_netif_create_default_wifi_sta();

    // 3) 初始化 WiFi 驱动（使用默认配置）
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 4) 注册事件处理器：WIFI 层和 IP 层
    //    - WIFI_EVENT: 监听诸如启动、连接、断开等事件
    //    - IP_EVENT_STA_GOT_IP: 当 STA 获取到 IP 时触发
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    // 5) 配置 WiFi 参数（SSID、密码、加密方式、PMF 等）
    wifi_config_t wifi_config =
        {
            .sta =
                {
                    .ssid = DEFAULT_WIFI_SSID,                // 目标 WiFi 名称
                    .password = DEFAULT_WIFI_PASSWORD,        // WiFi 密码
                    .threshold.authmode = WIFI_AUTH_WPA2_PSK, // 最低认证模式要求

                    .pmf_cfg =
                        {
                            .capable = true,  // 是否支持 PMF（Protected Management Frames）
                            .required = false, // 是否强制要求 PMF
                        },
                },
        };

    // 保存上层传入的回调，以便在获取 IP 后通知上层
    wifi_cb = f;

    // 6) 设置工作模式为 STA，写入配置并启动 WiFi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));               // 设置为 Station 模式
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config)); // 应用配置
    ESP_ERROR_CHECK(esp_wifi_start());                               // 启动 WiFi 子系统

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    return ESP_OK;
}