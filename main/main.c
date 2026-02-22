#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "Wifi_STA.h"
#include "DHT11.h"
#include "driver/gpio.h"
#include "esp_task_wdt.h"
#include "oled.h"
#include "bmp.h"
#include "Mqtt/Mqtt.h"

static const char *TAG = "main";

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS出现错误，执行擦除
        ESP_ERROR_CHECK(nvs_flash_erase());
        // 重新尝试初始化
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "[APP] APP Is Start!~\r\n");
    ESP_LOGI(TAG, "[APP] IDF Version is %d.%d.%d", ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH);
    ESP_LOGI(TAG, "[APP] Free memory: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_rom_gpio_pad_select_gpio(DHT11_PIN);

    s_wifi_ev = xEventGroupCreate();
    EventBits_t ev = 0;

    OLED_Init();
    OLED_Clear();

    // 初始化WIFI，传入回调函数，用于通知连接成功事件
    wifi_sta_init(wifi_event_handler);

    // 一直监听WIFI连接事件，直到WiFi连接成功后，才启动MQTT连接
    ev = xEventGroupWaitBits(s_wifi_ev, WIFI_CONNECT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
    if (ev & WIFI_CONNECT_BIT)
    {
        mqtt_start();
    }

    xTaskCreatePinnedToCore(DataReport_Task, "DataReport", 2048, NULL, 4, NULL, 1);
    // xTaskCreatePinnedToCore(DHT11Get_Task, "DHT11Get", 2048, NULL, 3, NULL, 1);
    // xTaskCreatePinnedToCore(OledDisplay_Task, "OledDisplay", 2048, NULL, 3, NULL, 1);

    return;
}
