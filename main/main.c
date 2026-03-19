#include <stdio.h>
#include "freertos/FreeRTOS.h"    // FreeRTOS 内核
#include "freertos/task.h"         // FreeRTOS 任务管理
#include "freertos/event_groups.h" // 事件组同步机制
#include "esp_log.h"               // 日志输出模块
#include "nvs_flash.h"             // NVS（非易失性存储）
#include "driver/gpio.h"           // GPIO 驱动
#include "bsp_Audio.h"             // 音频播放板级支持包
#include "bsp_HumanIR.h"           // 人体红外传感器板级支持包
#include "mqtt.h"                  // MQTT 通信模块
#include "ble_prov.h"              // BLE 配网模块
#include "web_config.h"            // Web 配置服务
#include "oled.h"                    // OLED 显示模块
#include "esp_wifi.h"              // WiFi 控制与状态 API
#include "esp_netif.h"             // 网络接口与 IP 地址 API

/** @brief 日志标签 */
static const char *TAG = "MAIN";

// ================= 全局状态变量定义 =================

/** @brief 是否布防，true=启用告警，false=忽略传感器 */
bool is_armed = true;

/** @brief PIR 传感器状态：1=有人，0=无人 */
bool pir_state = false;

// ===================================================

// ================= UI 状态与全局数据 =================
typedef enum {
    UI_MODE_BOOT,    // 开机初始化界面
    UI_MODE_NORMAL   // 正常运行仪表盘界面
} ui_mode_t;

volatile ui_mode_t current_ui_mode = UI_MODE_BOOT; 
char boot_msg[32] = "Starting...";                 

// 传感器与网络状态数据
char current_ssid[33] = "None"; // 保存 WiFi 名称
char local_ip[20] = "0.0.0.0";
bool is_wifi_connected = false;
int current_temp = 0;   
int current_hum = 0;    
volatile bool is_alarming = false; // 👈 替换：不再弹窗，而是文字警报状态
// ===================================================


/** @brief SPIFFS 文件系统中的告警音频文件路径
 *  @details 当人体传感器触发时播放此音频文件 */
#define MP3_FILE_PATH "/spiffs/people.mp3"

/**
 * @brief PIR 监测 + 告警音频播放任务（布防时触发）
 */
void pir_audio_task(void *pvParameters)
{
    ESP_LOGI(TAG, "PIR Sensor Task started. Waiting for motion...");

    while (1) {
        // 周期性读取人体红外传感器的状态
        // 返回值: 1 表示检测到人体，0 表示无人
        pir_state = Get_HumanIR();

        // 同时检查两个条件：警报模式启用 AND 传感器检测到人体
        if (is_armed == true && pir_state == 1) {
            ESP_LOGI(TAG, "Motion Detected! (检测到人) - Starting playback...");
            
            // 可选：可以在此添加 WS2812B LED 视觉告警（红灯指示）
            // 当前未启用，如需启用请取消注释以下代码：
            // Led_SetColor(255, 0, 0); // 设置 LED 显示红色
            
            // 👇 通知 OLED 任务当前处于报警状态
            is_alarming = true;

            // 触发听觉告警：播放音频文件
            // 注意：play_mp3() 是阻塞调用，直到音频播放完毕才返回
            play_mp3(MP3_FILE_PATH);

            ESP_LOGI(TAG, "Playback finished. Waiting for next trigger...");
            // 👇 报警结束，恢复正常显示
            is_alarming = false;
            
            // 可选：恢复正常的 LED 指示色（绿灯表示就绪）
            // 当前未启用，如需启用请取消注释以下代码：
            // Led_SetColor(0, 255, 0); // 设置 LED 显示绿色
            
            // 播放完毕后强制延时 1 秒
            // 作用：防止传感器持续信号导致频繁触发告警
            vTaskDelay(pdMS_TO_TICKS(1000)); 
        }

        // 非阻塞检测循环：每 100 毫秒轮询一次传感器状态
        // 此延时同时用来：
        // 1. 喂 watchdog 定时器（防止复位）
        // 2. 让出 CPU 给其他任务执行
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


/** @brief OLED 显示任务（开机 + 仪表盘） */
void OLED_Display_Task(void *pvParameters)
{
    // 用于控制网络状态查询频率的计数器
    static uint8_t net_update_cnt = 0; 

    while(1) {
        // ----- 【后台任务：每隔 1 秒 (10*100ms) 拉取一次真实网络状态】 -----
        if (++net_update_cnt >= 10) {
            net_update_cnt = 0;
            wifi_ap_record_t ap_info;
            
            // 尝试获取当前连接的路由器信息
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                is_wifi_connected = true;
                snprintf(current_ssid, sizeof(current_ssid), "%s", ap_info.ssid);
                
                // 获取默认的 Station 网络接口并读取真实 IP
                esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
                if (netif) {
                    esp_netif_ip_info_t ip_info;
                    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                        // 将二进制 IP 地址转为常见的 192.168.x.x 字符串
                        snprintf(local_ip, sizeof(local_ip), "%d.%d.%d.%d", 
                                 esp_ip4_addr1_16(&ip_info.ip), esp_ip4_addr2_16(&ip_info.ip),
                                 esp_ip4_addr3_16(&ip_info.ip), esp_ip4_addr4_16(&ip_info.ip));
                    }
                }
            } else {
                // WiFi 未连接
                is_wifi_connected = false;
                snprintf(current_ssid, sizeof(current_ssid), "Wait...");
                snprintf(local_ip, sizeof(local_ip), "0.0.0.0");
            }
        }

        // ----- 【开始刷新屏幕画面】 -----
        OLED_Clear(); // 擦除显存

        if (current_ui_mode == UI_MODE_BOOT) {
            // 开机初始化界面
            OLED_ShowString(16, 0, (u8*)"CT-Watcher", 16, 1);
            OLED_ShowString(0, 32, (u8*)boot_msg, 16, 1);
            
        } else if (current_ui_mode == UI_MODE_NORMAL) {
            char str_buf[32];
            
            // 行 1 (Y=0): WiFi 状态与名称 (限长防止越界)
            sprintf(str_buf, "WF:%.12s", current_ssid);
            OLED_ShowString(0, 0, (u8*)str_buf, 16, 1);
            
            // 行 2 (Y=16): 真实局域网 IP
            sprintf(str_buf, "IP:%s", local_ip);
            OLED_ShowString(0, 16, (u8*)str_buf, 16, 1);
            
            // 行 3 (Y=32): 布防模式 (MD) 与 警报状态 (ST) 同显！
            if (is_armed) {
                if (is_alarming) {
                    // 布防且传感器触发：闪烁 ALM
                    if ((xTaskGetTickCount() * portTICK_PERIOD_MS) % 500 < 250) {
                        sprintf(str_buf, "MD:ARM ST:*ALM*");
                    } else {
                        sprintf(str_buf, "MD:ARM ST: ALM ");
                    }
                } else {
                    // 布防但安全 (无人进入)
                    sprintf(str_buf, "MD:ARM ST:Safe ");
                }
            } else {
                // 撤防模式下，忽略传感器，直接显示横线
                sprintf(str_buf, "MD:DIS ST:---- ");
            }
            OLED_ShowString(0, 32, (u8*)str_buf, 16, 1);

            // 行 4 (Y=48): 温湿度 (待 DHT11 接入)
            sprintf(str_buf, "T:%dC  H:%d%%", current_temp, current_hum);
            OLED_ShowString(0, 48, (u8*)str_buf, 16, 1);
        }

        OLED_Refresh(); // 一次性推送到屏幕，防止闪烁
        vTaskDelay(pdMS_TO_TICKS(100)); // 10 FPS
    }
}

/**
 * @brief 应用主入口（初始化各模块并创建后台任务）
 */
void app_main(void)
{
    // ========== 步骤 1：初始化 NVS（非易失性存储）==========
    // NVS 用于存储 WiFi SSID、密码、MQTT 服务器地址等配置信息
    esp_err_t ret = nvs_flash_init();
    
    // 如果 NVS 分区损坏或版本不兼容，则擦除后重新初始化
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());      // 清空 NVS 分区
        ESP_ERROR_CHECK(nvs_flash_init());        // 重新初始化
    }

    // ========== 优先初始化 OLED 并启动显示任务 ==========
    OLED_Init();
    OLED_Clear();
    // 创建 OLED 任务 (优先级可设为 4)
    xTaskCreate(OLED_Display_Task, "OLED_Task", 4096, NULL, 4, NULL);
    // ===================================================

    // ========== 步骤 2：挂载 SPIFFS 文件系统 ==========
    // SPIFFS 存储音频文件（people.mp3）等资源
    ESP_LOGI(TAG, "Mounting SPIFFS...");
    if (bsp_spiffs_mount() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS");  // 文件系统错误
        return;                                    // 无法继续初始化，退出
    }

    // ========== 步骤 3：初始化人体红外传感器 ==========
    // 初始化工作：
    // - 配置传感器连接的 GPIO 引脚
    // - 设置输入模式和中断（可选）
    // - 初始化传感器驱动
    HumanIR_Init();

    // ========== 步骤 4：初始化 WiFi 和配网服务 ==========
    // 支持两种配网模式：
    // - BLE 配网：通过蓝牙接收 WiFi 配置
    // - SoftAP 配网：通过热点接收配置
    ESP_LOGI(TAG, "Initializing WiFi...");
    app_wifi_prov_start();  // 启动配网流程，如已配置则自动连接

    // ========== 步骤 5：启动 Web 配置服务器 ==========
    // 在后台运行 HTTP/WebSocket 服务器，用于：
    // - 提供 Web UI 配置界面
    // - 接收远程控制命令（启用/禁用告警）
    // - 实时状态查询
    start_mqtt_web_server();

    // ========== 步骤 6：连接 MQTT 服务器 ==========
    // 使用之前保存的 MQTT 配置（服务器地址、端口、用户名等）
    // 用于：
    // - 上报传感器检测状态
    // - 接收远程控制命令
    mqtt_start();

    // ========== 步骤 7：初始化 I2S 音频接口和解码器 ==========
    // I2S（Inter-IC Sound）用于与音频编解码器通信
    // 工作内容：
    // - 配置 I2S 时钟、数据引脚
    // - 设置采样率、位深等参数
    // - 注册 MP3/其他格式的解码器
    ESP_LOGI(TAG, "Initializing I2S...");
    i2s_init();


    // ========== 初始化全部完成，切换到正常仪表盘 ==========
    sprintf(boot_msg, "System Ready!");
    vTaskDelay(pdMS_TO_TICKS(1000));
    current_ui_mode = UI_MODE_NORMAL; // 👈 这一句会将屏幕切换到仪表盘模式！
    // ===================================================

    // ========== 步骤 8：创建后台任务 ==========
    
    // 任务 1：人体传感器检测与告警任务
    // - 周期检测传感器状态
    // - 触发时播放告警音频
    // - 参数说明：
    //   - "pir_audio_task": 任务名（日志使用）
    //   - 8192: 堆栈大小（字节）
    //   - 5: 优先级（数字越大优先级越高）
    xTaskCreate(pir_audio_task, "pir_audio_task", 8192, NULL, 5, NULL);

    // 任务 2：MQTT 数据上报任务
    // - 周期性读取传感器和系统状态
    // - 通过 MQTT 发送数据到服务器
    // - 接收并执行远程控制命令
    xTaskCreate(DataReport_Task, "DataReport_Task", 4096, NULL, 5, NULL);
    
    // 在此之后，app_main() 返回，但已创建的任务会继续在后台运行
    // 系统由 FreeRTOS 调度器接管，轮流执行各个任务
}