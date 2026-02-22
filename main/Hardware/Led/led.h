#ifndef __LED_H__
#define __LED_H__

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_attr.h"

//设置LED引脚
#define  LED_PIN  48

/**
 * @brief   LED初始化
 * 
 */
void Led_Init(void);

/**
 * @brief       设置LED亮
 * 
 */
void LedOn(void);

/**
 * @brief       设置LED灭
 * 
 */
void LedOff(void);



void Ledc_Init(void);
void Ledc_cb_Init(void);
IRAM_ATTR bool ledc_finish_cb(const ledc_cb_param_t *param, void *user_arg);

#endif
 