#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "bsp_Audio.h"

static const char *TAG = "MAIN";

// 定义音乐路径和人体传感器引脚
#define MP3_FILE_PATH "/spiffs/Canon.mp3"
#define PIR_SENSOR_PIN 4 

// 独立的工作任务：检测传感器并播放音频
void pir_audio_task(void *pvParameters)
{
    ESP_LOGI(TAG, "PIR Sensor Task started. Waiting for motion...");

    while (1) {
        // 读取 GPIO 4 的状态 (1 为有人，0 为无人)
        int pir_state = gpio_get_level(PIR_SENSOR_PIN);

        if (pir_state == 1) {
            ESP_LOGI(TAG, "Motion Detected! (检测到人) - Starting playback...");
            
            // 播放音乐（此函数现在会一直阻塞直到播放完毕，中途不会死机）
            play_mp3(MP3_FILE_PATH);
            
            ESP_LOGI(TAG, "Playback finished. Waiting for next trigger...");
            
            // 播放完毕后强制延时 3 秒，防止传感器自带的高电平延时导致紧接着重复播放
            vTaskDelay(pdMS_TO_TICKS(3000)); 
        }

        // 每次检测间隔 100 毫秒，喂狗并让出 CPU 给其他任务
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    // 1. 初始化 NVS (为了系统的稳定性，强烈建议加上)
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

    // 3. 初始化 I2S 并注册解码器
    ESP_LOGI(TAG, "Initializing I2S...");
    i2s_init();

    // 4. 初始化人体感应传感器 (将引脚配置为带下拉的输入模式，防干扰)
    ESP_LOGI(TAG, "Initializing PIR Sensor on GPIO %d...", PIR_SENSOR_PIN);
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIR_SENSOR_PIN),
        .mode = GPIO_MODE_INPUT,                  
        .pull_up_en = GPIO_PULLUP_DISABLE,        
        .pull_down_en = GPIO_PULLDOWN_ENABLE,     
        .intr_type = GPIO_INTR_DISABLE            
    };
    gpio_config(&io_conf);

    // 5. 创建任务，分配 8192 字节的栈空间（MP3 解码非常消耗内存栈）
    xTaskCreate(pir_audio_task, "pir_audio_task", 8192, NULL, 5, NULL);
    
    // 主函数执行完毕，系统由后台的 pir_audio_task 接管
}