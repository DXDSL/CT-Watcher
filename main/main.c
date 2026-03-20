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
#include "key.h"

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
    // ==========================================
    // 1. 基础系统初始化 (NVS & UI)
    // ==========================================
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // 优先拉起 OLED 任务，接管系统后续所有的状态显示
    init_oled_u8g2(); 
    xTaskCreate(OLED_Display_Task, "OLED_Task", 8192, NULL, 4, NULL); 
    vTaskDelay(pdMS_TO_TICKS(500)); // 预留时间展示初始空 Checklist [ ]

    // ==========================================
    // 2. 核心硬件与文件系统自检
    // ==========================================
    // [步骤 0] 挂载 SPIFFS 文件系统 (用于 MP3 音频和中文字库)
    boot_states[0] = STEP_IN_PROGRESS; 
    vTaskDelay(pdMS_TO_TICKS(200));    
    if (bsp_spiffs_mount() != ESP_OK) {
        boot_states[0] = STEP_FAILED;  
        ESP_LOGE("MAIN", "SPIFFS Mount Failed! System Halted.");
        // 发生致命错误，挂起(销毁)当前主任务，保留 OLED 错误画面供排查
        vTaskDelete(NULL); 
    }
    boot_states[0] = STEP_SUCCESS;     

    // [步骤 1] 初始化人体红外传感器
    boot_states[1] = STEP_IN_PROGRESS;
    vTaskDelay(pdMS_TO_TICKS(200));
    HumanIR_Init();
    boot_states[1] = STEP_SUCCESS;

    // ==========================================
    // 3. 网络与云平台初始化
    // ==========================================
    // [步骤 2] 启动配网、Web 服务及 MQTT 客户端
    boot_states[2] = STEP_IN_PROGRESS;
    vTaskDelay(pdMS_TO_TICKS(200));
    app_wifi_prov_start();
    start_mqtt_web_server();
    mqtt_start();
    boot_states[2] = STEP_SUCCESS;

    // ==========================================
    // 4. 音频系统初始化
    // ==========================================
    // [步骤 3] 初始化 I2S 音频功放
    boot_states[3] = STEP_IN_PROGRESS;
    vTaskDelay(pdMS_TO_TICKS(200));
    i2s_init();
    boot_states[3] = STEP_SUCCESS; 

    // ==========================================
    // 5. 自检完成，系统业务拉起
    // ==========================================
    vTaskDelay(pdMS_TO_TICKS(1000)); 

    // 解锁系统安全锁并切换至主看板
    is_system_initialized = true; 
    current_ui_mode = UI_MODE_NORMAL;

    ESP_LOGI("MAIN", "System Initialized Successfully. Starting Business Tasks...");

    // 启动所有后台常驻业务任务
    xTaskCreate(pir_audio_task,"pir_audio_task",8192, NULL, 5, NULL); // 安防与音频告警任务
    xTaskCreate(DataReport_Task,"DataReport_Task",4096, NULL, 5, NULL); // MQTT 云端通信任务
    // xTaskCreate(DHT11Get_Task,"DHT11_Task",2048, NULL, 4, NULL); // 温湿度采集任务 (已补回)
    xTaskCreate(Led_Task,"Led_Task",4096, NULL, 4, NULL); // WS2812B 幻彩状态灯任务
    xTaskCreate(Key_Task,"Key_Task",4096, NULL, 4, NULL); // 硬件多按键扫描任务
    
    // app_main 执行完毕后，此初始线程将自动被 FreeRTOS 回收，不再占用栈空间
}