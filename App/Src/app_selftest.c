/**
  ******************************************************************************
  * @file    app_selftest.c
  * @brief   00 板子自检 + 按键控灯（两个画面）
  *
  * 按键功能（2026-09-26 会长定稿）
  *   K1 短按：切换画面        画面1 = 自检状态；画面2 = 时钟 + 光照 + 温度
  *   K1 长按：光控灯模式开关（亮着的灯由光照强度控制，越暗越亮）
  *   K2 短按：选灯——按一下换下一颗灯亮（单灯轮换）
  *   K2 长按：三颗灯全亮（常亮）
  *   K3 短按：当前亮着的灯 呼吸 ⇄ 常亮 切换
  *   K3 长按：关闭所有灯
  *
  * 串口（DEBUG 排针，115200）会打印每次按键动作和每秒心跳，调试靠它。
  ******************************************************************************
  */

#include "app_selftest.h"
#include "app_config.h"
#include "bsp_led.h"
#include "bsp_key.h"
#include "bsp_oled.h"
#include "bsp_uart.h"
#include "bsp_adc.h"
#include "bsp_rtc.h"

/* 屏幕：4 行（16 像素行高） */
#define ST_LINE_0   0
#define ST_LINE_1   1
#define ST_LINE_2   2
#define ST_LINE_3   3

#define HEARTBEAT_MS   2000U

typedef enum { SCR_STATUS = 0, SCR_CLOCK, SCR_NUM } ScreenId;

static uint8_t  oled_ok;
static uint16_t line_h;
static ScreenId screen = SCR_STATUS;

/* ---- 灯的状态 ---- */
static uint8_t lamp_all;        /* 1 = 三颗全亮 */
static uint8_t lamp_on;         /* 1 = 单灯模式下有一颗亮着 */
static uint8_t lamp_sel;        /* 单灯模式下选中的是第几颗（0..2） */
static uint8_t lamp_breath;     /* 1 = 呼吸 */
static uint8_t lamp_adc;        /* 1 = 光控（光照越暗越亮） */

static uint32_t last_adc_tick;
static uint32_t last_draw_tick;
static uint32_t last_beat_tick;
static uint8_t  last_keys;

/* ------------------------------------------------------------------ */
static void show_line(uint8_t row, const char *text)
{
    /* OLED 光标的 Y 是"基线"，所以第 n 行的基线 = (n+1) × 行高 */
    OLED_SetCursor(&g_oled, 0, (int16_t)((row + 1U) * line_h));
    OLED_DrawString(&g_oled, text);
}

static const char *temp_level_str(void)
{
    uint16_t raw = BSP_ADC_GetRaw(BSP_ADC_CH_TEMP);

    if (raw < CFG_TEMP_LOW_MAX)  return "LOW";
    if (raw < CFG_TEMP_MID_MAX)  return "MID";
    return "HIGH";
}

static const char *lamp_mode_str(void)
{
    if (lamp_adc)    return "ADC";
    if (lamp_breath) return "BREATH";
    return "ON";
}

static uint8_t read_keys(void)
{
    uint8_t v = 0U;
    if (BSP_KEY_IsPressed(BSP_KEY_1)) v |= 0x01U;
    if (BSP_KEY_IsPressed(BSP_KEY_2)) v |= 0x02U;
    if (BSP_KEY_IsPressed(BSP_KEY_3)) v |= 0x04U;
    return v;
}

/* 把"哪些灯亮、多亮"算出来写进 PWM */
static void lamp_apply(void)
{
    uint8_t lit[LED_NUM];
    uint32_t duty = 100U;      /* 基础亮度：100 = 常亮；光控时由光照决定 */

    for (uint8_t i = 0U; i < LED_NUM; i++)
    {
        lit[i] = 0U;
    }
    if (lamp_all)
    {
        for (uint8_t i = 0U; i < LED_NUM; i++) lit[i] = 1U;
    }
    else if (lamp_on)
    {
        lit[lamp_sel % LED_NUM] = 1U;
    }

    if (lamp_adc)
    {
        uint32_t p = BSP_ADC_LightPercent();

        if (p <= CFG_LAMP_ADC_DARK_MIN)
        {
            duty = 100U;
        }
        else if (p >= CFG_LAMP_ADC_BRIGHT_MAX)
        {
            duty = 0U;
        }
        else
        {
            duty = 100U - (p - CFG_LAMP_ADC_DARK_MIN) * 100U /
                           (CFG_LAMP_ADC_BRIGHT_MAX - CFG_LAMP_ADC_DARK_MIN);
        }
    }

    if (lamp_breath)
    {
        uint32_t t    = HAL_GetTick() % CFG_BREATH_PERIOD_MS;
        uint32_t half = CFG_BREATH_PERIOD_MS / 2U;
        uint32_t b    = (t < half) ? (t * 100U / half)
                                   : ((CFG_BREATH_PERIOD_MS - t) * 100U / half);
        duty = duty * b / 100U;
    }

    for (uint8_t i = 0U; i < LED_NUM; i++)
    {
        BSP_LED_SetPercent((uint8_t)i, lit[i] ? (uint8_t)duty : 0U);
    }
}

/* 画当前画面（整屏重画：I2C 提到 400kHz 后一整屏约 23ms，够用） */
static void screen_draw(void)
{
    uint8_t h = 0U, m = 0U, s = 0U;

    if (!oled_ok)
    {
        return;
    }

    if (screen == SCR_STATUS)
    {
        show_line(ST_LINE_0, "SELFTEST  v0.4");
        show_line(ST_LINE_1, "OLED : OK");
        OLED_SetCursor(&g_oled, 0, (int16_t)(2U * line_h));
        OLED_Printf(&g_oled, "KEY  : %d %d %d",
                    BSP_KEY_IsPressed(BSP_KEY_1) ? 1 : 0,
                    BSP_KEY_IsPressed(BSP_KEY_2) ? 1 : 0,
                    BSP_KEY_IsPressed(BSP_KEY_3) ? 1 : 0);
        BSP_OLED_ShowUserText(0, (int16_t)(4U * line_h));
    }
    else
    {
        BSP_RTC_Get(&h, &m, &s);
        OLED_SetCursor(&g_oled, 0, (int16_t)(1U * line_h));
        OLED_Printf(&g_oled, "TIME %02d:%02d:%02d", h, m, s);

        OLED_SetCursor(&g_oled, 0, (int16_t)(2U * line_h));
        OLED_Printf(&g_oled, "LIGHT %3d%%", BSP_ADC_LightPercent());

        OLED_SetCursor(&g_oled, 0, (int16_t)(3U * line_h));
        OLED_Printf(&g_oled, "TEMP %-4s %4d", temp_level_str(), BSP_ADC_GetRaw(BSP_ADC_CH_TEMP));

        OLED_SetCursor(&g_oled, 0, (int16_t)(4U * line_h));
        if (lamp_all)
        {
            OLED_Printf(&g_oled, "LAMP ALL %s", lamp_mode_str());
        }
        else if (lamp_on)
        {
            OLED_Printf(&g_oled, "LAMP %d %s", lamp_sel + 1, lamp_mode_str());
        }
        else
        {
            OLED_DrawString(&g_oled, "LAMP OFF");
        }
    }

    BSP_OLED_Refresh();
}

/* OLED 起不来时用三个灯快闪报警 */
static void oled_fail_blink(void)
{
    for (uint8_t n = 0U; n < 3U; n++)
    {
        for (uint8_t i = 0U; i < LED_NUM; i++)
        {
            BSP_LED_AllOff();
            BSP_LED_SetPercent(i, 100U);
            HAL_Delay(120);
        }
    }
    BSP_LED_AllOff();
}

void APP_SelfTest_Init(void)
{
    int ret;

    BSP_UART_Init();
    BSP_UART_Printf("\r\n=== WULIAN TRAINING BOARD DEMO v0.4 ===\r\n");

    BSP_LED_Init();
    BSP_KEY_Init();
    BSP_ADC_Init();
    BSP_RTC_Init();
    BSP_LED_AllOff();

    BSP_UART_Printf("[led ] 3 LEDs OK (PA6/PA7/PB0)\r\n");
    BSP_UART_Printf("[key ] 3 keys OK (PB12/PB8/PB9), long press = %u ms\r\n",
                    (unsigned)(BSP_KEY_LONG_TICKS * BSP_KEY_TICK_MS));
    BSP_UART_Printf("[rtc ] %s\r\n", BSP_RTC_Valid() ? "valid (LSE 32768Hz)" : "NOT valid - LSE fail");

    ret = BSP_OLED_Init();
    oled_ok = (ret == 0) ? 1U : 0U;
    BSP_UART_Printf("[oled] init %s (ret=%d)\r\n", oled_ok ? "OK" : "FAIL", ret);
    if (!oled_ok)
    {
        oled_fail_blink();
    }

    lamp_all = 0U; lamp_on = 0U; lamp_sel = 0U; lamp_breath = 0U; lamp_adc = 0U;
    lamp_apply();

    if (oled_ok)
    {
        line_h = OLED_GetFontHeight(&g_oled);
        if (line_h == 0U) line_h = 16U;
        OLED_Clear(&g_oled);
        screen_draw();
    }

    BSP_UART_Printf("[tips] K1=screen/adc-mode  K2=next lamp/all  K3=breath/off\r\n");

    last_adc_tick  = HAL_GetTick();
    last_draw_tick = last_adc_tick;
    last_beat_tick = last_adc_tick;
    last_keys      = 0U;
}

void APP_SelfTest_Process(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t  keys;
    BSP_KEY_Event ev;

    /* ---- 按键状态变化（给调试看） ---- */
    keys = read_keys();
    if (keys != last_keys)
    {
        BSP_UART_Printf("[key ] %u%u%u\r\n",
                        (unsigned)((keys >> 0) & 1U),
                        (unsigned)((keys >> 1) & 1U),
                        (unsigned)((keys >> 2) & 1U));
        last_keys = keys;
    }

    /* ---- 按键事件（状态机在 SysTick 中断里跑，这里只取事件；取走即清） ---- */
    ev = BSP_KEY_GetEvent(BSP_KEY_1);
    if (ev == BSP_KEY_EVENT_LONG_REPEAT)      /* 长按持续：以后调时间会用 */
    {
        BSP_UART_Printf("[key ] K1 long-repeat\r\n");
    }
    switch (ev)
    {
    case BSP_KEY_EVENT_CLICK:                 /* 短按：切换画面 */
        screen = (ScreenId)((screen + 1U) % (uint8_t)SCR_NUM);
        if (oled_ok)
        {
            OLED_Clear(&g_oled);              /* 换画面先清屏，避免残留 */
        }
        BSP_UART_Printf("[ui  ] screen -> %d\r\n", (int)screen);
        break;
    case BSP_KEY_EVENT_DOUBLE:                /* 双击：直接回画面 1 */
        screen = SCR_STATUS;
        if (oled_ok)
        {
            OLED_Clear(&g_oled);
        }
        BSP_UART_Printf("[ui  ] double -> screen 0\r\n");
        break;
    case BSP_KEY_EVENT_LONG:                  /* 长按：光控模式开关 */
        lamp_adc = (uint8_t)(!lamp_adc);
        if (lamp_adc && !lamp_all && !lamp_on)
        {
            lamp_on  = 1U;                    /* 光控模式下总得有一盏灯给你控 */
            lamp_sel = 0U;
        }
        BSP_UART_Printf("[lamp] ADC mode %s\r\n", lamp_adc ? "ON" : "OFF");
        break;
    default:
        break;
    }

    ev = BSP_KEY_GetEvent(BSP_KEY_2);
    if (ev == BSP_KEY_EVENT_LONG_REPEAT)
    {
        BSP_UART_Printf("[key ] K2 long-repeat\r\n");
    }
    switch (ev)
    {
    case BSP_KEY_EVENT_CLICK:                 /* 短按：选下一颗灯亮 */
        lamp_all = 0U;
        lamp_adc = 0U;
        if (lamp_on)
        {
            lamp_sel = (uint8_t)((lamp_sel + 1U) % LED_NUM);
        }
        else
        {
            lamp_sel = 0U;
            lamp_on  = 1U;
        }
        BSP_UART_Printf("[lamp] next -> LED%d\r\n", lamp_sel + 1);
        break;
    case BSP_KEY_EVENT_LONG:                  /* 长按：三灯全亮 */
        lamp_all    = 1U;
        lamp_on     = 0U;
        lamp_breath = 0U;
        lamp_adc    = 0U;
        BSP_UART_Printf("[lamp] ALL ON\r\n");
        break;
    default:
        break;
    }

    ev = BSP_KEY_GetEvent(BSP_KEY_3);
    if (ev == BSP_KEY_EVENT_LONG_REPEAT)
    {
        BSP_UART_Printf("[key ] K3 long-repeat\r\n");
    }
    switch (ev)
    {
    case BSP_KEY_EVENT_CLICK:                 /* 短按：呼吸 ⇄ 常亮 */
        lamp_breath = (uint8_t)(!lamp_breath);
        BSP_UART_Printf("[lamp] breath %s\r\n", lamp_breath ? "ON" : "OFF");
        break;
    case BSP_KEY_EVENT_LONG:                  /* 长按：关闭所有灯 */
        lamp_all    = 0U;
        lamp_on     = 0U;
        lamp_breath = 0U;
        lamp_adc    = 0U;
        BSP_UART_Printf("[lamp] ALL OFF\r\n");
        break;
    default:
        break;
    }

    /* ---- 灯：每 10ms 重算一次（呼吸要平滑） ---- */
    lamp_apply();

    /* ---- 100ms：采样光敏/热敏 ---- */
    if (now - last_adc_tick >= CFG_OLED_REFRESH_MS)
    {
        last_adc_tick += CFG_OLED_REFRESH_MS;
        BSP_ADC_Process();
    }

    /* ---- 200ms：刷屏（画面2 时钟要跟着走） ---- */
    if (oled_ok && (now - last_draw_tick >= 200U))
    {
        last_draw_tick += 200U;
        screen_draw();
    }

    /* ---- 2s：串口心跳（含原始码，方便标定） ---- */
    if (now - last_beat_tick >= HEARTBEAT_MS)
    {
        last_beat_tick += HEARTBEAT_MS;
        BSP_UART_Printf("[tick] scr=%d key=%u%u%u lgt=%u%% rawL=%u rawT=%u lamp=%s%s%s\r\n",
                        (int)screen,
                        (unsigned)((keys >> 0) & 1U), (unsigned)((keys >> 1) & 1U), (unsigned)((keys >> 2) & 1U),
                        (unsigned)BSP_ADC_LightPercent(),
                        (unsigned)BSP_ADC_GetRaw(BSP_ADC_CH_LIGHT),
                        (unsigned)BSP_ADC_GetRaw(BSP_ADC_CH_TEMP),
                        lamp_all ? "ALL" : (lamp_on ? "ONE" : "OFF"),
                        lamp_breath ? "+BREATH" : "",
                        lamp_adc ? "+ADC" : "");
    }
}
