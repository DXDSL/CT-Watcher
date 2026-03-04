#include "DHT11.h"

/**
 * @def TIMEOUT_US
 * @brief 通信超时时间
 * @details 单位为微秒，用于检测与DHT11通信是否正常，防止总线卡死
 */
#define TIMEOUT_US 1000


/** @brief 日志标签，用于ESP_LOG系列函数输出 */
static const char *TAG = "DHT11";

/**
 * @brief 微秒级延时函数
 * @param[in] nCount 延时时间，单位为微秒(us)
 * @note 基于ROM级延时函数实现，精度较高，误差应小于1us
 * @warning 此函数阻塞执行，在中断优先级较高的任务中使用可能影响系统响应
 */
void DelayUs(uint32_t nCount)
{
    esp_rom_delay_us(nCount);
}

/**
 * @brief DHT11启动序列
 * @details 
 *   - 主机拉低总线19ms以上以发出启动信号
 *   - 释放总线，等待DHT11响应
 *   - DHT11将拉低总线约80us，然后释放约80us以表示准备就绪
 * @note 该函数必须在DHT11_ReadValue()之前调用以启动通信
 * @retval 无返回值，出错时打印日志
 */
void DHT11_Start(void)
{
    // 第一步：主机发送启动信号
    DHT11_OUT;              // 设置GPIO为输出模式
    DHT11_CLR;              // 拉低数据线
    DelayUs(19 * 1000);     // 保持低电平至少19ms以启动DHT11

    // 第二步：释放总线等待DHT11响应
    DHT11_SET;              // 释放总线，由上拉电阻将其拉高
    DelayUs(30);            // 主机延时30us，等待DHT11识别信号
    DHT11_IN;               // 设置GPIO为输入模式以读取DHT11响应

    // 第三步：等待DHT11拉低总线（表示DHT11已就绪）
    uint32_t timeout = TIMEOUT_US;
    while (!gpio_get_level(DHT11_PIN) && timeout > 0)
    {
        DelayUs(1);
        timeout--;
    }

    if (timeout == 0)
    {
        ESP_LOGE(TAG, "Error: DHT11 response timeout\n");
        return;
    }

    // 第四步：等待DHT11释放总线（表示即将开始数据传输）
    timeout = TIMEOUT_US;
    while (gpio_get_level(DHT11_PIN) && timeout > 0)
    {
        DelayUs(1);
        timeout--;
    }

    if (timeout == 0)
    {
        ESP_LOGE(TAG, "Error: DHT11 pull-up timeout\n");
        return;
    }
}

/**
 * @brief 读取DHT11的一个字节数据
 * @details DHT11通过时序编码数据，每个bit的含义由相应的高电平时间判定：
 *   - 高电平<30us：表示bit为0
 *   - 高电平>30us：表示bit为1
 * @retval uint8_t 读取的一个字节数据，出错时返回0
 * @note 该函数应在DHT11_Start()后调用，依次读取5个字节（湿度整、湿度小数、
 *       温度整、温度小数、校验码）
 */
uint8_t DHT11_ReadValue(void)
{
    uint8_t i, sbuf = 0;
    
    // 逐bit读取，从MSB到LSB
    for (i = 8; i > 0; i--)
    {
        sbuf <<= 1;  // 左移一位为新的bit腾出空间
        uint32_t timeout = TIMEOUT_US;

        // 等待DHT11拉低总线（bit启动标志）
        while (!gpio_get_level(DHT11_PIN) && timeout > 0)
        {
            DelayUs(1);
            timeout--;
        }

        if (timeout == 0)
        {
            ESP_LOGE(TAG, "Error: DHT11 bit wait timeout\n");
            return 0;
        }

        // 等待30us后检测电平以判定bit值
        DelayUs(30);

        if (gpio_get_level(DHT11_PIN))
        {
            sbuf |= 1;  // 若仍为高电平，则此bit为1
        }
        // 否则此bit为0（已在sbuf中通过左移实现）

        // 等待DHT11释放总线（bit周期结束）
        timeout = TIMEOUT_US;
        while (gpio_get_level(DHT11_PIN) && timeout > 0)
        {
            DelayUs(1);
            timeout--;
        }

        if (timeout == 0)
        {
            ESP_LOGE(TAG, "Error: DHT11 bit read timeout\n");
            return 0;
        }
    }
    return sbuf;
}

/**
 * @brief 读取DHT11的温湿度数据
 * @param[out] buf 数据缓冲区指针，接收4字节数据
 *                  buf[0]: 湿度整数部分
 *                  buf[1]: 湿度小数部分
 *                  buf[2]: 温度整数部分
 *                  buf[3]: 温度小数部分
 * @retval 1 数据读取成功且校验码正确
 * @retval 0 数据读取失败或校验码错误
 * @note DHT11的校验码为前4字节的和的低8位
 */
uint8_t DHT11_ReadTemHum(uint8_t *buf)
{
    uint8_t check;  // 校验码变量

    // 依次读取4个字节数据
    buf[0] = DHT11_ReadValue();  // 湿度整数
    buf[1] = DHT11_ReadValue();  // 湿度小数
    buf[2] = DHT11_ReadValue();  // 温度整数
    buf[3] = DHT11_ReadValue();  // 温度小数

    // 读取校验码
    check = DHT11_ReadValue();

    // 校验数据完整性：校验码应等于前4字节之和
    if (check == buf[0] + buf[1] + buf[2] + buf[3])
        return 1;  // 校验通过
    else
        return 0;  // 校验失败
}

/**
 * @brief DHT11数据读取FreeRTOS任务
 * @param[in] pvParameters 任务参数（本应用中未使用）
 * @details 该任务定期读取DHT11传感器数据，周期为1秒
 *   - 调用DHT11_Start()发起通信
 *   - 调用DHT11_ReadTemHum()读取温湿度数据
 *   - 更新全局变量Temp和Humi
 *   - 打印错误日志（如果读取失败）
 * @note 任务启动前延时100ms以确保硬件初始化完成
 * @warning 若需要在中断中使用温度/湿度数据，应考虑添加互斥锁保护
 */
void DHT11Get_Task(void *pvParameters)
{
    // 初始延时，等待硬件稳定
    vTaskDelay(pdMS_TO_TICKS(100));

    while (1)
    {
        // 发起DHT11通信序列
        DHT11_Start();
        
        // 读取温湿度数据
        if (DHT11_ReadTemHum(DHT11Data))
        {
            // 数据读取成功，更新全局变量
            Temp = DHT11Data[2];  // 提取温度整数部分
            Humi = DHT11Data[0];  // 提取湿度整数部分
            // 可在此处添加日志输出：
            // ESP_LOGI(TAG, "Temp=%d, Humi=%d\r\n", Temp, Humi);
        }
        else
        {
            // 数据读取失败或校验失败
            ESP_LOGI(TAG, "DHT11 Error!\r\n");
        }
        
        // 可根据看门狗设置启用此行:
        // esp_task_wdt_reset();  // 定期重置任务看门狗
        
        // 延时1000ms后再次读取（DHT11最小采样间隔约2秒，此处相对保守）
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
