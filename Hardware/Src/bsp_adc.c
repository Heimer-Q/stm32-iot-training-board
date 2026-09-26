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

static uint16_t s_buf[BSP_ADC_CH_NUM][ADC_FILTER_N];
static uint32_t s_sum[BSP_ADC_CH_NUM];
static uint8_t  s_idx[BSP_ADC_CH_NUM];
static uint16_t s_raw[BSP_ADC_CH_NUM];
static uint8_t  s_ready;

static uint16_t adc_read_channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef cfg = {0};
    uint16_t value = 0U;

    cfg.Channel      = channel;
    cfg.Rank         = ADC_REGULAR_RANK_1;
    cfg.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;

    if (HAL_ADC_ConfigChannel(&hadc1, &cfg) != HAL_OK)
    {
        return 0U;
    }
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return 0U;
    }
    if (HAL_ADC_PollForConversion(&hadc1, 5U) == HAL_OK)
    {
        value = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    (void)HAL_ADC_Stop(&hadc1);
    return value;
}

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

void BSP_ADC_Process(void)
{
    for (uint8_t ch = 0U; ch < BSP_ADC_CH_NUM; ch++)
    {
        uint16_t v = adc_read_channel((ch == BSP_ADC_CH_LIGHT) ? ADC_CHANNEL_0 : ADC_CHANNEL_1);

        s_sum[ch] -= s_buf[ch][s_idx[ch]];
        s_buf[ch][s_idx[ch]] = v;
        s_sum[ch] += v;
        s_idx[ch] = (uint8_t)((s_idx[ch] + 1U) % ADC_FILTER_N);

        s_raw[ch] = (uint16_t)(s_sum[ch] / ADC_FILTER_N);
    }
    s_ready = 1U;
}

uint16_t BSP_ADC_GetRaw(uint8_t ch)
{
    return (ch < BSP_ADC_CH_NUM) ? s_raw[ch] : 0U;
}

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

    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    return (uint8_t)pct;
}
