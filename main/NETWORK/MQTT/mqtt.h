#ifndef __MQTT_H__
#define __MQTT_H__


// 定义一个事件组，用于通知main函数WIFI连接成功
#define WIFI_CONNECT_BIT BIT0


void mqtt_start(void);
void DataReport_Task(void *pvParameters);
void mqtt_publish_alarm(void);

#endif