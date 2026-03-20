#include "bsp_oled.h"
#include "u8g2.h"
#include "esp32_hw_i2c.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

// ================= 定义全局 UI 变量 =================
volatile ui_mode_t current_ui_mode = UI_MODE_BOOT; 
volatile boot_state_t boot_states[4] = {STEP_PENDING, STEP_PENDING, STEP_PENDING, STEP_PENDING};
bool is_system_initialized = false;

char current_ssid[33] = "None"; 
char local_ip[20] = "127.0.0.1";
bool is_wifi_connected = false;
int current_temp = 0;
int current_hum = 0;
volatile bool is_alarming = false; 

// ================= U8g2 底层变量 =================
u8g2_t u8g2; 
u8g2_esp32_i2c_ctx_t i2c_ctx = {0}; 

// ================= 初始化函数 =================
void init_oled_u8g2(void) {
    u8g2_esp32_i2c_config_t i2c_cfg = U8G2_ESP32_I2C_CONFIG_DEFAULT();
    i2c_cfg.sda_pin = 8; 
    i2c_cfg.scl_pin = 9; 
    i2c_ctx.cfg = i2c_cfg;

    u8g2_esp32_i2c_set_default_context(&i2c_ctx);
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8x8_byte_esp32_hw_i2c, u8x8_gpio_and_delay_esp32_i2c);

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0); 
    u8g2_ClearBuffer(&u8g2);     
}

// ================= 显示任务 =================
void OLED_Display_Task(void *pvParameters) {
    static uint8_t net_update_cnt = 0; 
    u8g2_SetFontMode(&u8g2, 1); 

    while(1) {
        // ----- 【后台任务：拉取网络状态】 -----
        if (++net_update_cnt >= 10) {
            net_update_cnt = 0;
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                is_wifi_connected = true;
                snprintf(current_ssid, sizeof(current_ssid), "%s", ap_info.ssid);
                
                esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
                if (netif) {
                    esp_netif_ip_info_t ip_info;
                    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                        snprintf(local_ip, sizeof(local_ip), "%d.%d.%d.%d", 
                                 esp_ip4_addr1_16(&ip_info.ip), esp_ip4_addr2_16(&ip_info.ip),
                                 esp_ip4_addr3_16(&ip_info.ip), esp_ip4_addr4_16(&ip_info.ip));
                    }
                }
            } else {
                is_wifi_connected = false;
                snprintf(current_ssid, sizeof(current_ssid), "已断开，等待连接...");
                snprintf(local_ip, sizeof(local_ip), "0.0.0.0");
            }
        }

        // ----- 【屏幕绘制逻辑】 -----
        u8g2_ClearBuffer(&u8g2); 
        u8g2_SetFont(&u8g2, u8g2_font_wqy12_t_gb2312); 

        if (current_ui_mode == UI_MODE_BOOT) {
            // 开机自检界面
            u8g2_DrawUTF8(&u8g2, 24, 11, "- 系统启动中 -");
            u8g2_DrawHLine(&u8g2, 0, 14, 128); 

            const char* step_names[4] = {"挂载系统文件", "初始化传感器", "拉起网络服务", "启动音频模块"};
            for(int i = 0; i < 4; i++) {
                char line_buf[64];
                int y_pos = 26 + i * 12; 

                if (boot_states[i] == STEP_PENDING) {
                    sprintf(line_buf, "[ ] %s", step_names[i]);
                } else if (boot_states[i] == STEP_IN_PROGRESS) {
                    if ((xTaskGetTickCount() * portTICK_PERIOD_MS) % 500 < 250) {
                        sprintf(line_buf, "[>] %s...", step_names[i]);
                    } else {
                        sprintf(line_buf, "[ ] %s...", step_names[i]);
                    }
                } else if (boot_states[i] == STEP_SUCCESS) {
                    sprintf(line_buf, "[√] %s", step_names[i]);
                } else if (boot_states[i] == STEP_FAILED) {
                    sprintf(line_buf, "[×] %s (ERR!)", step_names[i]);
                }
                u8g2_DrawUTF8(&u8g2, 4, y_pos, line_buf);
            }
        } 
        else if (current_ui_mode == UI_MODE_NORMAL) {
            // 正常仪表盘界面
            char str_buf[64];
            
            sprintf(str_buf, "WIFI: %s", current_ssid);
            u8g2_DrawUTF8(&u8g2, 0, 11, str_buf);
            u8g2_DrawHLine(&u8g2, 0, 14, 128); 
            
            sprintf(str_buf, "IP: %s", local_ip);
            u8g2_DrawUTF8(&u8g2, 0, 27, str_buf);
            u8g2_DrawHLine(&u8g2, 0, 30, 128); 
            
            if (is_armed) {
                if (is_alarming) {
                    if ((xTaskGetTickCount() * portTICK_PERIOD_MS) % 500 < 250) {
                        u8g2_DrawUTF8(&u8g2, 0, 43, "模式：布防    [!]警报!");
                    } else {
                        u8g2_DrawUTF8(&u8g2, 0, 43, "模式：布防       警报! ");
                    }
                } else {
                    u8g2_DrawUTF8(&u8g2, 0, 43, "模式：布防 状态：安全");
                }
            } else {
                u8g2_DrawUTF8(&u8g2, 0, 43, "模式：撤防 状态：关闭");
            }
            u8g2_DrawHLine(&u8g2, 0, 46, 128); 

            sprintf(str_buf, "温度：%d°C  湿度：%d%%", current_temp, current_hum);
            u8g2_DrawUTF8(&u8g2, 0, 59, str_buf);
        }
        else if (current_ui_mode == UI_MODE_PROV) {
            // 配网引导界面
            if ((xTaskGetTickCount() * portTICK_PERIOD_MS) % 1000 < 500) {
                u8g2_DrawUTF8(&u8g2, 8, 12, "[!] 连续5次连接失败");
            } else {
                u8g2_DrawUTF8(&u8g2, 16, 12, "   连续5次连接失败");
            }
            u8g2_DrawHLine(&u8g2, 0, 15, 128); 

            u8g2_DrawUTF8(&u8g2, 0, 31, "请打开手机蓝牙配网");
            u8g2_DrawUTF8(&u8g2, 0, 47, "设备名称: CT-Watcher");
            u8g2_DrawUTF8(&u8g2, 0, 63, "设备密码: abcd1234");
        }

        u8g2_SendBuffer(&u8g2); 
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}