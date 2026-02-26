/*
 * 立创开发板软硬件资料与相关扩展板软硬件资料官网全部开源
 * 开发板官网：www.lckfb.com
 * 技术支持常驻论坛，任何技术问题欢迎随时交流学习
 * 立创论坛：club.szlcsc.com
 * 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
 * 不靠卖板赚钱，以培养中国工程师为己任
 * Change Logs:
 * Date           Author       Notes
 * 2024-01-04     LCKFB-lp    first version
 */

#ifndef _BSP_HUMANIR_H_
#define _BSP_HUMANIR_H_
 
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include <inttypes.h>
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "rom/ets_sys.h"
#include "esp_system.h"
#include "driver/gpio.h"
 
 
#define HUMANIR_PIN    1


void delay_ms(unsigned int ms);
void HumanIR_Init(void);
bool Get_HumanIR(void);
void HumanIR_Task(void *arg);

#endif