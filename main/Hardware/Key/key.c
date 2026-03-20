#include "key.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bsp_oled.h" // 引入全局变量 is_armed 等

static const char *TAG = "KEY";

/**
 * @brief 按键 GPIO 初始化
 */
static void Key_Init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << KEY1_PIN) | (1ULL << KEY2_PIN) | (1ULL << KEY3_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,     // 开启内部上拉，按键未按下时为高电平
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE        // 我们采用高频轮询，不使用中断
    };
    gpio_config(&io_conf);
    ESP_LOGI(TAG, "Hardware Keys Initialized.");
}

/**
 * @brief 按键扫描任务 (非阻塞式状态机防抖)
 */
void Key_Task(void *pvParameters) {
    Key_Init();

    // 每个按键的独立状态机 (0: 未按下, 1: 消抖中, 2: 确认按下, 3: 等待释放)
    uint8_t key1_state = 0;
    uint8_t key2_state = 0;
    uint8_t key3_state = 0;

    while (1) {
        // ==========================================
        // KEY 1: GPIO 4 (本地布防/撤防切换)
        // ==========================================
        if (gpio_get_level(KEY1_PIN) == 0) {
            if (key1_state == 0) {
                key1_state = 1; // 初次检测到低电平，进入消抖
            } else if (key1_state == 1) {
                key1_state = 2; // 20ms后依然是低电平，确认按键按下！
                
                // 👈 核心业务逻辑：翻转布防状态
                is_armed = !is_armed; 
                ESP_LOGW(TAG, "本地按键触发：安防模式已切换为 -> %s", is_armed ? "布防" : "撤防");
            
            } else if (key1_state == 2) {
                key1_state = 3; // 动作已执行，进入死锁等待按键松开（防连发）
            }
        } else {
            key1_state = 0; // 按键一旦松开，重置状态机
        }

        // ==========================================
        // KEY 2: GPIO 15 (待定)
        // ==========================================
        if (gpio_get_level(KEY2_PIN) == 0) {
            if (key2_state == 0) key2_state = 1;
            else if (key2_state == 1) {
                key2_state = 2;
                ESP_LOGI(TAG, "按键 2 被按下 (功能待定)");
            } else if (key2_state == 2) key2_state = 3;
        } else {
            key2_state = 0;
        }

        // ==========================================
        // KEY 3: GPIO 16 (待定)
        // ==========================================
        if (gpio_get_level(KEY3_PIN) == 0) {
            if (key3_state == 0) key3_state = 1;
            else if (key3_state == 1) {
                key3_state = 2;
                ESP_LOGI(TAG, "按键 3 被按下 (功能待定)");
            } else if (key3_state == 2) key3_state = 3;
        } else {
            key3_state = 0;
        }

        // 任务休眠 20 毫秒 (这恰好起到了极其完美的软件滤波消抖作用)
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}