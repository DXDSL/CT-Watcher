#include "freertos/FreeRTOS.h"
#include "bsp_Audio.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "driver/i2s_std.h"

#include "esp_audio_dec.h"
#include "esp_audio_dec_default.h" // 必须包含默认解码器注册
#include "esp_mp3_dec.h"

#define I2S_BCLK 5
#define I2S_WS   6
#define I2S_DOUT 7

static i2s_chan_handle_t tx_chan;
static const char* TAG = "Audio";

/**
 * @brief 挂载 SPIFFS 分区
 */
esp_err_t bsp_spiffs_mount(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_BASE,
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = false,
    };

    esp_err_t ret_val = esp_vfs_spiffs_register(&conf);
    ESP_ERROR_CHECK(ret_val);

    size_t total = 0, used = 0;
    ret_val = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret_val != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret_val));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    return ret_val;
}

/**
 * @brief 初始化 I2S 并注册解码器
 */
void i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .bclk = I2S_BCLK,
            .ws   = I2S_WS,
            .dout = I2S_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .mclk = I2S_GPIO_UNUSED,
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    // 【关键修复】注册所有默认的音频解码器 (包含 MP3)
    esp_audio_dec_register_default();
    ESP_LOGI(TAG, "Audio decoders registered successfully");
}

/**
 * @brief 播放指定的 MP3 文件 (连续流解码，防卡顿)
 */
void play_mp3(const char *filepath)
{
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        return;
    }

    esp_audio_dec_cfg_t cfg = {
        .type = ESP_AUDIO_TYPE_MP3,
        .cfg = NULL,
        .cfg_sz = 0,
    };

    esp_audio_dec_handle_t decoder = NULL;
    if (esp_audio_dec_open(&cfg, &decoder) != ESP_AUDIO_ERR_OK || !decoder) {
        ESP_LOGE(TAG, "Failed to open audio decoder");
        fclose(fp);
        return;
    }

    #define INBUF_SIZE 4096
    uint8_t *inbuf = malloc(INBUF_SIZE);
    uint8_t *outbuf = malloc(4096);
    
    esp_audio_dec_in_raw_t raw = {
        .buffer = inbuf,
        .len = 0,
        .consumed = 0
    };

    esp_audio_dec_out_frame_t out_frame = {
        .buffer = outbuf,
        .len = 4096,
        .needed_size = 0,
        .decoded_size = 0
    };

    bool eof = false;
    ESP_LOGI(TAG, "Start playing MP3 continuous stream...");

    while (!eof || raw.len > 0) {
        // 维持缓冲区数据量，不足时从文件读取填补
        if (!eof && raw.len < 2048) {
            if (raw.len > 0 && raw.buffer != inbuf) {
                memmove(inbuf, raw.buffer, raw.len);
            }
            raw.buffer = inbuf;
            
            size_t bytes_to_read = INBUF_SIZE - raw.len;
            size_t read_bytes = fread(inbuf + raw.len, 1, bytes_to_read, fp);
            if (read_bytes < bytes_to_read) {
                eof = true;
            }
            raw.len += read_bytes;
        }

        if (raw.len == 0) break;

        // 执行解码
        esp_audio_err_t ret = esp_audio_dec_process(decoder, &raw, &out_frame);

        if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
            uint8_t *new_buf = realloc(out_frame.buffer, out_frame.needed_size);
            if (!new_buf) break;
            out_frame.buffer = new_buf;
            out_frame.len = out_frame.needed_size;
            continue;
        } 
        
        // 写入 I2S，依靠底层的 portMAX_DELAY 实现天然休眠喂狗，避免卡顿
        if (out_frame.decoded_size > 0) {
            size_t written = 0;
            i2s_channel_write(tx_chan, out_frame.buffer, out_frame.decoded_size, &written, portMAX_DELAY);
        }

        // 移动指针
        if (raw.consumed > 0) {
            raw.buffer += raw.consumed;
            raw.len -= raw.consumed;
            raw.consumed = 0;
        } else if (eof) {
            break;
        }
    }

    // 释放资源
    free(inbuf);
    free(out_frame.buffer);
    esp_audio_dec_close(decoder);
    fclose(fp);
    ESP_LOGI(TAG, "MP3 playback finished successfully");
}