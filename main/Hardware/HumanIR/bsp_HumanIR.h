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
 
#define PIR_PIN    1

void delay_ms(unsigned int ms);
void HumanIR_Init(void);
bool Get_HumanIR(void);

#endif