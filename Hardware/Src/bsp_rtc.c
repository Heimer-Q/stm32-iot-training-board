/**
  ******************************************************************************
  * @file    bsp_rtc.c
  * @brief   板载 RTC（实时时钟）：走时、校时、断电保持
  *
  * == 新人导读 ================================================================
  * 1. STM32F1 的 RTC 有个"奇怪"的设计，必须先知道：
  *    硬件里其实只有一个 32 位"秒计数器"（CNT），没有"年月日"寄存器！
  *    HAL 库用软件方式维护日期，而且日期放在【内存】里（RAM），
  *    一复位就没了——这也是"设置完时间按复位就回到初始"的原因。
  *
  * 2. 我们的解决办法（两层保险）：
  *    ① 日期同步保存在【备份寄存器 DR2】里（备份域只要不断电就不丢），
  *       上电时从 DR2 恢复日期，秒计数器本来就是连着走的 → 时间和日期都还在；
  *    ② 备份寄存器也丢了（真拔电、板子没有纽扣电池）→ 用【固件编译时刻】
  *       先校一个"大概合理"的时间，屏上至少看到钟在走。
  *
  * 3. 什么时候会"重设时间"？
  *    只有真断电（备份域掉电）之后第一次上电才会重设；
  *    平时按复位键、重新下载程序，时间都会保持。
  ******************************************************************************
  */

#include "bsp_rtc.h"

static uint8_t s_valid;      /* 1 = 初始化成功，时间可用 */

/* "Oct" 这样的月份缩写 → 数字（给 __DATE__ 字符串用，__DATE__ 是编译时刻） */
static uint8_t month_from_str(const char *m)
{
    static const char names[12][4] = {"Jan","Feb","Mar","Apr","May","Jun",
                                      "Jul","Aug","Sep","Oct","Nov","Dec"};
    for (uint8_t i = 0U; i < 12U; i++)
    {
        if (m[0] == names[i][0] && m[1] == names[i][1] && m[2] == names[i][2])
        {
            return (uint8_t)(i + 1U);
        }
    }
    return 1U;
}

/* 把 __DATE__/__TIME__ 里的两个字符（比如 "17"）转成数字 */
static uint8_t two_digits(char hi, char lo)
{
    return (uint8_t)(((hi == ' ') ? 0 : (hi - '0')) * 10 + (lo - '0'));
}

/* 初始化：优先从备份寄存器恢复，其次用编译时刻校时。
   日期打包格式（16 位）：年(7位)<<9 | 月(4位)<<5 | 日(5位) */
void BSP_RTC_Init(void)
{
    RTC_TimeTypeDef t = {0};
    RTC_DateTypeDef d = {0};
    RTC_DateTypeDef nd = {0};
    uint16_t dr2;
    uint8_t  y, mo, da;

    s_valid = 0U;

    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);   /* F1 必须读一次日期，时间才会刷新 */

    /* F1 的 RTC 硬件只有秒计数器、没有日期寄存器：HAL 的日期存在 RAM 里，复位就丢。
       所以把日期同步保存在备份寄存器 DR2（由 BSP_RTC_SyncDateBkup 每 2 秒维护）。 */
    dr2 = (uint16_t)HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR2);
    y  = (uint8_t)((dr2 >> 9) & 0x7FU);
    mo = (uint8_t)((dr2 >> 5) & 0x0FU);
    da = (uint8_t)(dr2 & 0x1FU);

    if ((HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) == 0x32F2U) &&
        (mo >= 1U) && (mo <= 12U) && (da >= 1U) && (da <= 31U) &&
        (y >= 20U) && (y <= 60U))
    {
        /* 备份域一直有电：时间（秒计数器）本来就连着走，只需把日期恢复到内存结构 */
        nd.Year  = y;
        nd.Month = mo;
        nd.Date  = da;
        (void)HAL_RTC_SetDate(&hrtc, &nd, RTC_FORMAT_BIN);
    }
    else if ((d.Year < 20U) || (d.Year > 60U))
    {
        /* 备份域掉过电（板子没有 RTC 电池）：用编译时刻全校一次，并把日期存入 DR2 */
        RTC_TimeTypeDef nt = {0};
        RTC_DateTypeDef nb = {0};

        nt.Hours   = two_digits(__TIME__[0], __TIME__[1]);
        nt.Minutes = two_digits(__TIME__[3], __TIME__[4]);
        nt.Seconds = two_digits(__TIME__[6], __TIME__[7]);
        nb.Year    = two_digits(__DATE__[9], __DATE__[10]);
        nb.Month   = month_from_str(__DATE__);
        nb.Date    = two_digits(__DATE__[4], __DATE__[5]);
        nb.WeekDay = RTC_WEEKDAY_MONDAY;

        if (HAL_RTC_SetTime(&hrtc, &nt, RTC_FORMAT_BIN) != HAL_OK)
        {
            return;
        }
        if (HAL_RTC_SetDate(&hrtc, &nb, RTC_FORMAT_BIN) != HAL_OK)
        {
            return;
        }
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR2,
                            ((uint32_t)nb.Year << 9) |
                            ((uint32_t)nb.Month << 5) |
                            (uint32_t)nb.Date);
    }

    s_valid = 1U;
}

/* 每 2 秒由主循环调用：把当前日期（含跨天进位）同步进备份寄存器 DR2 */
void BSP_RTC_SyncDateBkup(void)
{
    RTC_DateTypeDef d = {0};
    uint16_t packed;

    if (!s_valid)
    {
        return;
    }

    /* 顺便让 HAL 处理天数进位：CNT 超过 24 小时时压回并把日期 +1 天 */
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);

    packed = (uint16_t)(((uint16_t)d.Year << 9) | ((uint16_t)d.Month << 5) | (uint16_t)d.Date);
    if ((uint16_t)HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR2) != packed)
    {
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR2, (uint32_t)packed);
    }
}

/* 时间是否有效（初始化失败时上层可以据此提示） */
uint8_t BSP_RTC_Valid(void)
{
    return s_valid;
}

/* 取当前时分秒（参数可传 0 表示不要那一项） */
void BSP_RTC_Get(uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    RTC_TimeTypeDef t = {0};
    RTC_DateTypeDef d = {0};

    if (!s_valid)
    {
        if (hour)   *hour = 0U;
        if (minute) *minute = 0U;
        if (second) *second = 0U;
        return;
    }

    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);

    if (hour)   *hour   = (uint8_t)t.Hours;
    if (minute) *minute = (uint8_t)t.Minutes;
    if (second) *second = (uint8_t)t.Seconds;
}
