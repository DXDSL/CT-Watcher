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
#include "led.h"

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

// ================= 主函数 =================
void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // 1. 优先初始化屏幕并拉起 UI 任务
    init_oled_u8g2(); 
    xTaskCreate(OLED_Display_Task, "OLED_Task", 8192, NULL, 4, NULL); 
    vTaskDelay(pdMS_TO_TICKS(500)); // 让用户看一眼全部为空 [ ] 的初始界面

    // 2. 挂载系统文件 (SPIFFS)
    boot_states[0] = STEP_IN_PROGRESS; // 设为执行中 (屏幕开始闪烁 [>])
    vTaskDelay(pdMS_TO_TICKS(200));    // 略微延时，确保 UI 刷新出动画
    if (bsp_spiffs_mount() != ESP_OK) {
        boot_states[0] = STEP_FAILED;  // 设为失败
        // 发生致命错误，死循环挂起，保留屏幕错误画面供排查
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); } 
    }
    boot_states[0] = STEP_SUCCESS;     // 设为成功

    // 3. 初始化人体红外传感器
    boot_states[1] = STEP_IN_PROGRESS;
    vTaskDelay(pdMS_TO_TICKS(200));
    HumanIR_Init();
    boot_states[1] = STEP_SUCCESS;

    // 4. 初始化网络服务 (如果这里有阻塞配网，屏幕会一直完美闪烁动画！)
    boot_states[2] = STEP_IN_PROGRESS;
    vTaskDelay(pdMS_TO_TICKS(200));
    app_wifi_prov_start();
    start_mqtt_web_server();
    mqtt_start();
    boot_states[2] = STEP_SUCCESS;

    // 5. 初始化音频模块
    boot_states[3] = STEP_IN_PROGRESS;
    vTaskDelay(pdMS_TO_TICKS(200));
    i2s_init();
    // 假设 i2s 初始化失败，你也可以像 SPIFFS 那样加上条件判断，这里默认成功
    boot_states[3] = STEP_SUCCESS; 

    // 6. 全部完成，展示完美的“全绿”打勾清单
    vTaskDelay(pdMS_TO_TICKS(1000)); 

    // 7. 切换到正常仪表盘界面并启动后台业务任务
    is_system_initialized = true; 
    current_ui_mode = UI_MODE_NORMAL;

    xTaskCreate(pir_audio_task, "pir_audio_task", 8192, NULL, 5, NULL);
    xTaskCreate(DataReport_Task, "DataReport_Task", 4096, NULL, 5, NULL);
    xTaskCreate(Led_Task, "Led_Task", 4096, NULL, 4, NULL); // 启动 LED 任务
}