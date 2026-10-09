/**
  ******************************************************************************
  * @file    app_hwcheck.c
  * @brief   最简硬件自检（DEMO_HWCHECK = 3）：上电自动跑，**不需要按任何键**
  *
  * == 它想回答一个问题：这块板子焊好了吗？ ==
  *
  *   ① 三颗 LED   每 500ms 轮流亮一颗（肉眼看灯）＋ 按住任意键时，对应那颗灯亮
  *   ② OLED 屏幕  4 行：标题 / 当前灯号 / 三个键的状态 / 光照 + 温度
  *   ③ 按键       直接读"现在按着没"（用驱动里已消抖的 BSP_KEY_IsPressed，
  *                不玩单击/双击/长按——自检要的是简单可信）
  *   ④ 光敏/热敏  每 500ms 采一次，原始码和百分比都打出来
  *   ⑤ RTC        打印时间；秒在走就说明时钟源在跑（板子没电池，掉电会重设）
  *   ⑥ 蜂鸣器     上电"滴"一声；每次按键也滴一声
  *
  * 串口（DEBUG 排针，115200）每 500ms 打一行，形如：
  *   [hw] LED=2 RTC=19:20:31 LGT=65% T=48% RAW_T=1948 K=0/0/0 OLED=OK
  *
  * == 屏幕黑屏、但其它都正常？==
  *   那是 OLED 显存 malloc 失败（驱动一次要 128×8+1 = 1025 字节的堆）。
  *   本自检**不会因此停**：串口会打 `OLED FAIL(ret=-2)` 提示，
  *   灯 / 按键 / 蜂鸣器 / ADC / RTC 继续跑，方便一眼分清"屏坏了"还是"板子坏了"。
  ******************************************************************************
  */

#include "app_hwcheck.h"
#include "app_config.h"
#include "bsp_led.h"
#include "bsp_key.h"
#include "bsp_oled.h"
#include "bsp_uart.h"
#include "bsp_adc.h"
#include "bsp_rtc.h"
#include "bsp_beep.h"

#include <stdio.h>

#define HW_PERIOD_MS   500U     /* 采样 + 打印周期 */
#define HW_RAW_FULL    4095U    /* 12 位 ADC 满量程 */

static uint8_t  oled_ok;
static uint16_t line_h = 16U;
static uint32_t last_tick;
static uint32_t t_adc;          /* ADC 处理节拍（100ms 一次，让 8 点滑动平均早点填满） */
static uint8_t  led_step;       /* 轮到第几颗灯（0/1/2 循环） */
static uint8_t  key_last;       /* 上一轮的按键状态，用来只打印"变化" */

/* 屏幕第 row 行（row 从 0 开始）——OLED 光标给的是基线，所以是 (row+1)×行高 */
static void show_line(uint8_t row, const char *text)
{
    OLED_SetCursor(&g_oled, 0, (int16_t)((row + 1U) * line_h));
    OLED_DrawString(&g_oled, text);
}

/* ADC 原始码 → 0~100% */
static uint8_t raw_to_pct(uint16_t raw)
{
    return (uint8_t)((uint32_t)raw * 100U / HW_RAW_FULL);
}

void APP_HWCheck_Init(void)
{
    uint8_t h, m, s;
    int     ret;

    BSP_UART_Init();
    BSP_UART_Printf("\r\n=== TRAINING BOARD HWCHECK ===\r\n");

    BSP_LED_Init();
    BSP_LED_AllOff();                       /* 先全灭：上电时灯乱亮会看不清现象 */
    BSP_KEY_Init();
    BSP_BEEP_Init();
    BSP_ADC_Init();
    BSP_RTC_Init();

    ret     = BSP_OLED_Init();
    oled_ok = (ret == 0) ? 1U : 0U;
    BSP_UART_Printf("[hw  ] OLED = %s (ret=%d)%s\r\n",
                    oled_ok ? "OK" : "FAIL", ret,
                    oled_ok ? "" : "  -> heap < 1025 B? check Training.ioc HeapSize");
    if (oled_ok)
    {
        line_h = OLED_GetFontHeight(&g_oled);
        if (line_h == 0U) line_h = 16U;
        OLED_Clear(&g_oled);
    }

    /* 上电提示音：听到"滴"就说明蜂鸣器这一路通了 */
    BSP_BEEP_Beep(80U);

    /* 三颗灯各闪一次：确认 LED 与 PWM 通道 */
    for (uint8_t i = 0U; i < LED_NUM; i++)
    {
        BSP_LED_SetPercent(i, 100U);
        for (volatile uint32_t d = 0U; d < 400000U; d++) { }
        BSP_LED_SetPercent(i, 0U);
    }

    BSP_RTC_Get(&h, &m, &s);
    BSP_UART_Printf("[hw  ] RTC = %02d:%02d:%02d  (ticking = RTC ok; no battery, resets after power off)\r\n", h, m, s);
    BSP_UART_Printf("[hw  ] printing status every %u ms; press any key to hear a beep\r\n", (unsigned)HW_PERIOD_MS);

    last_tick = HAL_GetTick();
    t_adc     = last_tick;
    led_step  = 0U;
    key_last  = 0U;
}

void APP_HWCheck_Process(void)
{
    uint32_t now = HAL_GetTick();

    BSP_BEEP_Task();                    /* 提示音到点自动停（非阻塞） */

    /* ---- ADC：每 100ms 处理一次。驱动内部是 8 点滑动平均，调太慢要好几秒才收敛 ---- */
    if ((now - t_adc) >= 100U)
    {
        t_adc += 100U;
        BSP_ADC_Process();
    }

    /* ---- 按键：按着就亮对应那颗灯 + 滴一声 + 串口打印（只在状态变化时打） ---- */
    uint8_t k1 = BSP_KEY_IsPressed(BSP_KEY_1);
    uint8_t k2 = BSP_KEY_IsPressed(BSP_KEY_2);
    uint8_t k3 = BSP_KEY_IsPressed(BSP_KEY_3);
    uint8_t keys = (uint8_t)(k1 | (k2 << 1) | (k3 << 2));

    if (keys != key_last)
    {
        key_last = keys;
        if (keys != 0U)
        {
            BSP_BEEP_Beep(30U);
            BSP_UART_Printf("[key ] pressed: %s%s%s\r\n",
                            k1 ? "K1 " : "", k2 ? "K2 " : "", k3 ? "K3 " : "");
        }
        else
        {
            BSP_UART_Printf("[key ] all released\r\n");
        }
    }

    /* ---- 灯：每 500ms 换下一颗；按着键时，对应那颗灯保持亮 ---- */
    if ((now - last_tick) >= HW_PERIOD_MS)
    {
        uint8_t h, m, s;
        uint16_t raw_t;
        char     buf[24];

        last_tick += HW_PERIOD_MS;
        led_step   = (uint8_t)((led_step + 1U) % LED_NUM);

        BSP_RTC_Get(&h, &m, &s);
        raw_t = BSP_ADC_GetRaw(BSP_ADC_CH_TEMP);

        for (uint8_t i = 0U; i < LED_NUM; i++)
        {
            /* 默认：轮流亮一颗；如果按着 K1/K2/K3，则对应 LED 常亮 */
            uint8_t on = (i == led_step) || ((keys >> i) & 1U);
            BSP_LED_SetPercent(i, on ? 100U : 0U);
        }

        BSP_UART_Printf("[hw  ] LED=%u RTC=%02d:%02d:%02d LGT=%u%% T=%u%% RAW_T=%u K=%u/%u/%u OLED=%s\r\n",
                        (unsigned)(led_step + 1U), h, m, s,
                        (unsigned)BSP_ADC_LightPercent(), (unsigned)raw_to_pct(raw_t), (unsigned)raw_t,
                        k1, k2, k3, oled_ok ? "OK" : "FAIL");

        if (oled_ok)
        {
            OLED_Clear(&g_oled);
            show_line(0U, "== HW CHECK ==");
            (void)snprintf(buf, sizeof(buf), "LED %u/3  %s", (unsigned)(led_step + 1U), "RUN");
            show_line(1U, buf);
            (void)snprintf(buf, sizeof(buf), "KEY %u%u%u", k1, k2, k3);
            show_line(2U, buf);
            (void)snprintf(buf, sizeof(buf), "L%3u%% T%3u%%", (unsigned)BSP_ADC_LightPercent(), (unsigned)raw_to_pct(raw_t));
            show_line(3U, buf);
            BSP_OLED_Refresh();
        }
    }
}
