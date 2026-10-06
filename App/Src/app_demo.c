/**
  ******************************************************************************
  * @file    app_demo.c
  * @brief   全功能演示：6 页面 x 3 模式 + 时间设置 + 曲库音乐页（DEMO_FINAL；锁屏 2026-10-06 已移除）
  *
  * 页面顺序（K1 单击循环）：信息 -> 状态 -> 时间 -> 灯控 -> 图片 -> 音乐
  *
  * 按键约定（2026-10-06 重构后）
  *   K1 单击：**永远切页**（时间设置中＝保存并退出后再切页；离开音乐页＝歌自动暂停）
  *   K1 长按：**只在状态页**＝切模式（常态 -> 光控 -> 热控），并响 1/2/3 声；时间设置中＝退出设置；其它页不动作
  *   K1 双击：不使用
  *   K2 / K3：**只跟当前页面有关**——
  *     信息页：无动作
  *     状态页：K2 单击＝切参数（THR->MID->HIGH）、K2 长按＝+1、K3 长按＝-1、K3 单击＝恢复默认
  *     时间页：K3 长按＝进时间设置
  *     灯控页：K2 单击＝选下一颗、K2 双击＝呼吸⇄常亮、K2 长按＝全亮⇄全灭；
  *             K3 单击＝开/关选中的那颗、K3 长按＝流水灯开/关
  *     图片页：K2/K3 单击＝上一张/下一张动图，长按＝变慢/变快
  *     音乐页：K2 单击＝播放/暂停（暂停后从当前音符继续）、K3 单击＝切歌（停到待播）
  *   （光控 / 热控模式下，灯控页的手动键不响应——灯由传感器控制）
  *
  * 时间设置里：
  *   K2 单击 = 六个单位循环（年->月->日->时->分->秒->年）；
  *   K2 长按 = 当前单位 -1、K3 长按 = 当前单位 +1（长按连调），**每改一次立即写回 RTC**；
  *   K1 单击 / 长按 = 退出设置（单击同时切页）。进设置时置 k3_swallow：**松手前吞掉 K3 事件**，
  *   避免"长按进设置、手没松就把时间改了"。
  *
  * 光控 / 热控都是"三档渐进"（同一个套路）：
  *   光控：越暗亮的颗数越多（1->2->3 颗，呼吸状态）
  *   热控：越热亮的颗数越多（1->2->3 颗，闪烁状态）
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
#include "bsp_beep.h"          /* 板载蜂鸣器（PB1 / TIM3_CH4）：开机提示音 + 按键音 */
#include "app_songs.h"         /* 曲库 + 共用播放器（音乐页） */
#include "oled_gif.h"          /* 动图（Tools/make_gif.py 生成） */

#include <stdarg.h>
#include <stdio.h>

/* ------------------------------- 状态 ------------------------------- */
/* 画面顺序（也是 K1 短按的循环顺序 + 上电默认停在第一页）
   0 信息页 → 1 状态页（THR/MID/HIGH）→ 2 时间页 → 3 灯控页 → 4 图片页 → 5 音乐页（曲库） */
typedef enum { SCR_INFO = 0, SCR_STATUS, SCR_CLOCK, SCR_LAMP, SCR_IMAGE, SCR_MUSIC, SCR_NUM } ScreenId;
typedef enum { MODE_NORMAL = 0, MODE_LIGHT, MODE_THERMAL, MODE_NUM } WorkMode;
enum { TU_YEAR = 0, TU_MONTH, TU_DAY, TU_HOUR, TU_MIN, TU_SEC, TU_NUM };

static ScreenId screen   = SCR_INFO;
static WorkMode mode     = MODE_NORMAL;
static uint8_t  time_set;
static uint8_t  k3_swallow;   /* 进时间设置那一次是按着 K3 完成的：松手前吞掉 K3 事件，避免顺手改时间 */
static uint8_t  thr_sel;      /* 状态页当前选中的参数：0=THR 1=MID 2=HIGH */
static uint8_t  time_unit;
static uint8_t  anim_idx;                               /* 当前是第几张动图 */
static uint8_t  gif_frame;                              /* 当前动图播到第几帧 */
static uint16_t gif_delay_ms;                           /* 每帧时长，可在图片页用按键调 */
static uint32_t t_gif;

/* 灯 */
static uint8_t lamp_sel, lamp_mask, lamp_breath, lamp_flow;   /* mask: bit0/1/2 = LED1/2/3 亮灭 */

/* 阈值（按键可调） */
static uint8_t  light_thr = CFG_LIGHT_THR_DEFAULT;   /* 光控：低于这个光照百分比开始点灯 */
/* 热控阈值：按满量程百分比换算成原始码（4095 满量程） */
static uint16_t temp_mid  = (uint16_t)(4095UL * CFG_TEMP_MID_PCT  / 100UL);   /* 45% ≈ 1842 */
static uint16_t temp_high = (uint16_t)(4095UL * CFG_TEMP_HIGH_PCT / 100UL);   /* 30% ≈ 1228 */

/* 时间影子（改完立即写 RTC） */
static uint8_t t_year, t_month, t_day, t_hour, t_min, t_sec;

/* 统计与节拍 */
static uint32_t boot_ms;
static uint16_t ev_click, ev_long, ev_dbl;
static uint8_t  beep_left;              /* 切模式还要响几声 */
static uint32_t beep_next_ms;           /* 下一声最早什么时候响 */
static uint32_t t_adc, t_draw, t_beat;
static uint16_t line_h = 16U;
static uint8_t  oled_ok;

/* ------------------------------ 小工具 ------------------------------ */
/* 居中的一行（信息页用：按当前字体实际宽度算 x） */
static void show_line_center(uint8_t row, const char *text)
{
    uint16_t w = OLED_GetStrWidth(&g_oled, text);

    OLED_SetCursor(&g_oled, (int16_t)((128 - (int16_t)w) / 2), (int16_t)((row + 1U) * line_h));
    OLED_DrawString(&g_oled, text);
}

/* 画一行居中文字；sel_idx >= 0 时把从第 sel_idx 个字符起的 2 字符反白（白底黑字）。
   时间页专用：日期串的年/月/日和钟串的时/分/秒都是 2 字符，正好整段反白。 */
static void show_line_center_hl(uint8_t row, const char *text, int8_t sel_idx)
{
    int16_t  y = (int16_t)((row + 1U) * line_h);
    uint16_t w = OLED_GetStrWidth(&g_oled, text);
    int16_t  x = (int16_t)((128 - (int16_t)w) / 2);
    char     seg[3];

    OLED_SetCursor(&g_oled, x, y);
    OLED_DrawString(&g_oled, text);          /* 整行先正常画 */

    if (sel_idx < 0)
    {
        return;
    }

    seg[0] = text[sel_idx];
    seg[1] = text[sel_idx + 1];
    seg[2] = '\0';
    BSP_OLED_DrawTextInverseAt((int16_t)(x + (int16_t)sel_idx * 8), y, seg);   /* 选中的 2 字符反白 */
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

/* 温度占满量程的百分比（0—100） */
static uint32_t temp_percent(void)
{
    return ((uint32_t)BSP_ADC_GetRaw(BSP_ADC_CH_TEMP) * 100U + 2047U) / 4095U;   /* 四舍五入 */
}

/* 原始码 → 百分比（四舍五入）。直接截断会出现"阈值 42% 显示成 41%"的假象 */
static uint32_t raw_to_pct(uint16_t raw)
{
    return ((uint32_t)raw * 100U + 2047U) / 4095U;
}

/* 光控档位：把 [遮住下限 … 阈值上限] 均分三份，返回该亮几颗（0—3） */
static uint8_t light_band_count(void)
{
    uint32_t p    = BSP_ADC_LightPercent();
    uint32_t off  = light_thr;              /* 上限（按键可调，默认 77%） */
    uint32_t on   = CFG_LIGHT_ON_PCT;       /* 下限（实测遮住值 12%） */
    uint32_t span = (off > on) ? (off - on) : 1U;

    if (p >= off) return 0U;                /* 够亮：不亮灯 */
    if (p >= on + span * 2U / 3U) return 1U;
    if (p >= on + span / 3U)      return 2U;
    return 3U;
}

/* 热控档位：把 [高温端 … 中温端] 均分三份，返回该亮几颗（0—3）
   —— 和光控同一套逻辑，越热/越暗，亮的颗数越多 */
static uint8_t temp_band_count(void)
{
    /* 注意：这里必须统一用"原始码"比较。
       之前拿百分比(47) 去比原始码(1924)，条件永远不成立，导致任何温度都返回 3 颗。 */
    uint32_t raw  = BSP_ADC_GetRaw(BSP_ADC_CH_TEMP);
    uint32_t hi   = temp_mid;                       /* 起点（原始码，按键可调） */
    uint32_t lo   = temp_high;                      /* 终点（原始码，按键可调） */
    uint32_t span = (hi > lo) ? (hi - lo) : 1U;

#if CFG_TEMP_INVERT
    if (raw >= hi) return 0U;                       /* 室温：读数大 → 不亮 */
    if (raw >= lo + span * 2U / 3U) return 1U;      /* 有点热：1 颗闪 */
    if (raw >= lo + span / 3U)      return 2U;      /* 比较热：2 颗闪 */
    return 3U;                                      /* 很热：3 颗一起闪 */
#else
    if (raw <= lo) return 3U;
    if (raw >= hi) return 0U;
    if (raw >= hi - span / 3U)      return 1U;
    if (raw >= hi - span * 2U / 3U) return 2U;
    return 3U;
#endif
}

/* --------------------------- 模式提示音（非阻塞） ---------------------------
   切模式后响 N 声：常态 1 声、光控 2 声、热敏 3 声。
   只发一声、隔 200ms 再发下一声，靠 BSP_BEEP_Task() 收尾，不占用主循环。 */
static void mode_beep_start(void)
{
    beep_left    = (uint8_t)((uint8_t)mode + 1U);
    beep_next_ms = HAL_GetTick();
}

static void mode_beep_task(void)
{
    if ((beep_left == 0U) || (HAL_GetTick() < beep_next_ms) || BSP_BEEP_IsBusy())
    {
        return;
    }
    BSP_BEEP_Beep(80U);                     /* 80ms 一声（默认音调） */
    beep_left--;
    beep_next_ms = HAL_GetTick() + 200U;    /* 声与声之间隔 200ms，听得清 */
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
        /* 越暗，依次点亮更多颗，并且都是呼吸状态（档位算法见 light_band_count） */
        uint8_t n = light_band_count();
        uint8_t b = breath_level(now);

        for (i = 0U; i < n; i++)
        {
            duty[i] = b;
        }
    }
    else if (mode == MODE_THERMAL)
    {
        /* 三档：越热亮的颗数越多（1 → 2 → 3 颗），都是闪烁 */
        uint8_t n     = temp_band_count();
        uint8_t blink = blink_level(now);

        for (i = 0U; i < n; i++)
        {
            duty[i] = blink;
        }
    }
    else if (lamp_flow)                   /* 手动：流水灯 */
    {
        uint8_t pos = (uint8_t)((now / 200U) % LED_NUM);
        for (i = 0U; i < LED_NUM; i++) duty[i] = (i == pos) ? 100U : 0U;
    }
    else if (lamp_mask != 0U)             /* 手动：按位亮灯（每颗独立开/关） */
    {
        uint8_t base = lamp_breath ? breath_level(now) : 100U;
        for (i = 0U; i < LED_NUM; i++)
        {
            if (lamp_mask & (uint8_t)(1U << i))
            {
                duty[i] = base;
            }
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

/* --------------------- 参数/状态页：调 THR / MID / HIGH ---------------------
   THR  = 光控上限（百分比，5—95 循环）
   MID  = 热敏"开始亮"的起点（原始码，步长 ≈1%）
   HIGH = 热敏"最亮"的终点（原始码；按 CFG_TEMP_INVERT 保持 HIGH 比 MID 更"热"）
   调完即时生效：lamp_update() 在主循环里紧跟 handle_keys()，光控/热控下灯会立刻跟着变。 */
#define THR_TEMP_STEP   ((int32_t)(4095U / 100U))   /* 温度阈值步长 ≈ 1% */

static void thr_step(int32_t dir)
{
    int32_t v;

    if (thr_sel == 0U)
    {
        light_thr = wrap_u8((int32_t)light_thr + dir, 5, 95);
        BSP_UART_Printf("[thr ] THR=%d%%   LGT=%d%%\r\n", light_thr, BSP_ADC_LightPercent());
        return;
    }

    v = (thr_sel == 1U) ? (int32_t)temp_mid : (int32_t)temp_high;
    v += dir * THR_TEMP_STEP;
    if (v < 100)  v = 100;
    if (v > 4000) v = 4000;

    if (thr_sel == 1U) temp_mid  = (uint16_t)v;
    else               temp_high = (uint16_t)v;

#if CFG_TEMP_INVERT
    if (temp_high >= temp_mid)  temp_high = (uint16_t)((temp_mid > 200U) ? (temp_mid - 100U) : 100U);
#else
    if (temp_high <= temp_mid)  temp_high = (uint16_t)(temp_mid + 100U);
#endif
    BSP_UART_Printf("[thr ] MID=%u(%u%%)  HIGH=%u(%u%%)  rawT=%u\r\n",
                    temp_mid, (unsigned)raw_to_pct(temp_mid),
                    temp_high, (unsigned)raw_to_pct(temp_high),
                    BSP_ADC_GetRaw(BSP_ADC_CH_TEMP));
}

static void thr_reset(void)
{
    if (thr_sel == 0U)
    {
        light_thr = CFG_LIGHT_THR_DEFAULT;
    }
    else if (thr_sel == 1U)
    {
        temp_mid = (uint16_t)(4095UL * CFG_TEMP_MID_PCT / 100UL);
    }
    else
    {
        temp_high = (uint16_t)(4095UL * CFG_TEMP_HIGH_PCT / 100UL);
    }
    BSP_UART_Printf("[thr ] reset sel=%u -> THR=%d%%  MID=%u  HIGH=%u\r\n",
                    thr_sel, light_thr, temp_mid, temp_high);
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
    BSP_UART_Printf("[time] 20%02d-%02d-%02d %02d:%02d:%02d\r\n",
                    t_year, t_month, t_day, t_hour, t_min, t_sec);
}

/* ------------------------------ 画面 ------------------------------ */
/* 时间页（2026-10-06 晚改版）：
   平时＝日期行 + 时间行（都居中）；设置中＝选中单位"白底黑字"反白，
   第 1、4 行放键位提示（K2 换单位 / K1 退出 / 长按 K2- K3+ 加减）。 */
static void draw_clock(void)
{
    char   dbuf[12];
    char   tbuf[12];
    int8_t dsel = -1;               /* 日期行反白起点（字符索引），-1 = 不反白 */
    int8_t tsel = -1;

    if (!time_set)
    {
        rtc_load();                 /* 平时显示实时时间（设置中显示编辑中的影子值） */
    }

    (void)snprintf(dbuf, sizeof(dbuf), "%02d-%02d-%02d", t_year, t_month, t_day);
    (void)snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d", t_hour, t_min, t_sec);

    if (time_set)
    {
        show_line_center(0U, "K2:UNIT K1:EXIT");
        show_line_center(3U, "HOLD K2- K3+");
        switch (time_unit)
        {
            case TU_YEAR:  dsel = 0; break;
            case TU_MONTH: dsel = 3; break;
            case TU_DAY:   dsel = 6; break;
            case TU_HOUR:  tsel = 0; break;
            case TU_MIN:   tsel = 3; break;
            default:       tsel = 6; break;
        }
    }

    show_line_center_hl(1U, dbuf, dsel);
    show_line_center_hl(2U, tbuf, tsel);
}

static void draw_image(void)
{
    const OledAnim *a = &OLED_ANIMS[anim_idx % OLED_ANIM_COUNT];

    /* 动图：每帧整屏重画，先清屏避免上一帧残留 */
    OLED_Clear(&g_oled);
    OLED_SetCursor(&g_oled, (int16_t)((128 - (int16_t)a->w) / 2),
                            (int16_t)((64 - (int16_t)a->h) / 2));
    OLED_DrawBitmap(&g_oled, a->w, a->h, a->frames[gif_frame % a->count]);
}

/* 参数/状态页（2026-10-06 重构）：
   第 1—3 行 = 三个可调参数（当前选中的那行"白底黑字"反白，只包文字）；
   第 4 行 = 当前模式 + 两个实测值（光照% / 温度%）。
   按键：K2 单击切参数、K2 长按 +1、K3 长按 −1、K3 单击恢复默认（见 handle_keys）。 */
static void draw_status(void)
{
    char buf[20];
    uint8_t i;

    for (i = 0U; i < 3U; i++)
    {
        switch (i)
        {
            case 0U:
                (void)snprintf(buf, sizeof(buf), "THR  %2d%%   L %2d%%",
                               (int)light_thr, (int)BSP_ADC_LightPercent());
                break;
            case 1U:
                (void)snprintf(buf, sizeof(buf), "MID  %2d%%   T %2d%%",
                               (int)raw_to_pct(temp_mid), (int)temp_percent());
                break;
            default:
                (void)snprintf(buf, sizeof(buf), "HIGH %2d%%", (int)raw_to_pct(temp_high));
                break;
        }

        if (i == thr_sel)
        {
            BSP_OLED_DrawTextInverse((int16_t)((i + 1U) * line_h), buf);   /* 白底黑字＝当前在调这个 */
        }
        else
        {
            show_line_center(i, buf);
        }
    }

    (void)snprintf(buf, sizeof(buf), "MODE %s", mode_name());
    show_line_center(3U, buf);
}

/* 灯控页（2026-10-06 新增）：手动控灯都集中到这一页（原来散在信息页/时钟页）。
   第 4 行：常态模式显示键位提示；光控/热控显示模式名（此时手动键不响应，灯由传感器控制）。 */
/* 灯控页（2026-10-06 晚二版）：三颗灯独立开关。
   第 1 行 = 三个状态位（1=亮 0=灭，从左到右 LED1/2/3），当前选中的那位反白；
   K2 单击＝选下一颗、K2 双击＝呼吸⇄常亮、K2 长按＝全亮⇄全灭；
   K3 单击＝开/关选中的那颗、K3 长按＝流水灯开/关。 */
static void draw_lamp(void)
{
    char     buf[20];
    char     seg[2];
    uint16_t w;
    int16_t  x;
    int16_t  y = (int16_t)(1U * line_h);

    /* 第 1 行：LAMP + 三个状态位（先整行画，再把选中位反白） */
    (void)snprintf(buf, sizeof(buf), "LAMP  %d %d %d",
                   (lamp_mask & 0x1U) ? 1 : 0,
                   (lamp_mask & 0x2U) ? 1 : 0,
                   (lamp_mask & 0x4U) ? 1 : 0);
    w = OLED_GetStrWidth(&g_oled, buf);
    x = (int16_t)((128 - (int16_t)w) / 2);
    OLED_SetCursor(&g_oled, x, y);
    OLED_DrawString(&g_oled, buf);

    seg[0] = (char)('0' + ((lamp_mask >> lamp_sel) & 1U));
    seg[1] = '\0';
    BSP_OLED_DrawTextInverseAt((int16_t)(x + (int16_t)(6U + 2U * lamp_sel) * 8), y, seg);

    (void)snprintf(buf, sizeof(buf), "FLOW:%d BRTH:%d",
                   lamp_flow ? 1 : 0, lamp_breath ? 1 : 0);
    show_line_center(1U, buf);

    show_line_center(2U, "K2 SEL  K3 TOG");
    show_line_center(3U, "K2H:ALL K3H:FLOW");
}

static void draw_music(void)
{
    char        buf[20];
    const char *st;

    (void)snprintf(buf, sizeof(buf), "SONG %u/%u",
                   (unsigned)(Songs_Index() + 1U), (unsigned)g_song_num);
    show_line_center(0U, buf);

    show_line_center(1U, Songs_Name());          /* 曲名居中 */

    if (Songs_IsPlaying())
    {
        st = "PLAY";
    }
    else if (Songs_Pos() >= Songs_Len())
    {
        st = "DONE";                             /* 播完了：按 K2 从头再来 */
    }
    else if (Songs_Pos() > 0U)
    {
        st = "PAUSE";
    }
    else
    {
        st = "READY";
    }
    (void)snprintf(buf, sizeof(buf), "%-5s %2u/%2u",
                   st, (unsigned)Songs_Pos(), (unsigned)Songs_Len());
    show_line_center(2U, buf);

    show_line_center(3U, Songs_IsPlaying() ? "K2 PAUSE K3 NEXT" : "K2 PLAY  K3 NEXT");
}

/* 信息页：前三行是学生字库里的中文（生成时用 | 分隔），第 4 行显示"日期 + 时间" */
static void draw_info(void)
{
    uint8_t h, m, s;
    char buf[20];

    /* 四行全部水平居中 */
    BSP_OLED_ShowUserLineCenter(0U, (int16_t)(1U * line_h));   /* 例如 哲学本263   */
    BSP_OLED_ShowUserLineCenter(1U, (int16_t)(2U * line_h));   /* 例如 陈桂林      */

    BSP_OLED_ShowUserLineCenter(2U, (int16_t)(3U * line_h));   /* 例如 物联网协会 */

    /* 第 4 行：日期 + 时间，格式 YYMM-DD HH:MM:SS（例 2610-06 20:34:56）
       正好 16 个字符 = 128px 占满一行；日期用"年月-日"省掉一个分隔符，秒才留得下。
       板子无纽扣电池：上电时若 RTC 无效，会按固件编译时刻校一次 */
    BSP_RTC_Get(&h, &m, &s);
    (void)snprintf(buf, sizeof(buf), "%02d%02d-%02d %02d:%02d:%02d",
                   t_year, t_month, t_day, h, m, s);
    show_line_center(3U, buf);
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
        OLED_Clear(&g_oled);          /* 清屏再重画：数字/曲名长度变化时不留旧像素（2026-10-06 实机残影） */
        switch (screen)
        {
            case SCR_CLOCK:  draw_clock();  break;
            case SCR_STATUS: draw_status(); break;
            case SCR_LAMP:   draw_lamp();   break;
            case SCR_MUSIC:  draw_music();  break;
            case SCR_INFO:   draw_info();   break;
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

    /* ---------- 进设置那一次是按着 K3 完成的：松手前吞掉 K3 的事件 ---------- */
    if (k3_swallow)
    {
        if (BSP_KEY_IsPressed(BSP_KEY_3) == 0U)
        {
            k3_swallow = 0U;
        }
        else
        {
            e3 = BSP_KEY_EVENT_NONE;
        }
    }

    /* ---------- K1：单击永远切页（时间设置里＝保存并退出）；长按＝切模式 ---------- */
    switch (e1)
    {
    case BSP_KEY_EVENT_CLICK:
        if (time_set)
        {
            time_set = 0U;                      /* K1 一动就退出设置 */
            BSP_UART_Printf("[time] exit set (K1 click)\r\n");
        }
        if ((screen == SCR_MUSIC) && (Songs_IsPlaying() != 0U))
        {
            Songs_Pause();                      /* 离开音乐页：歌暂停（位置保留，回来按 K2 继续） */
        }
        screen = (ScreenId)((screen + 1U) % (uint8_t)SCR_NUM);
        if (oled_ok) OLED_Clear(&g_oled);
        BSP_UART_Printf("[ui  ] screen -> %d\r\n", (int)screen);
        break;
    case BSP_KEY_EVENT_LONG:
        if (time_set)
        {
            time_set = 0U;                      /* K1 一动就退出设置 */
            BSP_UART_Printf("[time] exit set (K1 hold)\r\n");
        }
        else if (screen == SCR_STATUS)          /* 只有状态页能切模式：音乐页/其它页不响提示音，避免抢蜂鸣器 */
        {
            mode = (WorkMode)((mode + 1U) % (uint8_t)MODE_NUM);
            BSP_UART_Printf("[mode] -> %s\r\n", mode_name());
            mode_beep_start();                  /* 常态 1 声 / 光控 2 声 / 热敏 3 声 */
        }
        else
        {
            BSP_UART_Printf("[ui  ] mode switch only on STATUS page\r\n");
        }
        break;
    default:                                    /* 双击暂时不用 */
        break;
    }

    /* ---------- 时间设置：K2 单击＝换单位；K2/K3 长按＝±1（连调） ---------- */
    if (time_set)
    {
        if (e2 == BSP_KEY_EVENT_CLICK)
        {
            time_unit = (uint8_t)((time_unit + 1U) % TU_NUM);
            BSP_UART_Printf("[time] unit -> %s\r\n", unit_name());
        }
        if ((e2 == BSP_KEY_EVENT_LONG) || (e2 == BSP_KEY_EVENT_LONG_REPEAT)) time_step(-1);
        if ((e3 == BSP_KEY_EVENT_LONG) || (e3 == BSP_KEY_EVENT_LONG_REPEAT)) time_step(+1);
        return;
    }
    /* ---------- 参数/状态页：K2 单击＝切参数；K2 长按＝+1、K3 长按＝−1；K3 单击＝恢复默认 ---------- */
    if (screen == SCR_STATUS)
    {
        if (e2 == BSP_KEY_EVENT_CLICK)
        {
            thr_sel = (uint8_t)((thr_sel + 1U) % 3U);
            BSP_UART_Printf("[thr ] select %u (0=THR 1=MID 2=HIGH)\r\n", thr_sel);
        }
        if ((e2 == BSP_KEY_EVENT_LONG) || (e2 == BSP_KEY_EVENT_LONG_REPEAT)) thr_step(+1);
        if ((e3 == BSP_KEY_EVENT_LONG) || (e3 == BSP_KEY_EVENT_LONG_REPEAT)) thr_step(-1);
        if (e3 == BSP_KEY_EVENT_CLICK) thr_reset();
        return;
    }

    /* ---------- 图片页：短按换动图，长按调速度 ---------- */
    if (screen == SCR_IMAGE)
    {
        if (e2 == BSP_KEY_EVENT_CLICK)                 /* 上一张动图 */
        {
            anim_idx = (uint8_t)((anim_idx + OLED_ANIM_COUNT - 1U) % OLED_ANIM_COUNT);
            gif_frame = 0U;
            gif_delay_ms = OLED_ANIMS[anim_idx].delay_ms;
            BSP_UART_Printf("[gif ] anim %d/%d  %dx%d  %d frames\r\n",
                            anim_idx + 1, OLED_ANIM_COUNT,
                            OLED_ANIMS[anim_idx].w, OLED_ANIMS[anim_idx].h,
                            OLED_ANIMS[anim_idx].count);
        }
        if (e3 == BSP_KEY_EVENT_CLICK)                 /* 下一张动图 */
        {
            anim_idx = (uint8_t)((anim_idx + 1U) % OLED_ANIM_COUNT);
            gif_frame = 0U;
            gif_delay_ms = OLED_ANIMS[anim_idx].delay_ms;
            BSP_UART_Printf("[gif ] anim %d/%d  %dx%d  %d frames\r\n",
                            anim_idx + 1, OLED_ANIM_COUNT,
                            OLED_ANIMS[anim_idx].w, OLED_ANIMS[anim_idx].h,
                            OLED_ANIMS[anim_idx].count);
        }
        if (e2 == BSP_KEY_EVENT_LONG)                  /* 慢 */
        {
            gif_delay_ms = (uint16_t)((gif_delay_ms < 400U) ? (gif_delay_ms + 20U) : 400U);
            BSP_UART_Printf("[gif ] delay %u ms\r\n", gif_delay_ms);
        }
        if (e3 == BSP_KEY_EVENT_LONG)                  /* 快 */
        {
            gif_delay_ms = (uint16_t)((gif_delay_ms > 30U) ? (gif_delay_ms - 20U) : 30U);
            BSP_UART_Printf("[gif ] delay %u ms\r\n", gif_delay_ms);
        }
        return;
    }

    /* ---------- 时间页：K3 长按＝进时间设置（任何模式下都能进）；其它键不动作 ---------- */
    if (screen == SCR_CLOCK)
    {
        if (e3 == BSP_KEY_EVENT_LONG)
        {
            time_set   = 1U;
            time_unit  = TU_HOUR;
            k3_swallow = 1U;             /* 松手前吞掉 K3 的后续事件 */
            rtc_load();
            if (oled_ok) OLED_Clear(&g_oled);
            BSP_UART_Printf("[time] enter set (K3 hold)\r\n");
        }
        return;
    }

    /* ---------- 灯控页：三颗灯独立开关 ----------
       K2 单击＝选下一颗、K2 双击＝呼吸⇄常亮、K2 长按＝全亮⇄全灭；
       K3 单击＝开/关选中的那颗、K3 长按＝流水灯开/关。
       进本页按任何键＝自动回到常态（灯由手动控制）。 */
    if (screen == SCR_LAMP)
    {
        /* 如果当前在光控/热控，先自动切回常态，免得按键"没反应" */
        if (mode != MODE_NORMAL)
        {
            BSP_UART_Printf("[lamp] auto back to NORMAL (was %s)\r\n", mode_name());
            mode = MODE_NORMAL;
        }

        if (e2 == BSP_KEY_EVENT_CLICK)                 /* 选下一颗（只动选中） */
        {
            lamp_sel = (uint8_t)((lamp_sel + 1U) % LED_NUM);
            BSP_UART_Printf("[lamp] sel -> LED%d\r\n", lamp_sel + 1);
        }
        if (e2 == BSP_KEY_EVENT_DOUBLE)                /* 呼吸 ⇄ 常亮 */
        {
            lamp_breath = (uint8_t)(!lamp_breath);
            BSP_UART_Printf("[lamp] breath %s\r\n", lamp_breath ? "ON" : "OFF");
        }
        if (e2 == BSP_KEY_EVENT_LONG)                  /* 全亮 ⇄ 全灭 */
        {
            lamp_flow = 0U;
            lamp_mask = (lamp_mask == 0x07U) ? 0x00U : 0x07U;
            BSP_UART_Printf("[lamp] ALL %s\r\n", (lamp_mask == 0x07U) ? "ON" : "OFF");
        }
        if (e3 == BSP_KEY_EVENT_CLICK)                 /* 开/关选中的那颗 */
        {
            lamp_flow = 0U;
            lamp_mask ^= (uint8_t)(1U << lamp_sel);
            BSP_UART_Printf("[lamp] LED%d %s  mask=%d\r\n", lamp_sel + 1,
                            (lamp_mask & (1U << lamp_sel)) ? "ON" : "OFF", lamp_mask);
        }
        if (e3 == BSP_KEY_EVENT_LONG)                  /* 流水灯开/关 */
        {
            lamp_flow = (uint8_t)(!lamp_flow);
            BSP_UART_Printf("[lamp] flow %s\r\n", lamp_flow ? "ON" : "OFF");
        }
        return;
    }

    /* ---------- 音乐页：K2 单击＝播放/暂停、K3 单击＝切歌 ----------
       暂停后按 K2 从当前音符继续；播完/待播状态按 K2 从头播。离开这一页时（K1）自动暂停。 */
    if (screen == SCR_MUSIC)
    {
        if (e2 == BSP_KEY_EVENT_CLICK)
        {
            if (Songs_IsPlaying() != 0U) Songs_Pause();
            else                         Songs_Resume();
        }
        if (e3 == BSP_KEY_EVENT_CLICK)
        {
            Songs_Next();
        }
        return;
    }

    /* ---------- 信息页：纯展示，无 K2/K3 动作 ---------- */
}

/* ------------------------------ 入口 ------------------------------ */
void APP_Demo_Init(void)
{
    int ret;

    BSP_UART_Init();
    BSP_UART_Printf("\r\n=== WULIAN BOARD DEMO (FINAL) ===\r\n");

    BSP_LED_Init();
    BSP_KEY_Init();
    BSP_BEEP_Init();
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
    for (uint8_t i = 0U; i < OLED_ANIM_COUNT; i++)
    {
        BSP_UART_Printf("[gif ] anim%d: %dx%d, %d frames, %u ms, %d bytes\r\n",
                        i, OLED_ANIMS[i].w, OLED_ANIMS[i].h, OLED_ANIMS[i].count,
                        OLED_ANIMS[i].delay_ms,
                        ((OLED_ANIMS[i].w + 7) / 8) * OLED_ANIMS[i].h * OLED_ANIMS[i].count);
    }
    BSP_UART_Printf("[tips] 6 pages INFO>STATUS>CLOCK>LAMP>IMAGE>MUSIC; K1 click=next, K1 hold on STATUS=mode(1/2/3 beeps)\r\n");
    BSP_UART_Printf("[tips] STATUS: K2 sel/K2H+/K3H-/K3 reset | CLOCK: K3H=set | LAMP: K2 sel+ALL/K3 breath+flow | MUSIC: K2 play-pause/K3 next (%u songs)\r\n", (unsigned)g_song_num);

    lamp_sel = 0U; lamp_mask = 0U; lamp_breath = 0U; lamp_flow = 0U;
    screen = SCR_INFO; mode = MODE_NORMAL; time_set = 0U; k3_swallow = 0U; thr_sel = 0U;

    boot_ms = HAL_GetTick();
    t_adc = t_draw = t_beat = boot_ms;
    t_gif = boot_ms;

    BSP_BEEP_Beep(80U);        /* 上电"滴"一声：听到就说明蜂鸣器这一路通了 */
    gif_frame = 0U;
    anim_idx = 0U;
    gif_delay_ms = OLED_ANIMS[0].delay_ms;

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

    BSP_BEEP_Task();           /* 提示音到点自动停（不阻塞） */
    mode_beep_task();          /* 切模式的 1/2/3 声提示音 */
    Songs_Task();              /* 音乐页曲库：上一个音放完推下一个（离开页时已暂停） */
    handle_keys();
    lamp_update();

    if (now - t_adc >= CFG_OLED_REFRESH_MS)
    {
        t_adc += CFG_OLED_REFRESH_MS;
        BSP_ADC_Process();
    }

    if (oled_ok && (screen == SCR_IMAGE))
    {
        /* 动图：按 gif_delay_ms 逐帧推进（图片页专用节拍） */
        if (now - t_gif >= gif_delay_ms)
        {
            const OledAnim *a = &OLED_ANIMS[anim_idx % OLED_ANIM_COUNT];
            t_gif = now;
            gif_frame = (uint8_t)((gif_frame + 1U) % a->count);
            screen_draw();
        }
    }
    else if (oled_ok && (now - t_draw >= 200U))
    {
        t_draw += 200U;
        screen_draw();
    }

    if (now - t_beat >= 2000U)
    {
        t_beat += 2000U;
        BSP_RTC_SyncDateBkup();     /* 日期滚动同步进备份寄存器（复位后能恢复） */
        /* TN = 热控现在该亮几颗（0—3），RemoteT = 原始码；调阈值时看这两个数最直观 */
        BSP_UART_Printf("[tick] scr=%d mode=%s lgt=%d%% rawT=%d TN%d lamp=%d/%d/%d/%d ev=%d/%d/%d\r\n",
                        (int)screen, mode_name(),
                        BSP_ADC_LightPercent(), BSP_ADC_GetRaw(BSP_ADC_CH_TEMP), temp_band_count(),
                        lamp_mask, lamp_sel, lamp_breath, lamp_flow,
                        ev_click, ev_long, ev_dbl);
    }
}
