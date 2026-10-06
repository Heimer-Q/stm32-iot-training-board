/**
  ******************************************************************************
  * @file    bsp_led.c
  * @brief   三颗 LED 的开关与亮度控制（练习点：PWM 调亮度）
  *
  * == 新人导读 ================================================================
  * 1. 这块代码管什么？
  *    板上三颗 LED 接在 PA6 / PA7 / PB0，由定时器 TIM3 的三个 PWM 通道驱动。
  *    你不需要自己算时间，只要这样调用：
  *        BSP_LED_SetPercent(0, 50);   // 第 1 颗灯，半亮
  *
  * 2. 为什么"调亮度"要用 PWM？
  *    LED 只有亮和灭两种状态，没有"半亮"。PWM 的办法是让 LED 在一秒钟里
  *    快速地亮-灭很多次：如果"亮"占 30% 的时间、"灭"占 70%，人眼分辨不过来，
  *    看到的就是 30% 的亮度。这个比例叫"占空比"。
  *
  * 3. 一个重要的共享关系（上课会讲）：
  *    三颗 LED（TIM3 的 CH1~CH3）和蜂鸣器（CH4）共用同一个定时器。
  *    蜂鸣器改音调时会顺带改变 LED 的波形频率，但"亮的比例"不变，
  *    所以肉眼看到的亮度不受影响。
  ******************************************************************************
  */

#include "bsp_led.h"

/* 灯号(0/1/2) → TIM3 通道。有了这张小表，传"灯号"就能找到对应的定时器通道 */
static const uint32_t led_channel[LED_NUM] = {LED1_CHANNEL, LED2_CHANNEL, LED3_CHANNEL};

/* 初始化：让三个通道开始输出 PWM，并把亮度清零（上电不会突然亮）
   调用时机：APP 初始化时调用一次即可 */
void BSP_LED_Init(void)
{
    for (uint8_t i = 0; i < LED_NUM; i++)
    {
        HAL_TIM_PWM_Start(LED_TIM, led_channel[i]);          /* 启动这一路 PWM 输出 */
        __HAL_TIM_SET_COMPARE(LED_TIM, led_channel[i], 0);   /* 比较值=0 → 占空比 0% → 灯灭 */
    }
}

/* 把某颗灯调到指定亮度
     参数 index  ：0 = 第 1 颗、1 = 第 2 颗、2 = 第 3 颗
     参数 percent：0 = 全灭 …… 100 = 最亮
   原理：定时器每个周期从 0 数到 LED_PWM_PERIOD；计数值小于"比较值"的时间里，
   引脚输出高电平（灯亮），其余时间输出低电平——比较值越大，亮的时间越长。 */
void BSP_LED_SetPercent(uint8_t index, uint8_t percent)
{
    if (index >= LED_NUM)   return;          /* 灯号越界（比如传了 5）：直接忽略，不会改到别人 */
    if (percent > 100U)     percent = 100U;  /* 亮度封顶 100%，防止算出超过周期的比较值 */
    uint32_t compare = (LED_PWM_PERIOD * (uint32_t)percent) / 100U;  /* 百分比 → 比较值 */
    __HAL_TIM_SET_COMPARE(LED_TIM, led_channel[index], compare);
}

/* 三颗全灭：开机初始化和"全灭"按键都会用到 */
void BSP_LED_AllOff(void)
{
    for (uint8_t i = 0; i < LED_NUM; i++)
    {
        BSP_LED_SetPercent(i, 0);
    }
}
