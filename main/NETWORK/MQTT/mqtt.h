#ifndef __MQTT_H__
#define __MQTT_H__

#include "wifi_sta.h"
#include "mqtt_client.h"
#include "DHT11.h"

// 定义一个事件组，用于通知main函数WIFI连接成功
#define WIFI_CONNECT_BIT BIT0
extern EventGroupHandle_t s_wifi_ev;

void mqtt_start(void);
void wifi_event_handler(WIFI_EV_e ev);
void DataReport_Task(void *pvParameters);
void mqtt_publish_alarm(void);

#endif