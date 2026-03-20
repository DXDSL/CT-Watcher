#ifndef __KEY_H__
#define __KEY_H__

#include <stdint.h>
#include "driver/gpio.h"

// 硬件引脚定义 (参考 README 映射表)
#define KEY1_PIN GPIO_NUM_4  // 布防/撤防切换
#define KEY2_PIN GPIO_NUM_15 // 待定 (原计划: 测试喇叭)
#define KEY3_PIN GPIO_NUM_16 // 待定 (原计划: SOS)

// 启动按键扫描后台任务
void Key_Task(void *pvParameters);

#endif