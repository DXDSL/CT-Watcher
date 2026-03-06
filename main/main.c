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

/** @brief 日志标签 */
static const char *TAG = "MAIN";

// ================= 全局状态变量定义 =================

/** @brief 警报启用状态标志
 *  @details true: 启用检测模式，传感器触发时播放音频告警
 *           false: 禁用检测模式，忽略传感器信号
 *  @note 上电默认启用，可通过 MQTT 或 Web 接口远程控制 */
bool is_armed = true;

/** @brief 人体红外传感器检测状态
 *  @details 1: 检测到人体运动
 *           0: 未检测到人体（传感器无信号）
 *  @note 由 pir_audio_task 定期读取和更新 */
bool pir_state = false;

// ===================================================

/** @brief SPIFFS 文件系统中的告警音频文件路径
 *  @details 当人体传感器触发时播放此音频文件 */
#define MP3_FILE_PATH "/spiffs/people.mp3"

/**
 * @brief 人体传感器检测与音频告警任务
 * @param[in] pvParameters FreeRTOS 任务参数（未使用）
 * @return 无返回值，任务循环执行
 * 
 * @details 此任务持续监控人体红外传感器状态，当以下条件同时满足时触发告警：
 *          1. 警报模式启用 (is_armed == true)
 *          2. 传感器检测到人体运动 (pir_state == 1)
 *          
 *          触发流程：
 *          - 读取传感器状态
 *          - 判断是否需要告警
 *          - 播放音频文件（阻塞直到完毕）
 *          - 延时 1 秒防止重复触发
 *          
 * @note 任务优先级为 5，堆栈大小为 8192 字节
 * @note 循环检测间隔为 100 毫秒
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
            
            // 触发听觉告警：播放音频文件
            // 注意：play_mp3() 是阻塞调用，直到音频播放完毕才返回
            play_mp3(MP3_FILE_PATH);
            
            ESP_LOGI(TAG, "Playback finished. Waiting for next trigger...");
            
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

/**
 * @brief 应用程序主入口点
 * @return 无返回值
 * 
 * @details 系统初始化流程：
 *          1. 初始化 NVS（非易失性存储）- 存储 WiFi/MQTT 配置
 *          2. 挂载 SPIFFS 文件系统 - 存储音频和其他资源
 *          3. 初始化人体红外传感器 - 配置 GPIO 和中断
 *          4. 初始化 WiFi 和配网服务 - BLE/SoftAP 配网模式
 *          5. 启动 Web 配置服务器 - 用于远程配置和控制
 *          6. 连接 MQTT 服务器 - 数据上报和远程控制
 *          7. 初始化 I2S 音频接口和解码器 - 配置音频播放
 *          8. 创建后台任务 - 传感器轮询和数据上报
 * 
 * @note 各步骤严格按顺序执行，每一步都是后续步骤的前置条件
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