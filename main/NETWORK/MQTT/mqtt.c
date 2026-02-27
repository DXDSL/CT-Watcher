#include "Mqtt.h"

static const char* TAG = "MQTT";

#define MQTT_USERNAME "17291772"                                    // 修改为平台上的"产品ID"
#define MQTT_CLIENT "172917721"                                     // 请在平台"设备列表"里找到具体的那台设备，复制它的"设备ID"填在这里
#define MQTT_ADDRESS "mqtt://2000506597.non-nb.ctwing.cn"          // 修改为平台上的"产品ID"对应的"连接地址"
#define MQTT_PORT   1883
#define MQTT_PASSWORD "04d85_5O6kRYt6thxVwRH2fo5nw52nu_7uRiC4jqNe4"            // 特征串

#define MQTT_data_report_TOPIC      "data_report"             
#define MQTT_SUBSCRIBE_TOPIC        "device_control" 

EventGroupHandle_t s_wifi_ev = NULL;

// MQTT客户端操作句柄
static esp_mqtt_client_handle_t s_mqtt_client = NULL;

// MQTT连接标志
static bool s_is_mqtt_connected = false;

/**
 * mqtt连接事件处理函数
 * @param event 事件参数
 * @return 无
 */
static void aliot_mqtt_event_handler(void *event_handler_arg,
                                     esp_event_base_t event_base,
                                     int32_t event_id,
                                     void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    // esp_mqtt_client_handle_t client = event->client;

    // your_context_t *context = event->context;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED: // 连接成功
        ESP_LOGI(TAG, "mqtt connected");
        s_is_mqtt_connected = true;
        // 连接成功后，订阅测试主题
        esp_mqtt_client_subscribe_single(s_mqtt_client, MQTT_SUBSCRIBE_TOPIC, 1);
        break;
    case MQTT_EVENT_DISCONNECTED: // 连接断开
        ESP_LOGI(TAG, "mqtt disconnected");
        s_is_mqtt_connected = false;
        break;
    case MQTT_EVENT_SUBSCRIBED: // 收到订阅消息ACK
        ESP_LOGI(TAG, "mqtt subscribed ack, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED: // 收到解订阅消息ACK
        break;
    case MQTT_EVENT_PUBLISHED: // 收到发布消息ACK
        ESP_LOGI(TAG, "mqtt publish ack, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic); // 收到Pub消息直接打印出来
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        break;
    }
}

/** 启动mqtt连接
 * @param 无
 * @return 无
 */
void mqtt_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {0};
    mqtt_cfg.broker.address.uri = MQTT_ADDRESS;
    mqtt_cfg.broker.address.port = MQTT_PORT;
    // Client ID
    mqtt_cfg.credentials.client_id = MQTT_CLIENT;
    // 用户名
    mqtt_cfg.credentials.username = MQTT_USERNAME;
    // 密码
    mqtt_cfg.credentials.authentication.password = MQTT_PASSWORD;
    ESP_LOGI(TAG, "mqtt connect->clientId:%s,username:%s,password:%s", mqtt_cfg.credentials.client_id,
             mqtt_cfg.credentials.username, mqtt_cfg.credentials.authentication.password);
    // 设置mqtt配置，返回mqtt操作句柄
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    // 注册mqtt事件回调函数
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, aliot_mqtt_event_handler, s_mqtt_client);
    // 启动mqtt连接
    esp_mqtt_client_start(s_mqtt_client);
}

/** wifi事件通知
 * @param 无
 * @return 无
 */
void wifi_event_handler(WIFI_EV_e ev)
{
    if (ev == WIFI_CONNECTED)
    {
        xEventGroupSetBits(s_wifi_ev, WIFI_CONNECT_BIT);
    }
}

void DataReport_Task(void *pvParameters)
{
    static char mqtt_pub_buff[128];
    vTaskDelay(pdMS_TO_TICKS(100));
    while (1)
    {
        // 延时2秒发布一条消息到 MQTT_PUBLIC_TOPIC主题
        if (s_is_mqtt_connected)
        {
            snprintf(mqtt_pub_buff, 128, "{\"temperature_data\":%d,\"humidity_data\":%d}", Temp, Humi);
            // snprintf(mqtt_pub_buff, 128, "{\"temperature_data\":%d}", Temp);
            esp_mqtt_client_publish(s_mqtt_client, MQTT_data_report_TOPIC,
                                    mqtt_pub_buff, strlen(mqtt_pub_buff), 1, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


void mqtt_publish_alarm(void) 
{
    if (s_is_mqtt_connected && s_mqtt_client != NULL) 
    {
        char *alarm_json = "{\"motion_detected\":1}"; // 构造 JSON 告警数据
        esp_mqtt_client_publish(s_mqtt_client, MQTT_data_report_TOPIC, alarm_json, strlen(alarm_json), 1, 0);
    }
}