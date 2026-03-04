#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"
#include "cJSON.h"
#include "ble_prov.h"

static const char *TAG = "BLE_PROV";
device_mqtt_config_t g_mqtt_cfg = {0};
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_EVENT = BIT0;

// 自定义数据端点回调函数：解析手机下发的 MQTT JSON
static esp_err_t custom_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen, 
                                          uint8_t **outbuf, ssize_t *outlen, void *priv_data) 
{
    if (inbuf) {
        ESP_LOGI(TAG, "收到手机 APP 下发的自定义数据: %.*s", inlen, (char *)inbuf);
        cJSON *root = cJSON_Parse((const char *)inbuf);
        if (root) {
            nvs_handle_t my_handle;
            nvs_open("storage", NVS_READWRITE, &my_handle);

            cJSON *uri = cJSON_GetObjectItem(root, "uri");
            if (uri) nvs_set_str(my_handle, "mqtt_uri", uri->valuestring);
            
            cJSON *devid = cJSON_GetObjectItem(root, "devid");
            if (devid) nvs_set_str(my_handle, "mqtt_devid", devid->valuestring);
            
            cJSON *pwd = cJSON_GetObjectItem(root, "mqpass");
            if (pwd) nvs_set_str(my_handle, "mqtt_pwd", pwd->valuestring);

            nvs_commit(my_handle);
            nvs_close(my_handle);
            cJSON_Delete(root);
            ESP_LOGI(TAG, "MQTT 参数已安全保存至 NVS！");
        }
    }
    // 必须回复 APP，否则 APP 会报错
    char response[] = "SUCCESS";
    *outbuf = (uint8_t *)strdup(response);
    *outlen = strlen(response);
    return ESP_OK;
}

// 事件回调
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START: ESP_LOGI(TAG, "配网启动，请打开手机 APP"); break;
            case WIFI_PROV_CRED_RECV: ESP_LOGI(TAG, "收到 WiFi 账号密码，正在连接..."); break;
            case WIFI_PROV_END: ESP_LOGI(TAG, "配网完成！"); break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
    }
}

// 加载 NVS 里的 MQTT 参数
bool load_mqtt_config(void) {
    nvs_handle_t my_handle;
    if (nvs_open("storage", NVS_READONLY, &my_handle) != ESP_OK) return false;
    
    size_t len = sizeof(g_mqtt_cfg.mqtt_uri);
    nvs_get_str(my_handle, "mqtt_uri", g_mqtt_cfg.mqtt_uri, &len);
    len = sizeof(g_mqtt_cfg.mqtt_devid);
    nvs_get_str(my_handle, "mqtt_devid", g_mqtt_cfg.mqtt_devid, &len);
    len = sizeof(g_mqtt_cfg.mqtt_pwd);
    nvs_get_str(my_handle, "mqtt_pwd", g_mqtt_cfg.mqtt_pwd, &len);
    
    nvs_close(my_handle);
    return strlen(g_mqtt_cfg.mqtt_uri) > 0;
}

void app_wifi_prov_start(void) {
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 配置配网管理器
    wifi_prov_mgr_config_t manager_config = {
        .scheme = wifi_prov_scheme_ble, // 指定使用蓝牙 BLE
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM // 配网结束后释放蓝牙内存
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(manager_config));

    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned) {
        ESP_LOGI(TAG, "未找到配置，启动 BLE 配网...");
        
        // 关键：注册自定义数据端点，用于接收 MQTT JSON
        wifi_prov_mgr_endpoint_create("custom-data");
        wifi_prov_mgr_endpoint_register("custom-data", custom_prov_data_handler, NULL);
        
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());

        // 设置配网的蓝牙名称和验证密码 (PoP)
        // 为了安全，手机连接该蓝牙时必须输入密码 CT123456
        wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, "CT123456", "CT-Watcher-BLE", NULL);
    } else {
        ESP_LOGI(TAG, "设备已配过网，直接连接 WiFi...");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    }

    // 阻塞等待 WiFi 连接成功
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "网络连接就绪，加载 MQTT 参数...");
    load_mqtt_config();
}