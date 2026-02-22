#include "led.h"

// 配置输出寄存器
#define GPIO_OUTPUT_PIN_SEL (1ULL << LED_PIN)

/**
 * @函数说明        LED的初始化
 *
 */
void Led_Init(void)
{
    gpio_config_t gpio_init_struct = {0};

    // 配置IO为通用IO
    esp_rom_gpio_pad_select_gpio(LED_PIN);

    gpio_init_struct.intr_type = GPIO_INTR_DISABLE;        // 不使用中断
    gpio_init_struct.mode = GPIO_MODE_OUTPUT;              // 输出模式
    gpio_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;      // 使能上拉模式
    gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE; // 失能下拉模式
    gpio_init_struct.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;   // 使用GPIO9输出寄存器

    // 将以上参数配置到引脚
    gpio_config(&gpio_init_struct);

    // 设置引脚输出高电平，默认不让LED亮
    gpio_set_level(LED_PIN, 1);
}

/**
 * @函数说明        设置LED亮
 *
 */
void LedOn(void)
{
    gpio_set_level(LED_PIN, 0);
}

/**
 * @函数说明        设置LED灭
 *
 */
void LedOff(void)
{
    gpio_set_level(LED_PIN, 1);
}

void Ledc_Init(void)
{
    ledc_timer_config_t ledcTim_init_struct = {
        // 速度模式，低速度模式
        .speed_mode = LEDC_LOW_SPEED_MODE,
        // 根据应用程序的需求选择 duty 分辨率。
        .duty_resolution = LEDC_TIMER_12_BIT,
        // 定时器编号，根据 ESP32 的要求设置。
        .timer_num = LEDC_TIMER_0,
        // 设置定时器频率（Hz）。
        .freq_hz = 5 * 1000, // 5 kHz
        // 配置 LEDC 定时器的时钟源。
        .clk_cfg = LEDC_AUTO_CLK, // 时钟源
        // 如果你想要配置 LEDC 定时器，请将该字段设置为 false。如果你将其设置为 true，定时器将不会被配置，并且 duty_resolution、freq_hz、clk_cfg 字段将被忽略。
        .deconfigure = false,
    };
    ledc_timer_config(&ledcTim_init_struct);

    ledc_channel_config_t ledcCha_init_struct = {
        .channel = LEDC_CHANNEL_0,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .gpio_num = LED_PIN,
        .duty = 0,
        .intr_type = LEDC_INTR_DISABLE,
    };
    ledc_channel_config(&ledcCha_init_struct);
}

void Ledc_cb_Init(void)
{
    ledc_cbs_t cbs = {
        .fade_cb = ledc_finish_cb,
    };
    ledc_cb_register(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, &cbs, NULL);
}