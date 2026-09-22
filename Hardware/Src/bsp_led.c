#include "bsp_led.h"

static const uint32_t led_channel[LED_NUM] = {LED1_CHANNEL, LED2_CHANNEL, LED3_CHANNEL};

void BSP_LED_Init(void)
{
    for (uint8_t i = 0; i < LED_NUM; i++)
    {
        HAL_TIM_PWM_Start(LED_TIM, led_channel[i]);
        __HAL_TIM_SET_COMPARE(LED_TIM, led_channel[i], 0);
    }
}

void BSP_LED_SetPercent(uint8_t index, uint8_t percent)
{
    if (index >= LED_NUM)   return;
    if (percent > 100U)     percent = 100U;
    uint32_t compare = (LED_PWM_PERIOD * (uint32_t)percent) / 100U;
    __HAL_TIM_SET_COMPARE(LED_TIM, led_channel[index], compare);
}

void BSP_LED_AllOff(void)
{
    for (uint8_t i = 0; i < LED_NUM; i++)
    {
        BSP_LED_SetPercent(i, 0);
    }
}
