#include "bsp_rtc.h"

static uint8_t s_valid;

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

static uint8_t two_digits(char hi, char lo)
{
    return (uint8_t)(((hi == ' ') ? 0 : (hi - '0')) * 10 + (lo - '0'));
}

void BSP_RTC_Init(void)
{
    RTC_TimeTypeDef t = {0};
    RTC_DateTypeDef d = {0};

    s_valid = 0U;

    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);   /* F1 必须读一次日期，时间才会刷新 */

    if ((d.Year < 20U) || (d.Year > 60U))         /* 没电池 → 出厂默认值，用编译时间设一次 */
    {
        RTC_TimeTypeDef nt = {0};
        RTC_DateTypeDef nd = {0};

        nt.Hours   = two_digits(__TIME__[0], __TIME__[1]);
        nt.Minutes = two_digits(__TIME__[3], __TIME__[4]);
        nt.Seconds = two_digits(__TIME__[6], __TIME__[7]);
        nd.Year    = two_digits(__DATE__[9], __DATE__[10]);
        nd.Month   = month_from_str(__DATE__);
        nd.Date    = two_digits(__DATE__[4], __DATE__[5]);
        nd.WeekDay = RTC_WEEKDAY_MONDAY;

        if (HAL_RTC_SetTime(&hrtc, &nt, RTC_FORMAT_BIN) != HAL_OK)
        {
            return;
        }
        if (HAL_RTC_SetDate(&hrtc, &nd, RTC_FORMAT_BIN) != HAL_OK)
        {
            return;
        }
    }

    s_valid = 1U;
}

uint8_t BSP_RTC_Valid(void)
{
    return s_valid;
}

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
