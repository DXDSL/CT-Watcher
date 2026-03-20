#ifndef __LED_H__
#define __LED_H__

#include <stdint.h>

// 定义 WS2812B 连接的引脚和灯珠数量
#define LED_STRIP_GPIO 48
#define LED_STRIP_LED_NUM 1

// 基础 API
void WS2812B_Init(void);
void WS2812B_SetColor(uint8_t red, uint8_t green, uint8_t blue);

// 后台状态指示任务
void Led_Task(void *pvParameters);

#endif