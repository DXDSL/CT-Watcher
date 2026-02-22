#ifndef __MQTT_H__
#define __MQTT_H__

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "./Hardware/STA/Wifi_STA.h"
#include "mqtt_client.h"
#include "DHT11.h"

// 定义一个事件组，用于通知main函数WIFI连接成功
#define WIFI_CONNECT_BIT BIT0
extern EventGroupHandle_t s_wifi_ev;

void mqtt_start(void);
void wifi_event_handler(WIFI_EV_e ev);
void DataReport_Task(void *pvParameters);

#endif