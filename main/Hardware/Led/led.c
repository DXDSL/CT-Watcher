#include "led.h"
#include "led_strip.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_oled.h" // 引入全局状态变量 is_armed 和 is_alarming

static const char *TAG = "WS2812B";

// 定义 LED 灯带的句柄
static led_strip_handle_t led_strip;

/**
 * @brief 初始化 WS2812B (使用 RMT 硬件驱动)
 */
void WS2812B_Init(void) {
    ESP_LOGI(TAG, "Initializing WS2812B on GPIO %d", LED_STRIP_GPIO);

    // 👈 完美适配 3.0.3 版本的全新结构体字段和宏定义
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = LED_STRIP_LED_NUM,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // 使用全新的字段名和宏
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, 
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
}

/**
 * @brief 设置这颗灯珠的颜色
 * @param red 红色亮度 (0-255)
 * @param green 绿色亮度 (0-255)
 * @param blue 蓝色亮度 (0-255)
 */
void WS2812B_SetColor(uint8_t red, uint8_t green, uint8_t blue) {
    // 设置第 0 颗灯珠的颜色 (因为我们只有 1 颗灯，索引是 0)
    led_strip_set_pixel(led_strip, 0, red, green, blue);
    // 将数据推送到物理灯珠上
    led_strip_refresh(led_strip);
}

/**
 * @brief LED 状态指示后台任务
 * @details 独立运行，根据系统的全局状态自动改变灯光效果
 */
void Led_Task(void *pvParameters) {
    // 1. 初始化灯带
    WS2812B_Init();
    
    bool flash_toggle = false; // 用于控制红蓝爆闪的切换标志

    while (1) {
        if (is_armed) {
            if (is_alarming) {
                // ----- 状态 1：布防且触发警报 (红蓝高频爆闪) -----
                if (flash_toggle) {
                    WS2812B_SetColor(255, 0, 0); // 纯红全亮
                } else {
                    WS2812B_SetColor(0, 0, 255); // 纯蓝全亮
                }
                flash_toggle = !flash_toggle;
                vTaskDelay(pdMS_TO_TICKS(100)); // 100ms 极速切换
            } else {
                // ----- 状态 2：布防但安全 (微弱绿灯长亮，表示系统正在警戒) -----
                WS2812B_SetColor(0, 10, 0); // 亮度给 10，低功耗且不刺眼
                vTaskDelay(pdMS_TO_TICKS(500)); 
            }
        } else {
            // ----- 状态 3：撤防状态 (微弱黄灯长亮) -----
            // 👇 修改这里：红色和绿色混合产生黄色，蓝光关掉 (10, 10, 0)
            WS2812B_SetColor(10, 10, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}