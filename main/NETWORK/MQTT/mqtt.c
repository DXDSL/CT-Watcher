#include "Mqtt.h"
#include "ble_prov.h"
#include "mqtt_client.h"
#include "DHT11.h"

static const char* TAG = "MQTT";

// #define MQTT_USERNAME "17291772"                                    // 修改为平台上的"产品ID"
// #define MQTT_CLIENT "172917721"                                     // 请在平台"设备列表"里找到具体的那台设备，复制它的"设备ID"填在这里
// #define MQTT_ADDRESS "mqtt://2000506597.non-nb.ctwing.cn"          // 修改为平台上的"产品ID"对应的"连接地址"
// #define MQTT_PORT   1883
// #define MQTT_PASSWORD "04d85_5O6kRYt6thxVwRH2fo5nw52nu_7uRiC4jqNe4"            // 特征串

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
// 检查是否为空，如果为空可以给一个默认值防止指针越界
    if (strlen(g_mqtt_cfg.mqtt_uri) == 0) {
        ESP_LOGE("MQTT", "MQTT 参数为空，跳过连接！");
        return;
    }

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = g_mqtt_cfg.mqtt_uri,
        .credentials.client_id = g_mqtt_cfg.mqtt_devid,
        .credentials.username = g_mqtt_cfg.mqtt_devid, // username 和 client_id 通常一样
        .credentials.authentication.password = g_mqtt_cfg.mqtt_pwd,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, aliot_mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);
}


extern bool is_armed;         // 当前是否处于布防模式
extern bool pir_state;    // PIR 是否检测到人 (可以通过按键或传感器改变)

void DataReport_Task(void *pvParameters)
{
    // 1. 去掉 static，放在栈上，并将容量稍微扩大以应对后续加入更多字段
    char mqtt_pub_buff[256]; 
    
    // 用于记录上一次成功发送的数据，用来做“变化阈值对比”
    uint8_t last_temp = 0xFF; 
    uint8_t last_humi = 0xFF;
    bool last_pir_state = false;
    
    // 记录时间，用于心跳包计算
    uint32_t last_report_time = 0; 
    const uint32_t HEARTBEAT_INTERVAL = 60000; // 60秒心跳一次 (毫秒)

    vTaskDelay(pdMS_TO_TICKS(1000)); // 等待传感器初始化稳定

    while (1)
    {
        if (s_is_mqtt_connected)
        {
            bool need_report = false;
            uint32_t current_time = pdTICKS_TO_MS(xTaskGetTickCount());

            // 条件 A：距离上次上报超过了 60 秒 (定时心跳包)
            if ((current_time - last_report_time) >= HEARTBEAT_INTERVAL) {
                need_report = true;
            }
            // 条件 B：温度变化 >= 1度，或湿度变化 >= 2% (数据剧烈变化)
            else if (abs(Temp - last_temp) >= 1 || abs(Humi - last_humi) >= 2) {
                need_report = true;
            }
            // 条件 C：PIR 状态发生了翻转 (例如突然有人闯入)
            else if (pir_state != last_pir_state) {
                need_report = true;
            }

            // 如果满足上述任意一个条件，则打包发送
            if (need_report)
            {
                // 2. 使用 sizeof 自动获取缓冲大小，加入更多的业务字段
                snprintf(mqtt_pub_buff, sizeof(mqtt_pub_buff), 
                         "{\"temp\":%d,\"humi\":%d,\"pir\":%d,\"armed\":%d}", 
                         Temp, Humi, pir_state ? 1 : 0, is_armed ? 1 : 0);

                // 3. 检查 API 的返回值
                int msg_id = esp_mqtt_client_publish(s_mqtt_client, MQTT_data_report_TOPIC,
                                                     mqtt_pub_buff, strlen(mqtt_pub_buff), 1, 0);
                
                if (msg_id != -1) {
                    ESP_LOGI("MQTT_PUB", "Report Success! ID:%d, Data:%s", msg_id, mqtt_pub_buff);
                    // 发送成功后，更新记录值，用于下一次的对比
                    last_temp = Temp;
                    last_humi = Humi;
                    last_pir_state = pir_state;
                    last_report_time = current_time;
                } else {
                    ESP_LOGE("MQTT_PUB", "Report Failed! Buffer full or disconnected.");
                }
            }
        }

        // 任务的轮询周期可以保持 100ms 或 500ms（响应突发事件），但不再是每轮都无脑上报了
        vTaskDelay(pdMS_TO_TICKS(500));
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