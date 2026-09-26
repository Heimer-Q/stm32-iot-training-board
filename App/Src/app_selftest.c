/**
  ******************************************************************************
  * @file    app_selftest.c
  * @brief   00 板子自检：OLED 逐项报状态，按键当场验灯
  *
  * 用法（讲给学生听）：
  *   ① 板子不亮、屏不显、灯不对，先烧这个程序；
  *   ② 屏上报哪一项不对，就先修那一处（虚焊 / 插反 / 线接错）；
  *   ③ 三颗按键按下时屏上对应数字变 1，同时对应的灯亮。
  *
  * 学员要改的两处：
  *   ① 把 K2 的功能改成"三个灯轮流亮一遍"（现在 K2 是中间的灯亮）；
  *   ② 把自检顺序换成你自己喜欢的顺序（改下面的行号常量）。
  ******************************************************************************
  */

#include "app_selftest.h"
#include "app_config.h"
#include "bsp_led.h"
#include "bsp_key.h"
#include "bsp_oled.h"

/* 屏幕上一行一个字符串，行距 = 字体高度（默认字库 5x8，一行能放 21 个字符） */
#define ST_LINE_TITLE   0
#define ST_LINE_OLED    1
#define ST_LINE_KEY     2
#define ST_LINE_USER    3          /* 学生自己生成的中文班级/姓名 */

static uint8_t  oled_ok;
static uint16_t line_h;
static uint32_t last_scan_tick;
static uint32_t last_refresh_tick;

static void show_line(uint8_t row, const char *text)
{
    /* 注意：OLED 光标的 Y 是"基线"而不是行的上边缘，所以第 n 行的基线 = (n+1) × 行高 */
    OLED_SetCursor(&g_oled, 0, (int16_t)((row + 1U) * line_h));
    OLED_DrawString(&g_oled, text);
}

/* OLED 起不来时的兜底：用灯报错（红→绿→黄 快闪三次），屏幕救不回来但能让人知道 */
static void oled_fail_blink(void)
{
    for (uint8_t n = 0; n < 3U; n++)
    {
        for (uint8_t i = 0; i < LED_NUM; i++)
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

    BSP_LED_Init();
    BSP_KEY_Init();
    BSP_LED_AllOff();

    ret = BSP_OLED_Init();
    oled_ok = (ret == 0) ? 1U : 0U;
    if (!oled_ok)
    {
        oled_fail_blink();
        return;
    }

    line_h = OLED_GetFontHeight(&g_oled);
    if (line_h == 0U) line_h = 8U;

    show_line(ST_LINE_TITLE, "SELFTEST  v0.2");
    show_line(ST_LINE_OLED,  "OLED : OK");
    show_line(ST_LINE_KEY,   "KEY  : 0 0 0");
    /* 第四行：学生自己生成的班级/姓名（字模来自 Tools/make_font.py） */
    BSP_OLED_ShowUserText(0, (int16_t)((ST_LINE_USER + 1U) * line_h));
    BSP_OLED_Refresh();

    last_scan_tick = HAL_GetTick();
    last_refresh_tick = last_scan_tick;
}

void APP_SelfTest_Process(void)
{
    char buf[24];

    if (!oled_ok)
    {
        return;                     /* 屏都不亮，这里没什么可做的 */
    }

    /* ---- 每 10ms 扫一次按键 ---- */
    if (HAL_GetTick() - last_scan_tick >= CFG_KEY_SCAN_MS)
    {
        last_scan_tick += CFG_KEY_SCAN_MS;
        BSP_KEY_Scan();
    }

    /* ---- 每 100ms 刷一次屏（只在变化时才重画） ---- */
    if (HAL_GetTick() - last_refresh_tick >= CFG_OLED_REFRESH_MS)
    {
        last_refresh_tick += CFG_OLED_REFRESH_MS;

        buf[0] = 'K'; buf[1] = 'E'; buf[2] = 'Y'; buf[3] = ' '; buf[4] = ' '; buf[5] = ':'; buf[6] = ' ';
        buf[7]  = (char)('0' + (BSP_KEY_IsPressed(BSP_KEY_1) ? 1 : 0));
        buf[8]  = ' ';
        buf[9]  = (char)('0' + (BSP_KEY_IsPressed(BSP_KEY_2) ? 1 : 0));
        buf[10] = ' ';
        buf[11] = (char)('0' + (BSP_KEY_IsPressed(BSP_KEY_3) ? 1 : 0));
        buf[12] = '\0';
        show_line(ST_LINE_KEY, buf);
        BSP_OLED_Refresh();
    }

    /* ---- 按键动作：K1 三个灯全亮，K2 只亮中间，K3 全灭 ---- */
    if (BSP_KEY_WasClicked(BSP_KEY_1))
    {
        for (uint8_t i = 0; i < LED_NUM; i++) BSP_LED_SetPercent(i, 100U);
    }
    if (BSP_KEY_WasClicked(BSP_KEY_2))
    {
        BSP_LED_AllOff();
        BSP_LED_SetPercent(1, 100U);
    }
    if (BSP_KEY_WasClicked(BSP_KEY_3))
    {
        BSP_LED_AllOff();
    }
}
