/**
  ******************************************************************************
  * @file    app_demo.c
  * @brief   全功能演示：3 画面 × 3 模式 + 时间设置 + 锁屏（DEMO_FINAL）
  *
  * 按键约定（3 个键 × 短按/长按/双击）
  *   K1 短按：切画面（时钟 → 图片 → 状态）
  *   K1 长按：锁屏 / 解锁（锁屏后除 K1 长按外全部忽略）
  *   K1 双击：模式循环（手动 → 光控 → 热控）
  *   K2 短按：选下一颗灯 / 图片页上一张 / 光控阈值−1 / 热控中温阈值−50 / 时间设置单位−1
  *   K2 长按：三灯全选 / 光控阈值−10 / 热控中温阈值−200
  *   K2 双击：流水灯开/关（手动模式）
  *   K3 短按：呼吸⇄常亮 / 图片页下一张 / 光控阈值+1 / 热控高温阈值+50 / 时间设置单位+1
  *   K3 长按：全灭 / 光控阈值+10 / 热控高温阈值+200
  *   K3 双击：时钟页进入/退出时间设置（加减立即写回 RTC）
  ******************************************************************************
  */

#include "app_demo.h"
#include "app_config.h"
#include "bsp_led.h"
#include "bsp_key.h"
#include "bsp_oled.h"
#include "bsp_uart.h"
#include "bsp_adc.h"
#include "bsp_rtc.h"
#include "oled_images.h"

#include <stdarg.h>
#include <stdio.h>

/* ------------------------------- 状态 ------------------------------- */
typedef enum { SCR_CLOCK = 0, SCR_IMAGE, SCR_STATUS, SCR_NUM } ScreenId;
typedef enum { MODE_NORMAL = 0, MODE_LIGHT, MODE_THERMAL, MODE_NUM } WorkMode;
enum { TU_YEAR = 0, TU_MONTH, TU_DAY, TU_HOUR, TU_MIN, TU_SEC, TU_NUM };

static ScreenId screen   = SCR_CLOCK;
static WorkMode mode     = MODE_NORMAL;
static uint8_t  locked;
static uint8_t  time_set;
static uint8_t  time_unit;
static uint8_t  img_idx;

/* 灯 */
static uint8_t lamp_sel, lamp_all, lamp_on, lamp_breath, lamp_flow;

/* 阈值（按键可调） */
static uint8_t  light_thr = 40U;      /* 光控：低于这个光照百分比开始点灯 */
static uint16_t temp_mid  = 1900U;    /* 热控中温阈值（原始码） */
static uint16_t temp_high = 2300U;    /* 热控高温阈值 */

/* 时间影子（改完立即写 RTC） */
static uint8_t t_year, t_month, t_day, t_hour, t_min, t_sec;

/* 统计与节拍 */
static uint32_t boot_ms;
static uint16_t ev_click, ev_long, ev_dbl;
static uint32_t t_adc, t_draw, t_beat;
static uint16_t line_h = 16U;
static uint8_t  oled_ok;

/* ------------------------------ 小工具 ------------------------------ */
static void show_line(uint8_t row, const char *text)
{
    OLED_SetCursor(&g_oled, 0, (int16_t)((row + 1U) * line_h));
    OLED_DrawString(&g_oled, text);
}

static void line_printf(uint8_t row, const char *fmt, ...)
{
    va_list ap;
    char    buf[24];

    va_start(ap, fmt);
    (void)vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    show_line(row, buf);
}

/* 呼吸：0→100→0，周期 CFG_BREATH_PERIOD_MS */
static uint8_t breath_level(uint32_t now)
{
    uint32_t t    = now % CFG_BREATH_PERIOD_MS;
    uint32_t half = CFG_BREATH_PERIOD_MS / 2U;
    return (uint8_t)((t < half) ? (t * 100U / half)
                                : ((CFG_BREATH_PERIOD_MS - t) * 100U / half));
}

/* 闪烁：1Hz，亮一半时间 */
static uint8_t blink_level(uint32_t now)
{
    return ((now / 500U) % 2U) ? 100U : 0U;
}

static const char *mode_name(void)
{
    switch (mode)
    {
        case MODE_LIGHT:   return "LIGHT";
        case MODE_THERMAL: return "THERMAL";
        default:           return "NORMAL";
    }
}

static const char *unit_name(void)
{
    static const char *n[TU_NUM] = {"YEAR", "MONTH", "DAY", "HOUR", "MIN", "SEC"};
    return n[time_unit % TU_NUM];
}

static const char *temp_level_str(void)
{
    uint16_t raw = BSP_ADC_GetRaw(BSP_ADC_CH_TEMP);

    if (raw >= temp_high) return "HIGH";
    if (raw >= temp_mid)  return "MID";
    return "LOW";
}

/* ------------------------------ 灯的输出 ------------------------------ */
static void lamp_update(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t  duty[LED_NUM];
    uint8_t  i;

    for (i = 0U; i < LED_NUM; i++)
    {
        duty[i] = 0U;
    }

    if (mode == MODE_LIGHT)
    {
        /* 越暗，依次点亮更多颗，并且都是呼吸状态 */
        uint32_t p   = BSP_ADC_LightPercent();
        uint32_t thr = light_thr;
        uint8_t  n   = 0U;
        uint8_t  b   = breath_level(now);

        if (p < thr)             n = 1U;
        if (p + 15U < thr)       n = 2U;
        if (p + 30U < thr)       n = 3U;
        for (i = 0U; i < n; i++)
        {
            duty[i] = b;
        }
    }
    else if (mode == MODE_THERMAL)
    {
        uint16_t raw   = BSP_ADC_GetRaw(BSP_ADC_CH_TEMP);
        uint8_t  blink = blink_level(now);

        if (raw >= temp_high)             /* 高温：三颗一起闪 */
        {
            for (i = 0U; i < LED_NUM; i++) duty[i] = blink;
        }
        else if (raw >= temp_mid)         /* 中温：单颗闪 */
        {
            duty[lamp_sel % LED_NUM] = blink;
        }
    }
    else if (lamp_flow)                   /* 手动：流水灯 */
    {
        uint8_t pos = (uint8_t)((now / 200U) % LED_NUM);
        for (i = 0U; i < LED_NUM; i++) duty[i] = (i == pos) ? 100U : 0U;
    }
    else if (lamp_all || lamp_on)         /* 手动：全亮 / 单灯 */
    {
        uint8_t base = lamp_breath ? breath_level(now) : 100U;
        if (lamp_all)
        {
            for (i = 0U; i < LED_NUM; i++) duty[i] = base;
        }
        else
        {
            duty[lamp_sel % LED_NUM] = base;
        }
    }

    for (i = 0U; i < LED_NUM; i++)
    {
        BSP_LED_SetPercent(i, duty[i]);
    }
}

/* ------------------------------ RTC 读写 ------------------------------ */
static void rtc_load(void)
{
    RTC_TimeTypeDef t = {0};
    RTC_DateTypeDef d = {0};

    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
    t_hour = (uint8_t)t.Hours;
    t_min  = (uint8_t)t.Minutes;
    t_sec  = (uint8_t)t.Seconds;
    t_year = (uint8_t)d.Year;
    t_month = (uint8_t)d.Month;
    t_day   = (uint8_t)d.Date;
}

static void rtc_store(void)
{
    RTC_TimeTypeDef t = {0};
    RTC_DateTypeDef d = {0};

    t.Hours   = t_hour;
    t.Minutes = t_min;
    t.Seconds = t_sec;
    d.Year    = t_year;
    d.Month   = t_month;
    d.Date    = t_day;
    d.WeekDay = RTC_WEEKDAY_MONDAY;

    (void)HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN);
    (void)HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN);
}

static uint8_t wrap_u8(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) v = hi;
    if (v > hi) v = lo;
    return (uint8_t)v;
}

/* 改一个单位并立即覆盖 RTC */
static void time_step(int32_t delta)
{
    switch (time_unit)
    {
        case TU_YEAR:  t_year  = wrap_u8((int32_t)t_year  + delta, 0, 99);  break;
        case TU_MONTH: t_month = wrap_u8((int32_t)t_month + delta, 1, 12);  break;
        case TU_DAY:   t_day   = wrap_u8((int32_t)t_day   + delta, 1, 31);  break;
        case TU_HOUR:  t_hour  = wrap_u8((int32_t)t_hour  + delta, 0, 23);  break;
        case TU_MIN:   t_min   = wrap_u8((int32_t)t_min   + delta, 0, 59);  break;
        case TU_SEC:   t_sec   = wrap_u8((int32_t)t_sec   + delta, 0, 59);  break;
        default: break;
    }
    rtc_store();
}

/* ------------------------------ 画面 ------------------------------ */
static void draw_clock(void)
{
    if (time_set)
    {
        /* 编辑中：把当前单位用 [ ] 括起来 */
        line_printf(0, "SET %s", unit_name());
        switch (time_unit)
        {
            case TU_YEAR:  line_printf(1, "[%02d]-%02d-%02d", t_year, t_month, t_day);      break;
            case TU_MONTH: line_printf(1, "%02d-[%02d]-%02d", t_year, t_month, t_day);      break;
            case TU_DAY:   line_printf(1, "%02d-%02d-[%02d]", t_year, t_month, t_day);      break;
            case TU_HOUR:  line_printf(1, "[%02d]:%02d:%02d", t_hour, t_min, t_sec);        break;
            case TU_MIN:   line_printf(1, "%02d:[%02d]:%02d", t_hour, t_min, t_sec);        break;
            default:       line_printf(1, "%02d:%02d:[%02d]", t_hour, t_min, t_sec);        break;
        }
        line_printf(2, "K2-%s K3+%s", unit_name(), unit_name());
        show_line(3, "K1 next  K1 hold exit");
    }
    else
    {
        rtc_load();
        line_printf(0, "TIME %02d:%02d:%02d", t_hour, t_min, t_sec);
        line_printf(1, "DATE 20%02d-%02d-%02d", t_year, t_month, t_day);
        line_printf(2, "LGT %3d%% TMP %4d", BSP_ADC_LightPercent(), BSP_ADC_GetRaw(BSP_ADC_CH_TEMP));
        if (locked)
        {
            show_line(3, "** LOCKED **");
        }
        else
        {
            line_printf(3, "LAMP %s %s", mode_name(), temp_level_str());
        }
    }
}

static void draw_image(void)
{
    OLED_SetCursor(&g_oled, 0, 0);
    OLED_DrawBitmap(&g_oled, OLED_IMG_W, OLED_IMG_H, OLED_IMG[img_idx % OLED_IMG_COUNT]);
    if (locked)
    {
        show_line(0, "** LOCKED **");
    }
}

static void draw_status(void)
{
    uint32_t up = (HAL_GetTick() - boot_ms) / 1000U;

    line_printf(0, "MODE %s", mode_name());
    if (locked)
    {
        show_line(1, "** LOCKED **");
    }
    else
    {
        line_printf(1, "UP %02lu:%02lu:%02lu",
                    (unsigned long)(up / 3600U),
                    (unsigned long)((up / 60U) % 60U),
                    (unsigned long)(up % 60U));
    }
    line_printf(2, "LGT %3d%% THR %2d", BSP_ADC_LightPercent(), light_thr);
    line_printf(3, "TMP%4d M%02d H%02d",
                BSP_ADC_GetRaw(BSP_ADC_CH_TEMP),
                temp_mid / 100U, temp_high / 100U);
}

static void screen_draw(void)
{
    if (!oled_ok)
    {
        return;
    }

    if (screen == SCR_IMAGE)
    {
        /* 图片页整屏重画，不需要先清屏 */
        draw_image();
    }
    else
    {
        switch (screen)
        {
            case SCR_CLOCK:  draw_clock();  break;
            case SCR_STATUS: draw_status(); break;
            default: break;
        }
    }
    BSP_OLED_Refresh();
}

/* ------------------------------ 按键 ------------------------------ */
static void count_event(BSP_KEY_Event e)
{
    if (e == BSP_KEY_EVENT_CLICK)       ev_click++;
    else if (e == BSP_KEY_EVENT_LONG)   ev_long++;
    else if (e == BSP_KEY_EVENT_DOUBLE) ev_dbl++;
}

static void handle_keys(void)
{
    BSP_KEY_Event e1, e2, e3;

    e1 = BSP_KEY_GetEvent(BSP_KEY_1);
    e2 = BSP_KEY_GetEvent(BSP_KEY_2);
    e3 = BSP_KEY_GetEvent(BSP_KEY_3);
    count_event(e1); count_event(e2); count_event(e3);

    /* ---------- 锁屏：只认 K1 长按 ---------- */
    if (locked)
    {
        if (e1 == BSP_KEY_EVENT_LONG)
        {
            locked = 0U;
            BSP_UART_Printf("[ui  ] unlock\r\n");
        }
        return;
    }

    /* ---------- K1 ---------- */
    switch (e1)
    {
    case BSP_KEY_EVENT_CLICK:
        if (time_set)
        {
            if (++time_unit >= TU_NUM)     /* 转完一圈自动退出 */
            {
                time_unit = 0U;
                time_set  = 0U;
                BSP_UART_Printf("[time] exit set\r\n");
            }
        }
        else
        {
            screen = (ScreenId)((screen + 1U) % (uint8_t)SCR_NUM);
            if (oled_ok) OLED_Clear(&g_oled);
            BSP_UART_Printf("[ui  ] screen -> %d\r\n", (int)screen);
        }
        break;
    case BSP_KEY_EVENT_LONG:
        if (time_set)
        {
            time_set = 0U;
            BSP_UART_Printf("[time] exit set (hold)\r\n");
        }
        else
        {
            locked = 1U;
            BSP_UART_Printf("[ui  ] LOCK\r\n");
        }
        break;
    case BSP_KEY_EVENT_DOUBLE:
        if (!time_set)
        {
            mode = (WorkMode)((mode + 1U) % (uint8_t)MODE_NUM);
            BSP_UART_Printf("[mode] -> %s\r\n", mode_name());
        }
        break;
    default:
        break;
    }

    /* ---------- 时间设置：K2/K3 加减（长按连调） ---------- */
    if (time_set)
    {
        if ((e2 == BSP_KEY_EVENT_CLICK) || (e2 == BSP_KEY_EVENT_LONG_REPEAT)) time_step(-1);
        if ((e3 == BSP_KEY_EVENT_CLICK) || (e3 == BSP_KEY_EVENT_LONG_REPEAT)) time_step(+1);
        return;
    }

    /* ---------- 图片页：K2 上一张 / K3 下一张 ---------- */
    if (screen == SCR_IMAGE)
    {
        if (e2 == BSP_KEY_EVENT_CLICK)
        {
            img_idx = (uint8_t)((img_idx + OLED_IMG_COUNT - 1U) % OLED_IMG_COUNT);
            BSP_UART_Printf("[img ] -> %d\r\n", img_idx + 1);
        }
        if (e3 == BSP_KEY_EVENT_CLICK)
        {
            img_idx = (uint8_t)((img_idx + 1U) % OLED_IMG_COUNT);
            BSP_UART_Printf("[img ] -> %d\r\n", img_idx + 1);
        }
        return;
    }

    /* ---------- 光控模式：K2/K3 调阈值 ---------- */
    if (mode == MODE_LIGHT)
    {
        if (e2 == BSP_KEY_EVENT_LONG)          light_thr = wrap_u8((int32_t)light_thr - 10, 5, 95);
        if (e2 == BSP_KEY_EVENT_CLICK)         light_thr = wrap_u8((int32_t)light_thr - 1,  5, 95);
        if (e2 == BSP_KEY_EVENT_LONG_REPEAT)   light_thr = wrap_u8((int32_t)light_thr - 1,  5, 95);
        if (e3 == BSP_KEY_EVENT_LONG)          light_thr = wrap_u8((int32_t)light_thr + 10, 5, 95);
        if (e3 == BSP_KEY_EVENT_CLICK)         light_thr = wrap_u8((int32_t)light_thr + 1,  5, 95);
        if (e3 == BSP_KEY_EVENT_LONG_REPEAT)   light_thr = wrap_u8((int32_t)light_thr + 1,  5, 95);
        BSP_UART_Printf("[light] thr=%d%%  lgt=%d%%\r\n", light_thr, BSP_ADC_LightPercent());
        return;
    }

    /* ---------- 热控模式：K2 调中温 / K3 调高温 ---------- */
    if (mode == MODE_THERMAL)
    {
        if (e2 == BSP_KEY_EVENT_LONG)        temp_mid = (uint16_t)((temp_mid > 300U) ? (temp_mid - 200U) : 100U);
        if (e2 == BSP_KEY_EVENT_CLICK)       temp_mid = (uint16_t)((temp_mid > 100U) ? (temp_mid - 50U)  : 100U);
        if (e2 == BSP_KEY_EVENT_LONG_REPEAT) temp_mid = (uint16_t)((temp_mid > 100U) ? (temp_mid - 50U)  : 100U);
        if (e3 == BSP_KEY_EVENT_LONG)        temp_high += 200U;
        if (e3 == BSP_KEY_EVENT_CLICK)       temp_high += 50U;
        if (e3 == BSP_KEY_EVENT_LONG_REPEAT) temp_high += 50U;
        if (temp_high <= temp_mid)           temp_high = (uint16_t)(temp_mid + 100U);
        BSP_UART_Printf("[temp ] mid=%u high=%u  raw=%u\r\n",
                        temp_mid, temp_high, BSP_ADC_GetRaw(BSP_ADC_CH_TEMP));
        return;
    }

    /* ---------- 手动模式：灯 ---------- */
    if (e2 == BSP_KEY_EVENT_CLICK)                 /* 选下一颗灯 */
    {
        lamp_flow = 0U;
        lamp_all  = 0U;
        if (lamp_on) lamp_sel = (uint8_t)((lamp_sel + 1U) % LED_NUM);
        else         { lamp_sel = 0U; lamp_on = 1U; }
        BSP_UART_Printf("[lamp] -> LED%d\r\n", lamp_sel + 1);
    }
    if (e2 == BSP_KEY_EVENT_LONG)                  /* 全选 */
    {
        lamp_flow = 0U;
        lamp_all  = 1U;
        lamp_on   = 1U;
        BSP_UART_Printf("[lamp] ALL\r\n");
    }
    if (e2 == BSP_KEY_EVENT_DOUBLE)                /* 流水灯开关 */
    {
        lamp_flow = (uint8_t)(!lamp_flow);
        BSP_UART_Printf("[lamp] flow %s\r\n", lamp_flow ? "ON" : "OFF");
    }
    if (e3 == BSP_KEY_EVENT_CLICK)                 /* 呼吸⇄常亮 */
    {
        lamp_breath = (uint8_t)(!lamp_breath);
        BSP_UART_Printf("[lamp] breath %s\r\n", lamp_breath ? "ON" : "OFF");
    }
    if (e3 == BSP_KEY_EVENT_LONG)                  /* 全灭 */
    {
        lamp_all  = 0U;
        lamp_on   = 0U;
        lamp_flow = 0U;
        BSP_UART_Printf("[lamp] OFF\r\n");
    }
    if ((e3 == BSP_KEY_EVENT_DOUBLE) && (screen == SCR_CLOCK))   /* 进时间设置 */
    {
        time_set  = 1U;
        time_unit = TU_HOUR;
        rtc_load();
        BSP_UART_Printf("[time] enter set\r\n");
    }
}

/* ------------------------------ 入口 ------------------------------ */
void APP_Demo_Init(void)
{
    int ret;

    BSP_UART_Init();
    BSP_UART_Printf("\r\n=== WULIAN BOARD DEMO (FINAL) ===\r\n");

    BSP_LED_Init();
    BSP_KEY_Init();
    BSP_ADC_Init();
    BSP_RTC_Init();
    BSP_LED_AllOff();

    ret = BSP_OLED_Init();
    oled_ok = (ret == 0) ? 1U : 0U;
    BSP_UART_Printf("[oled] %s (ret=%d)\r\n", oled_ok ? "OK" : "FAIL", ret);

    line_h = oled_ok ? OLED_GetFontHeight(&g_oled) : 16U;
    if (line_h == 0U) line_h = 16U;

    rtc_load();
    BSP_UART_Printf("[rtc ] %02d:%02d:%02d 20%02d-%02d-%02d\r\n",
                    t_hour, t_min, t_sec, t_year, t_month, t_day);
    BSP_UART_Printf("[img ] %d images, %d bytes\r\n", OLED_IMG_COUNT, OLED_IMG_W / 8 * OLED_IMG_H);
    BSP_UART_Printf("[tips] K1 screen/hold=lock/double=mode | K2 sel/all/flow | K3 breath/off/set-time\r\n");

    lamp_sel = 0U; lamp_all = 0U; lamp_on = 0U; lamp_breath = 0U; lamp_flow = 0U;
    screen = SCR_CLOCK; mode = MODE_NORMAL; locked = 0U; time_set = 0U;

    boot_ms = HAL_GetTick();
    t_adc = t_draw = t_beat = boot_ms;

    if (oled_ok)
    {
        OLED_Clear(&g_oled);
        screen_draw();
    }
    lamp_update();
}

void APP_Demo_Process(void)
{
    uint32_t now = HAL_GetTick();

    handle_keys();
    lamp_update();

    if (now - t_adc >= CFG_OLED_REFRESH_MS)
    {
        t_adc += CFG_OLED_REFRESH_MS;
        BSP_ADC_Process();
    }

    if (oled_ok && (now - t_draw >= 200U))
    {
        t_draw += 200U;
        screen_draw();
    }

    if (now - t_beat >= 2000U)
    {
        t_beat += 2000U;
        BSP_UART_Printf("[tick] scr=%d mode=%s lock=%d lgt=%d%% rawT=%d lamp=%d/%d/%d/%d ev=%d/%d/%d\r\n",
                        (int)screen, mode_name(), locked,
                        BSP_ADC_LightPercent(), BSP_ADC_GetRaw(BSP_ADC_CH_TEMP),
                        lamp_on, lamp_all, lamp_breath, lamp_flow,
                        ev_click, ev_long, ev_dbl);
    }
}
