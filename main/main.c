#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "bsp_Audio.h"
#include "bsp_HumanIR.h"
#include "mqtt.h"

static const char *TAG = "MAIN";

// ================= 全局状态变量定义 =================
bool is_armed = true;       // 默认上电后开启检测模式
bool pir_state = false; // 默认没有检测到人
// ===================================================

// 定义音乐路径和人体传感器引脚
#define MP3_FILE_PATH "/spiffs/people.mp3"

// 独立的工作任务：检测传感器并播放音频
void pir_audio_task(void *pvParameters)
{
    ESP_LOGI(TAG, "PIR Sensor Task started. Waiting for motion...");

    while (1) {
        // 读取 人体传感器 的状态 (1 为有人，0 为无人)
        pir_state = Get_HumanIR();

        // 同时判断 "是否处于检测模式" AND "是否检测到人"
        if (is_armed == true && pir_state == 1) {
            ESP_LOGI(TAG, "Motion Detected! (检测到人) - Starting playback...");
            
            // --- 这里可以加入 WS2812B 视觉报警 (可选) ---
            // Led_SetColor(255, 0, 0); // 亮红灯
            
            // 听觉报警：播放音乐（阻塞直到播放完毕）
            play_mp3(MP3_FILE_PATH);
            
            ESP_LOGI(TAG, "Playback finished. Waiting for next trigger...");
            
            // --- 报警结束，恢复正常指示灯 (可选) ---
            // Led_SetColor(0, 255, 0); // 亮绿灯
            
            // 播放完毕后强制延时 1 秒，防止传感器重复触发
            vTaskDelay(pdMS_TO_TICKS(1000)); 
        }

        // 每次检测间隔 100 毫秒，喂狗并让出 CPU 给其他任务
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ==========================================
// 定义 WiFi 事件回调函数
// 当底层获取到 IP 地址后，会调用这个函数
// ==========================================
void ESP32_wifi_event_callback(WIFI_EV_e status)
{
    if (status == WIFI_CONNECTED) {
        ESP_LOGI(TAG, "🟢 WiFi Connected successfully! (WiFi连接成功，已获取IP)");
        
        // 通知 mqtt.c 网络已就绪，置位事件标志
        wifi_event_handler(status);

        // 启动 MQTT
        ESP_LOGI(TAG, "WiFi ready! Starting MQTT...");
        mqtt_start();
    }
}

void app_main(void)
{
    // 1. 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // 在初始化 WiFi 和 MQTT 之前，必须先创建事件组！
    s_wifi_ev = xEventGroupCreate();
    if (s_wifi_ev == NULL) {
        ESP_LOGE(TAG, "Failed to create WiFi event group!");
        return;
    }

    // 2. 挂载 SPIFFS
    ESP_LOGI(TAG, "Mounting SPIFFS...");
    if (bsp_spiffs_mount() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS");
        return;
    }

    // 3. 初始化网络模块
    ESP_LOGI(TAG, "Initializing WiFi...");
    wifi_sta_init(ESP32_wifi_event_callback); // 初始化 WIFI 连接

    // 4. 初始化人体感应传感器
    HumanIR_Init();

    // 5. 初始化 I2S 并注册解码器
    ESP_LOGI(TAG, "Initializing I2S...");
    i2s_init();

    // 6. 创建检测传感器并播放音频的任务
    xTaskCreate(pir_audio_task, "pir_audio_task", 8192, NULL, 5, NULL);

    // 7. 创建 MQTT 数据循环上报任务 
    xTaskCreate(DataReport_Task, "DataReport_Task", 4096, NULL, 5, NULL);
}