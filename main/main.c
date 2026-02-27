#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "bsp_Audio.h"
#include "bsp_HumanIR.h"
#include "wifi_sta.h"

static const char *TAG = "MAIN";


// 定义音乐路径和人体传感器引脚
#define MP3_FILE_PATH "/spiffs/people.mp3"

// 独立的工作任务：检测传感器并播放音频
void pir_audio_task(void *pvParameters)
{
    ESP_LOGI(TAG, "PIR Sensor Task started. Waiting for motion...");

    while (1) {
        // 读取 人体传感器 的状态 (0 为有人，1 为无人)
        bool pir_state = Get_HumanIR();

        if (pir_state == 0) {
            ESP_LOGI(TAG, "Motion Detected! (检测到人) - Starting playback...");
            
            // 播放音乐（此函数现在会一直阻塞直到播放完毕，中途不会死机）
            play_mp3(MP3_FILE_PATH);
            ESP_LOGI(TAG, "Playback finished. Waiting for next trigger...");
            
            // 播放完毕后强制延时 1 秒，防止传感器着重复播放
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
        
        // 以后你的 MQTT 初始化就可以写在这里
        // mqtt_app_start(); 
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

    // 2. 挂载 SPIFFS
    ESP_LOGI(TAG, "Mounting SPIFFS...");
    if (bsp_spiffs_mount() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS");
        return;
    }

    wifi_sta_init(ESP32_wifi_event_callback); // 初始化 WIFI 连接

    // 3. 初始化人体感应传感器
    HumanIR_Init();

    // 4. 初始化 I2S 并注册解码器
    ESP_LOGI(TAG, "Initializing I2S...");
    i2s_init();

    // 5. 创建任务，分配 8192 字节的栈空间（MP3 解码非常消耗内存栈）
    xTaskCreate(pir_audio_task, "pir_audio_task", 8192, NULL, 5, NULL);
}