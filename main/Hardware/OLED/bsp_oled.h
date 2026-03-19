#ifndef __BSP_OLED_H
#define __BSP_OLED_H

#include <stdbool.h>
#include <stdint.h>

// ================= UI 状态枚举 =================
typedef enum {
    UI_MODE_BOOT,    // 开机初始化界面
    UI_MODE_NORMAL,  // 正常运行仪表盘界面
    UI_MODE_PROV     // 👈 配网引导界面 (断网重连5次后触发)
} ui_mode_t;

// ================= 启动步骤状态枚举 =================
typedef enum {
    STEP_PENDING = 0, // 等待中 [ ]
    STEP_IN_PROGRESS, // 执行中 [>] (带闪烁动画)
    STEP_SUCCESS,     // 成功   [√]
    STEP_FAILED       // 失败   [×]
} boot_state_t;

// ================= 全局 UI 变量 =================
extern volatile ui_mode_t current_ui_mode; 
extern volatile boot_state_t boot_states[4]; // 👈 4个核心步骤的状态数组
extern char current_ssid[33]; 
extern char local_ip[20];
extern bool is_wifi_connected;
extern int current_temp;   
extern int current_hum;    
extern volatile bool is_alarming; 
extern bool is_armed;
extern bool is_system_initialized; 

// ================= OLED 任务与初始化 API =================
void init_oled_u8g2(void);
void OLED_Display_Task(void *pvParameters);

#endif