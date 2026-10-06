/**
  ******************************************************************************
  * @file    bsp_adc.c
  * @brief   光敏 / 热敏采样：ADC 读取 + 滑动平均滤波 + 光照百分比换算
  *
  * == 新人导读 ================================================================
  * 1. 传感器怎么变成数字的？
  *    光敏、热敏都是"电阻随环境变化"的元件，和固定电阻串联后，
  *    中间点的电压就跟着变。ADC（模数转换器）把这个电压量成 0~4095 的数字
  *    （12 位精度，3.3V 量程），这个数字叫"原始码"。
  *
  * 2. 为什么读数要"滤波"？
  *    ADC 单次读数会有小抖动（电源噪声、采样误差），直接用会让灯闪、数字跳。
  *    这里用最简单的"8 点滑动平均"：每次采一个新值、丢掉最老的一个，
  *    取平均——读数就稳了。窗口越大越稳、响应越慢（当前 8 次）。
  *
  * 3. 怎么调？(作业常改点)
  *    光敏方向：本模块"越亮读数越小"，所以 LIGHT_INVERT = 1 要反转；
  *    标定：蒙住光敏看串口打印的原始码填 LIGHT_RAW_DARK，
  *          手电照着填 LIGHT_RAW_BRIGHT——百分比就准了。
  ******************************************************************************
  */

#include "bsp_adc.h"

/* ============================================================================
 *  光敏标定（上电后看串口打印的 LIGHT 原始码，然后改这两个数）
 *    - 蒙住光敏时读到的原始码 → LIGHT_RAW_DARK
 *    - 手电照着读到的原始码   → LIGHT_RAW_BRIGHT
 *    - LIGHT_INVERT：读数越小越亮就填 1，越大越亮就填 0
 * ==========================================================================*/
/* 实测（2026-09-26）：本模块是"越亮读数越小"，所以要反转；
   反转后 = 会长说的 (1 - 原值)：室内灯下 rawL≈1050 → 77%，手遮住 rawL≈2600 → 27% */
#define LIGHT_RAW_DARK     300U
#define LIGHT_RAW_BRIGHT   3500U
#define LIGHT_INVERT       1U

#define ADC_FILTER_N       8U          /* 滑动平均窗口：越大越稳、响应越慢 */

/* 滑动平均用的小账本：环形缓冲 + 总和 + 写指针（每个通道一份） */
static uint16_t s_buf[BSP_ADC_CH_NUM][ADC_FILTER_N];
static uint32_t s_sum[BSP_ADC_CH_NUM];
static uint8_t  s_idx[BSP_ADC_CH_NUM];
static uint16_t s_raw[BSP_ADC_CH_NUM];   /* 滤波后的"原始码" */
static uint8_t  s_ready;                 /* 0 = 还没采过（上电最初一瞬） */

/* 读一个 ADC 通道：配置通道 → 启动 → 等待转换完成 → 取值
   （每个通道都重新配置一次，慢一点但最直观——先跑通再谈优化） */
static uint16_t adc_read_channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef cfg = {0};
    uint16_t value = 0U;

    cfg.Channel      = channel;
    cfg.Rank         = ADC_REGULAR_RANK_1;
    cfg.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;   /* 采样时间拉长：高阻传感器更稳 */

    if (HAL_ADC_ConfigChannel(&hadc1, &cfg) != HAL_OK)
    {
        return 0U;
    }
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return 0U;
    }
    if (HAL_ADC_PollForConversion(&hadc1, 5U) == HAL_OK)   /* 最多等 5ms */
    {
        value = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    (void)HAL_ADC_Stop(&hadc1);
    return value;
}

/* 初始化：改成"单通道单次转换"模式 + 校准 + 清空滤波缓冲 */
void BSP_ADC_Init(void)
{
    /* CubeMX 里配的是"扫描模式 + 2 个通道"，一次转换会把两个通道都转完、
       数据寄存器只剩最后一个。这里改成"单通道单次转换"，读哪路配哪路，最直观。 */
    hadc1.Init.ScanConvMode       = DISABLE;
    hadc1.Init.NbrOfConversion    = 1U;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv   = ADC_SOFTWARE_START;
    (void)HAL_ADC_Init(&hadc1);

    HAL_ADCEx_Calibration_Start(&hadc1);   /* F1 必须先校准，否则读数有固定偏差 */

    for (uint8_t ch = 0U; ch < BSP_ADC_CH_NUM; ch++)
    {
        for (uint8_t i = 0U; i < ADC_FILTER_N; i++)
        {
            s_buf[ch][i] = 0U;
        }
        s_sum[ch] = 0U;
        s_idx[ch] = 0U;
        s_raw[ch] = 0U;
    }
    s_ready = 0U;
}

/* 采样一次（每个通道各采一个点，做滑动平均）。
   由主循环定期调用，比如每 100ms 一次 */
void BSP_ADC_Process(void)
{
    for (uint8_t ch = 0U; ch < BSP_ADC_CH_NUM; ch++)
    {
        uint16_t v = adc_read_channel((ch == BSP_ADC_CH_LIGHT) ? ADC_CHANNEL_0 : ADC_CHANNEL_1);

        s_sum[ch] -= s_buf[ch][s_idx[ch]];      /* 丢掉最老的一个 */
        s_buf[ch][s_idx[ch]] = v;               /* 写入新值 */
        s_sum[ch] += v;
        s_idx[ch] = (uint8_t)((s_idx[ch] + 1U) % ADC_FILTER_N);   /* 环形推进 */

        s_raw[ch] = (uint16_t)(s_sum[ch] / ADC_FILTER_N);   /* 平均值 = 本次输出 */
    }
    s_ready = 1U;
}

/* 取滤波后的原始码（0~4095）。ch：0 = 光敏，1 = 热敏 */
uint16_t BSP_ADC_GetRaw(uint8_t ch)
{
    return (ch < BSP_ADC_CH_NUM) ? s_raw[ch] : 0U;
}

/* 光照百分比（0~100）：先按方向反转，再线性映射到标定区间，最后钳位到 0~100 */
uint8_t BSP_ADC_LightPercent(void)
{
    int32_t v;
    int32_t pct;

    if (!s_ready)
    {
        return 0U;
    }

    v = (int32_t)s_raw[BSP_ADC_CH_LIGHT];

    if (LIGHT_INVERT)
    {
        v = (int32_t)LIGHT_RAW_BRIGHT + (int32_t)LIGHT_RAW_DARK - v;   /* 反转方向 */
    }

    pct = (v - (int32_t)LIGHT_RAW_DARK) * 100 / ((int32_t)LIGHT_RAW_BRIGHT - (int32_t)LIGHT_RAW_DARK);

    if (pct < 0)   pct = 0;      /* 超出标定范围就钳在边界 */
    if (pct > 100) pct = 100;
    return (uint8_t)pct;
}
