#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "bsp_Audio.h"
#include "bsp_HumanIR.h"
#include "mqtt.h"
#include "ble_prov.h"
#include "web_config.h"
#include "bsp_oled.h"

static const char *TAG = "MAIN";

// ================= 系统核心业务状态 =================
bool is_armed = true;
bool pir_state = false;
#define MP3_FILE_PATH "/spiffs/people.mp3"

// ================= 安防与音频任务 =================
void pir_audio_task(void *pvParameters)
{
    ESP_LOGI(TAG, "PIR Sensor Task started.");
    while (1) {
        pir_state = Get_HumanIR();
        if (is_armed == true && pir_state == 1) {
            ESP_LOGI(TAG, "Motion Detected! Starting playback...");
            is_alarming = true; // 触发屏幕警告
            play_mp3(MP3_FILE_PATH);
            is_alarming = false; // 关闭屏幕警告
            vTaskDelay(pdMS_TO_TICKS(1000)); 
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // 1. 初始化屏幕并拉起 UI 任务
    init_oled_u8g2(); 
    xTaskCreate(OLED_Display_Task, "OLED_Task", 8192, NULL, 4, NULL); 

    // 此时屏幕会显示 4 个空的 [ ]
    boot_step = 0; 
    vTaskDelay(pdMS_TO_TICKS(400)); // 让用户看清初始自检画面

    // 2. 挂载系统文件
    ESP_LOGI(TAG, "Mounting SPIFFS...");
    if (bsp_spiffs_mount() != ESP_OK) {
        return; 
    }
    boot_step = 1; // 👈 此时第 1 行打上 [√]
    vTaskDelay(pdMS_TO_TICKS(400));

    // 3. 初始化人体红外传感器
    HumanIR_Init();
    boot_step = 2; // 👈 此时第 2 行打上 [√]
    vTaskDelay(pdMS_TO_TICKS(400));

    // 4. 初始化网络服务
    ESP_LOGI(TAG, "Initializing WiFi...");
    app_wifi_prov_start();
    start_mqtt_web_server();
    mqtt_start();
    boot_step = 3; // 👈 此时第 3 行打上 [√]
    vTaskDelay(pdMS_TO_TICKS(400));

    // 5. 初始化音频模块
    ESP_LOGI(TAG, "Initializing I2S...");
    i2s_init();
    boot_step = 4; // 👈 此时第 4 行打上 [√]，全部通过！
    
    // 强制停留 800 毫秒，向用户展示完美的“全绿”自检清单
    vTaskDelay(pdMS_TO_TICKS(800)); 

    // 6. 切换到正常仪表盘界面并启动后台任务
    current_ui_mode = UI_MODE_NORMAL; 

    xTaskCreate(pir_audio_task, "pir_audio_task", 8192, NULL, 5, NULL);
    xTaskCreate(DataReport_Task, "DataReport_Task", 4096, NULL, 5, NULL);
}