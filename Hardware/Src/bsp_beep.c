/**
  ******************************************************************************
  * @file    bsp_beep.c
  * @brief   蜂鸣器驱动：发指定音高、响指定时长（PB1 / TIM3_CH4）
  *
  * == 新人导读 ================================================================
  * 1. 板上插的是"无源蜂鸣器模块"（J4 三针插座）。
  *    无源的意思是"里面没有振荡电路"：你给它一个变化的方波，它才响；
  *    方波的频率决定音高（523Hz ≈ 中央 C），停掉方波就安静。
  *    （有源模块反过来：给个电平就一直响、只有一种音——换模块时改
  *      bsp_beep.h 里的 BEEP_MODE 就行，这套代码两种都支持。）
  *
  * 2. 音高怎么来的？
  *    蜂鸣器接在 TIM3 的第 4 通道。定时器时钟 72MHz，PWM 周期固定 1000，
  *    于是输出频率 = 72MHz / (分频值 × 1000)——改"分频值"就换音高。
  *    psc_for_hz() 就是做这个换算：给你想要的 Hz，算出分频值。
  *
  * 3. 三个函数的分工：
  *      BSP_BEEP_Tone(hz)         立即发某个音（一直响，直到 Off）
  *      BSP_BEEP_PlayTone(hz, ms) 响 ms 毫秒后自动停（非阻塞，"到点"由 Task 收尾）
  *      BSP_BEEP_Beep(ms)         用默认音发一声（提示音）
  *    主循环里要定期调用 BSP_BEEP_Task()，它负责"到点自动停"。
  ******************************************************************************
  */

#include "bsp_beep.h"

/* TIM3 的时钟是 72MHz（与 .ioc 的时钟树一致），计数频率 = 72MHz / (PSC+1)，
   音调 f = 72MHz / ((PSC+1) × (ARR+1))，这里 ARR+1 = BEEP_PWM_PERIOD = 1000 */
#define BEEP_TIM_CLK_HZ     72000000UL
#define BEEP_LED_PSC        71U      /* 恢复成 LED 的 1kHz（CubeMX 生成时就是 71） */

static uint32_t beep_stop_tick;      /* "该停了"的时刻（毫秒时间戳） */
static uint8_t  beep_busy;           /* 1 = 还在响 */

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

/* 频率(Hz) → 预分频值（结果自动限制在合法范围）
   例：523Hz → 72000000 / (523 × 1000) ≈ 137.7 → 取整 137
   频率太高（div=0）按硬件上限处理；太低则封顶 65536（PSC 是 16 位寄存器） */
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

/* 初始化：先把引脚放成"静音"电平，再使能输出——顺序反了会"上电就叫" */
void BSP_BEEP_Init(void)
{
    __HAL_TIM_SET_PRESCALER(BEEP_TIM, BEEP_LED_PSC);           /* LED 侧保持 1kHz */
    __HAL_TIM_SET_COMPARE(BEEP_TIM, BEEP_CHANNEL, ccr_silent());/* 先把引脚写成静音电平 */
    HAL_TIM_PWM_Start(BEEP_TIM, BEEP_CHANNEL);                 /* 再使能输出，避免上电就叫 */
    beep_busy = 0U;
}

/* 开鸣（默认音调一直响）——调试或"有源模块"场景用 */
void BSP_BEEP_On(void)
{
#if (BEEP_MODE == BEEP_MODE_TONE)
    BSP_BEEP_Tone(BEEP_DEFAULT_HZ);                            /* 无源：给方波才能响 */
#else
    __HAL_TIM_SET_COMPARE(BEEP_TIM, BEEP_CHANNEL, ccr_hold()); /* 有源：给持续电平 */
#endif
}

/* 停：把引脚写回静音电平，并把分频值还给 LED（1kHz） */
void BSP_BEEP_Off(void)
{
    __HAL_TIM_SET_COMPARE(BEEP_TIM, BEEP_CHANNEL, ccr_silent());
    __HAL_TIM_SET_PRESCALER(BEEP_TIM, BEEP_LED_PSC);           /* 把 1kHz 还给 LED */
    beep_busy = 0U;
}

/* 发一个指定音高的音（一直响，直到 Off 或下一个 PlayTone 覆盖） */
void BSP_BEEP_Tone(uint16_t hz)
{
    if (hz == 0U)
    {
        BSP_BEEP_Off();                  /* 0Hz 当"停"处理，方便当参数传 */
        return;
    }
    __HAL_TIM_SET_PRESCALER(BEEP_TIM, psc_for_hz(hz));
    __HAL_TIM_SET_COMPARE(BEEP_TIM, BEEP_CHANNEL, BEEP_PWM_PERIOD / 2U);   /* 50% 方波最响 */
}

/* 响 ms 毫秒后自动停——关键：本函数立即返回，不会卡住主循环；
   "到点停"交给主循环里的 BSP_BEEP_Task() 做 */
void BSP_BEEP_PlayTone(uint16_t hz, uint16_t ms)
{
    BSP_BEEP_Tone(hz);
    beep_stop_tick = HAL_GetTick() + (uint32_t)ms;
    beep_busy      = 1U;
}

/* 用默认音发一声短提示音（开机"滴"、按键反馈） */
void BSP_BEEP_Beep(uint16_t ms)
{
    BSP_BEEP_PlayTone(BEEP_DEFAULT_HZ, ms);
}

/* 主循环里定期调用：检查"该停的时间"到了没有，到了就停 */
void BSP_BEEP_Task(void)
{
    if ((beep_busy != 0U) && ((int32_t)(HAL_GetTick() - beep_stop_tick) >= 0))
    {
        BSP_BEEP_Off();
    }
}

/* 是否还在响（曲库播放器在"上一个音放完了吗"的判断里会看这个） */
uint8_t BSP_BEEP_IsBusy(void)
{
    return beep_busy;
}
