#include "bsp_beep.h"

/* TIM3 的时钟是 72MHz（与 .ioc 的时钟树一致），计数频率 = 72MHz / (PSC+1)，
   音调 f = 72MHz / ((PSC+1) × (ARR+1))，这里 ARR+1 = BEEP_PWM_PERIOD = 1000 */
#define BEEP_TIM_CLK_HZ     72000000UL
#define BEEP_LED_PSC        71U      /* 恢复成 LED 的 1kHz（CubeMX 生成时就是 71） */

static uint32_t beep_stop_tick;
static uint8_t  beep_busy;

/* 触发极性 → 比较值：低电平触发时，"静音"是让引脚保持高电平 */
static uint32_t ccr_silent(void)
{
#if (BEEP_TRIG == BEEP_TRIG_LOW)
    return BEEP_PWM_PERIOD;      /* 高电平 = 静音 */
#else
    return 0U;                   /* 低电平 = 静音 */
#endif
}

#if (BEEP_MODE == BEEP_MODE_DC)
/* 只有"有源"模块才用得上：给一个持续电平就让它一直响 */
static uint32_t ccr_hold(void)
{
#if (BEEP_TRIG == BEEP_TRIG_LOW)
    return 0U;                   /* 持续拉低 = 让（有源）模块一直响 */
#else
    return BEEP_PWM_PERIOD;      /* 持续拉高 = 让（有源）模块一直响 */
#endif
}
#endif

/* 频率 → 预分频值（结果自动落在合法范围） */
static uint32_t psc_for_hz(uint16_t hz)
{
    uint32_t div;

    if (hz == 0U)
    {
        return BEEP_LED_PSC;
    }
    div = BEEP_TIM_CLK_HZ / ((uint32_t)hz * BEEP_PWM_PERIOD);
    if (div == 0U)    { div = 1U; }        /* 频率太高：取硬件上限 */
    if (div > 65536U) { div = 65536U; }    /* 频率太低：PSC 是 16 位 */
    return div - 1U;
}

void BSP_BEEP_Init(void)
{
    __HAL_TIM_SET_PRESCALER(BEEP_TIM, BEEP_LED_PSC);           /* LED 侧保持 1kHz */
    __HAL_TIM_SET_COMPARE(BEEP_TIM, BEEP_CHANNEL, ccr_silent());/* 先把引脚写成静音电平 */
    HAL_TIM_PWM_Start(BEEP_TIM, BEEP_CHANNEL);                 /* 再使能输出，避免上电就叫 */
    beep_busy = 0U;
}

void BSP_BEEP_On(void)
{
#if (BEEP_MODE == BEEP_MODE_TONE)
    BSP_BEEP_Tone(BEEP_DEFAULT_HZ);                            /* 无源：给方波才能响 */
#else
    __HAL_TIM_SET_COMPARE(BEEP_TIM, BEEP_CHANNEL, ccr_hold()); /* 有源：给持续电平 */
#endif
}

void BSP_BEEP_Off(void)
{
    __HAL_TIM_SET_COMPARE(BEEP_TIM, BEEP_CHANNEL, ccr_silent());
    __HAL_TIM_SET_PRESCALER(BEEP_TIM, BEEP_LED_PSC);           /* 把 1kHz 还给 LED */
    beep_busy = 0U;
}

void BSP_BEEP_Tone(uint16_t hz)
{
    if (hz == 0U)
    {
        BSP_BEEP_Off();
        return;
    }
    __HAL_TIM_SET_PRESCALER(BEEP_TIM, psc_for_hz(hz));
    __HAL_TIM_SET_COMPARE(BEEP_TIM, BEEP_CHANNEL, BEEP_PWM_PERIOD / 2U);   /* 50% 方波最响 */
}

void BSP_BEEP_PlayTone(uint16_t hz, uint16_t ms)
{
    BSP_BEEP_Tone(hz);
    beep_stop_tick = HAL_GetTick() + (uint32_t)ms;
    beep_busy      = 1U;
}

void BSP_BEEP_Beep(uint16_t ms)
{
    BSP_BEEP_PlayTone(BEEP_DEFAULT_HZ, ms);
}

void BSP_BEEP_Task(void)
{
    if ((beep_busy != 0U) && ((int32_t)(HAL_GetTick() - beep_stop_tick) >= 0))
    {
        BSP_BEEP_Off();
    }
}

uint8_t BSP_BEEP_IsBusy(void)
{
    return beep_busy;
}
