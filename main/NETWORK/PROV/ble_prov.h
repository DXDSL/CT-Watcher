#ifndef __BLE_PROV_H__
#define __BLE_PROV_H__

#include <stdbool.h>

// 存放从 NVS 读取的 MQTT 配置
typedef struct {
    char mqtt_uri[64];
    char mqtt_devid[32];  // Client ID / Username
    char mqtt_pwd[64];    // 特征串 Password
    char mqtt_prodid[32]; // 产品ID
} device_mqtt_config_t;

extern device_mqtt_config_t g_mqtt_cfg;

/**
 * @brief 启动 BLE 配网或直接连接已保存的 WiFi
 * 此函数会阻塞，直到设备成功获取到 IP 地址
 */
void app_wifi_prov_start(void);

/**
 * @brief 从 NVS 加载 MQTT 配置
 */
bool load_mqtt_config(void);

#endif