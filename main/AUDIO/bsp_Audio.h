#ifndef __BSP_AUDIO_H
#define __BSP_AUDIO_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

// SPIFFS 的挂载点
#define SPIFFS_BASE "/spiffs"

// 函数声明
esp_err_t bsp_spiffs_mount(void);
void i2s_init(void);

// 修改为直接传入文件路径
void play_mp3(const char *filepath);

#endif // __BSP_AUDIO_H