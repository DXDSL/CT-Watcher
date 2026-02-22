#include "DHT11.h"

uint8_t DHT11Data[4] = {0};
uint8_t Temp, Humi;

#define TIMEOUT_US 1000      // 定义超时时间, 单位为微秒


static const char *TAG = "DHT11";

// us 延时函数，误差不能太大
void DelayUs(uint32_t nCount)
{
    esp_rom_delay_us(nCount);
}

void DHT11_Start(void)
{
    DHT11_OUT; // 设置端口方向
    DHT11_CLR; // 拉低端口
    DelayUs(19 * 1000);

    DHT11_SET;   // 释放总线
    DelayUs(30); // 总线由上拉电阻拉高，主机延时30us;
    DHT11_IN;    // 设置端口方向

    uint32_t timeout = TIMEOUT_US;
    while (!gpio_get_level(DHT11_PIN) && timeout > 0)
    {
        DelayUs(1);
        timeout--;
    }

    if (timeout == 0)
    {
        ESP_LOGE(TAG,"Error: DHT11 response timeout\n");
        return;
    }

    timeout = TIMEOUT_US;
    while (gpio_get_level(DHT11_PIN) && timeout > 0)
    {
        DelayUs(1);
        timeout--;
    }

    if (timeout == 0)
    {
        ESP_LOGE(TAG,"Error: DHT11 pull-up timeout\n");
        return;
    }
}

uint8_t DHT11_ReadValue(void)
{
    uint8_t i, sbuf = 0;
    for (i = 8; i > 0; i--)
    {
        sbuf <<= 1;
        uint32_t timeout = TIMEOUT_US;

        while (!gpio_get_level(DHT11_PIN) && timeout > 0)
        {
            DelayUs(1);
            timeout--;
        }

        if (timeout == 0)
        {
            ESP_LOGE(TAG,"Error: DHT11 bit wait timeout\n");
            return 0; // 或者其他适当的错误值
        }

        DelayUs(30); // 延时 30us 后检测数据线是否还是高电平

        if (gpio_get_level(DHT11_PIN))
        {
            sbuf |= 1;
        }

        timeout = TIMEOUT_US;
        while (gpio_get_level(DHT11_PIN) && timeout > 0)
        {
            DelayUs(1);
            timeout--;
        }

        if (timeout == 0)
        {
            ESP_LOGE(TAG,"Error: DHT11 bit read timeout\n");
            return 0; // 或者其他适当的错误值
        }
    }
    return sbuf;
}

uint8_t DHT11_ReadTemHum(uint8_t *buf)
{
    uint8_t check;

    buf[0] = DHT11_ReadValue();
    buf[1] = DHT11_ReadValue();
    buf[2] = DHT11_ReadValue();
    buf[3] = DHT11_ReadValue();

    check = DHT11_ReadValue();

    if (check == buf[0] + buf[1] + buf[2] + buf[3])
        return 1;
    else
        return 0;
}

void DHT11Get_Task(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(100));

    while (1)
    {
        DHT11_Start();
        if (DHT11_ReadTemHum(DHT11Data))
        {
            Temp = DHT11Data[2];
            Humi = DHT11Data[0];
            // ESP_LOGI(TAG,"Temp=%d, Humi=%d\r\n", Temp, Humi);
        }
        else
        {
            ESP_LOGI(TAG, "DHT11 Error!\r\n");
        }
        // esp_task_wdt_reset();// 定期重置任务看门狗
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
