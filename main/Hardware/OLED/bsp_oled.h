#ifndef __BSP_OLED_H
#define __BSP_OLED_H

#include <stdbool.h>
#include <stdint.h>

// ================= UI 状态枚举 =================
typedef enum {
    UI_MODE_BOOT,    // 开机初始化界面
    UI_MODE_NORMAL   // 正常运行仪表盘界面
} ui_mode_t;

// ================= 全局 UI 变量 =================
extern volatile ui_mode_t current_ui_mode; 
extern uint8_t boot_step;  // 👈 新增：用来控制开机打勾进度的变量
extern char current_ssid[33]; 
extern char local_ip[20];
extern bool is_wifi_connected;
extern int current_temp;   
extern int current_hum;    
extern volatile bool is_alarming; 
extern bool is_armed; 

// ================= OLED 任务与初始化 API =================
void init_oled_u8g2(void);
void OLED_Display_Task(void *pvParameters);

#endif