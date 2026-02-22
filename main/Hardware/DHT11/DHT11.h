#ifndef __DHT11_H__
#define __DHT11_H__

#include "esp32s3/rom/ets_sys.h"
#include <driver/rmt_rx.h>
#include <driver/rmt_tx.h>
#include <soc/rmt_reg.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"

#define DHT11_PIN (38) // 可通过宏定义，修改引脚

#define DHT11_CLR gpio_set_level(DHT11_PIN, 0)
#define DHT11_SET gpio_set_level(DHT11_PIN, 1)
#define DHT11_IN gpio_set_direction(DHT11_PIN, GPIO_MODE_INPUT)
#define DHT11_OUT gpio_set_direction(DHT11_PIN, GPIO_MODE_OUTPUT)

extern uint8_t DHT11Data[4];
extern uint8_t Temp,Humi;

void DHT11_Start(void);
uint8_t DHT11_ReadValue(void);
uint8_t DHT11_ReadTemHum(uint8_t *buf);

#endif