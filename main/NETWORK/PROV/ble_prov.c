#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_system.h" 
#include "nvs_flash.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"
#include "ble_prov.h"
#include "bsp_oled.h"

static const char *TAG = "BLE_PROV";  // 日志标签，用于 ESP_LOG 输出

// 全局 MQTT 配置结构体
device_mqtt_config_t g_mqtt_cfg = {0};

// Wi-Fi 事件组，用于同步 Wi-Fi 连接状态
static EventGroupHandle_t wifi_event_group;

// Wi-Fi 连接成功事件位
const int WIFI_CONNECTED_EVENT = BIT0;

// 记录 Wi-Fi 断线重试次数
static int s_retry_num = 0; 
// 记录当前是否处于配网模式
static bool s_is_provisioning = false; 

// 将启动蓝牙的重体力活剥离到一个独立的任务中
// 参数：arg - 任务参数（未使用）
static void restart_prov_task(void *arg) {
    ESP_LOGI(TAG, "⚡ 正在独立任务中无缝唤醒蓝牙配网模式...");
    
    // 1. 擦除 NVS 中失效的 Wi-Fi 账密
    wifi_prov_mgr_reset_provisioning();
    
    // 2. 唤醒配网模式
    wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, "abcd1234", "CT-Watcher", NULL);
    
    // 任务执行完毕后自我销毁，释放内存
    vTaskDelete(NULL); 
}

// 事件回调管理器：处理配网、连接、断开、获取IP等所有网络事件
// 参数：arg - 用户参数（未使用），event_base - 事件基础，event_id - 事件ID，event_data - 事件数据
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START: 
                s_is_provisioning = true; 
                ESP_LOGI(TAG, "配网启动，请打开手机 APP"); 
                break;
            case WIFI_PROV_CRED_RECV: 
                ESP_LOGI(TAG, "收到 WiFi 账号密码，正在连接..."); 
                break;
            case WIFI_PROV_END: 
                s_is_provisioning = false; 
                ESP_LOGI(TAG, "配网完成！蓝牙进入后台休眠。"); 
                break;
        }
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_is_provisioning) {
            ESP_LOGW(TAG, "配网连接失败，等待底层通知手机 APP...");
        } else {
            // 日常断网重连逻辑保持不变
            if (s_retry_num < 5) {
                esp_wifi_connect();
                s_retry_num++;
                ESP_LOGI(TAG, "Wi-Fi 连不上或断开了，正在重试 (%d/5)...", s_retry_num);
            } else {
                // 👈 核心逻辑：超过 5 次，放弃自动重连，呼出配网界面！
                ESP_LOGE("WIFI", "连接失败已达 5 次！请重新配网。");

                // 切换屏幕到配网模式
                current_ui_mode = UI_MODE_PROV;

                if(!s_is_provisioning) { 
                    boot_states[2] = STEP_FAILED;// 开机自检阶段失败，也可以将该步骤标记为失败
                }

                if (!s_is_provisioning) { 
                    s_retry_num = 0;
                    s_is_provisioning = true;
                    xTaskCreate(restart_prov_task, "restart_prov", 4096, NULL, 5, NULL);
                }
            }
        }
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "---------------------------------------------");
        ESP_LOGI(TAG, "🌐 Wi-Fi 连接成功！");
        ESP_LOGI(TAG, "📍 设备局域网 IP 地址: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "---------------------------------------------");
        
        // 必须满足系统曾经完美初始化过，才允许切回主界面！
        if (current_ui_mode == UI_MODE_PROV && is_system_initialized == true) {
            current_ui_mode = UI_MODE_NORMAL;
        }
        
        s_retry_num = 0; 
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT); 
    }
}

// 读取 NVS 中的 MQTT 参数
// 返回：true 如果成功读取至少 MQTT URI，否则 false
bool load_mqtt_config(void) {
    nvs_handle_t my_handle;
    if (nvs_open("storage", NVS_READONLY, &my_handle) != ESP_OK) return false;
    
    size_t len = sizeof(g_mqtt_cfg.mqtt_uri);
    nvs_get_str(my_handle, "mqtt_uri", g_mqtt_cfg.mqtt_uri, &len);
    
    len = sizeof(g_mqtt_cfg.mqtt_prodid);
    nvs_get_str(my_handle, "mqtt_prodid", g_mqtt_cfg.mqtt_prodid, &len);
    
    len = sizeof(g_mqtt_cfg.mqtt_devid);
    nvs_get_str(my_handle, "mqtt_devid", g_mqtt_cfg.mqtt_devid, &len);
    
    len = sizeof(g_mqtt_cfg.mqtt_pwd);
    nvs_get_str(my_handle, "mqtt_pwd", g_mqtt_cfg.mqtt_pwd, &len);
    
    nvs_close(my_handle);
    return strlen(g_mqtt_cfg.mqtt_uri) > 0;
}

// 初始化并启动 Wi-Fi 配网服务
// 如果设备未配网，则启动 BLE 配网；如果已配网，则直接连接 Wi-Fi
// 等待 Wi-Fi 连接成功后加载 MQTT 配置
void app_wifi_prov_start(void) {
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_prov_mgr_config_t manager_config = {
        .scheme = wifi_prov_scheme_ble, 
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE 
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(manager_config));

    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned) {
        ESP_LOGI(TAG, "未找到配置，启动 BLE 配网...");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, "abcd1234", "CT-Watcher", NULL);
    } else {
        ESP_LOGI(TAG, "设备已保存过配置，直接尝试连接 WiFi...");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    }

    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "网络环境已就绪，正在加载 MQTT 参数...");
    load_mqtt_config();
}