/*
 * 立创开发板软硬件资料与相关扩展板软硬件资料官网全部开源
 * 开发板官网：www.lckfb.com
 * 技术支持常驻论坛，任何技术问题欢迎随时交流学习
 * 立创论坛：club.szlcsc.com
 * 关注bilibili账号：【立创开发板】，掌握我们的最新动态！
 * 不靠卖板赚钱，以培养中国工程师为己任
 * Change Logs:
 * Date           Author       Notes
 * 2024-01-04     LCKFB-lp    first version
 */


#include "bsp_HumanIR.h"

static const char *TAG = "HumanIR";


void delay_ms(unsigned int ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}
/******************************************************************
 * 函 数 名 称：HumanIR_Init
 * 函 数 说 明：人体红外模块初始化
 * 函 数 形 参：无
 * 函 数 返 回：无
 * 作       者：LC
 * 备       注：无
******************************************************************/
void HumanIR_Init(void)
{
    gpio_config_t io_config = {
        .pin_bit_mask = (1ULL<<HUMANIR_PIN),     //配置引脚
        .mode =GPIO_MODE_INPUT,                  //输入模式
        .pull_up_en = GPIO_PULLUP_ENABLE,        //使能上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE,   //不使能下拉
        .intr_type = GPIO_INTR_DISABLE           //不使能引脚中断
    };
    gpio_config(&io_config);
}

/******************************************************************
 * 函 数 名 称：Get_HumanIR
 * 函 数 说 明：获取人体红外输出引脚的电平状态
 * 函 数 形 参：无
 * 函 数 返 回：0=感应到人体红外    1=未感应到人体红外
 * 作       者：LC
 * 备       注：无
******************************************************************/
bool Get_HumanIR(void)
{
    return ( gpio_get_level(HUMANIR_PIN) ? 1 : 0 );
}


/******************************************************************
 * 函 数 名 称：HumanIR_Task
 * 函 数 说 明：FreeRTOS任务，轮询PIR并打印状态
******************************************************************/
void HumanIR_Task(void *arg)
{
    int last_state = 0;

    while (1)
    {
        bool detected = Get_HumanIR();

        // 只在状态变化时打印
        if (detected && last_state == 0)
        {
            ESP_LOGI(TAG,"有人进入");
        }
        else if (!detected && last_state == 1)
        {
            ESP_LOGI(TAG,"无人");
        }

        last_state = detected;

        // 100ms轮询一次
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

