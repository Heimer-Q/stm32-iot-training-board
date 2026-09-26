/**
  ******************************************************************************
  * @file    app_selftest.c
  * @brief   00 板子自检：串口逐项报状态 + OLED 显示 + 按键验灯
  *
  * 用法（讲给学生听）：
  *   ① 板子不亮、屏不显、灯不对，先烧这个程序；
  *   ② 串口（DEBUG 排针，115200）会逐项打印结果，哪项 FAIL 就先修哪一项；
  *   ③ 屏上报的是同样的内容，测试时看不到屏就盯串口。
  *
  * 学员要改的两处：
  *   ① 把 K2 的功能改成"三个灯轮流亮一遍"；
  *   ② 把心跳打印的间隔（CFG_SELFTEST_HEARTBEAT_MS）改短或改长。
  ******************************************************************************
  */

#include "app_selftest.h"
#include "app_config.h"
#include "bsp_led.h"
#include "bsp_key.h"
#include "bsp_oled.h"
#include "bsp_uart.h"

/* 屏幕行号（16 像素一行，共 4 行） */
#define ST_LINE_TITLE   0
#define ST_LINE_OLED    1
#define ST_LINE_KEY     2
#define ST_LINE_USER    3

#define CFG_SELFTEST_HEARTBEAT_MS   2000U

static uint8_t  oled_ok;
static uint16_t line_h;
static uint32_t last_scan_tick;
static uint32_t last_refresh_tick;
static uint32_t last_heartbeat_tick;
static uint8_t  last_keys;          /* bit0..2 = KEY1..3 当前状态，用于只在变化时打印 */

static void show_line(uint8_t row, const char *text)
{
    /* 注意：OLED 光标的 Y 是"基线"而不是行的上边缘，所以第 n 行的基线 = (n+1) × 行高 */
    OLED_SetCursor(&g_oled, 0, (int16_t)((row + 1U) * line_h));
    OLED_DrawString(&g_oled, text);
}

static uint8_t read_keys(void)
{
    uint8_t v = 0U;
    if (BSP_KEY_IsPressed(BSP_KEY_1)) v |= 0x01U;
    if (BSP_KEY_IsPressed(BSP_KEY_2)) v |= 0x02U;
    if (BSP_KEY_IsPressed(BSP_KEY_3)) v |= 0x04U;
    return v;
}

/* OLED 起不来时用三个灯快闪报警（屏幕已经不亮了，只能靠灯） */
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

    BSP_UART_Init();
    BSP_UART_Printf("\r\n=== WULIAN TRAINING BOARD SELF TEST v0.3 ===\r\n");
    BSP_UART_Printf("[sys ] clock 72MHz, debug uart 115200-8-N-1\r\n");

    BSP_LED_Init();
    BSP_KEY_Init();
    BSP_LED_AllOff();
    BSP_UART_Printf("[led ] 3 LEDs init OK  (PA6/PA7/PB0, TIM3 PWM)\r\n");

    BSP_UART_Printf("[key ] 3 keys init OK  (KEY1=PB12 KEY2=PB8 KEY3=PB9, active low)\r\n");

    ret = BSP_OLED_Init();
    oled_ok = (ret == 0) ? 1U : 0U;
    if (oled_ok)
    {
        BSP_UART_Printf("[oled] init OK  (I2C1 PB6/PB7, addr 0x3C)\r\n");
    }
    else
    {
        /* -1 = I2C 无应答（模块没插 / 接线 / 地址）；-2 = malloc 失败（堆不够，检查 startup 的 Heap_Size） */
        BSP_UART_Printf("[oled] init FAIL ret=%d  (-1: I2C no ACK, -2: heap too small)\r\n", ret);
        oled_fail_blink();
    }

    if (oled_ok)
    {
        line_h = OLED_GetFontHeight(&g_oled);
        if (line_h == 0U)
        {
            line_h = 16U;
        }
        show_line(ST_LINE_TITLE, "SELFTEST  v0.3");
        show_line(ST_LINE_OLED,  "OLED : OK");
        show_line(ST_LINE_KEY,   "KEY  : 0 0 0");
        BSP_OLED_ShowUserText(0, (int16_t)((ST_LINE_USER + 1U) * line_h));
        BSP_OLED_Refresh();
    }

    BSP_UART_Printf("[tips] press K1=all on, K2=middle on, K3=all off\r\n");
    BSP_UART_Printf("=== self test done, heartbeat every %u ms ===\r\n", (unsigned)CFG_SELFTEST_HEARTBEAT_MS);

    last_scan_tick      = HAL_GetTick();
    last_refresh_tick   = last_scan_tick;
    last_heartbeat_tick = last_scan_tick;
    last_keys           = 0U;
}

void APP_SelfTest_Process(void)
{
    char    buf[24];
    uint32_t now = HAL_GetTick();
    uint8_t  keys;

    /* ---- 每 10ms 扫一次按键 ---- */
    if (now - last_scan_tick >= CFG_KEY_SCAN_MS)
    {
        last_scan_tick += CFG_KEY_SCAN_MS;
        BSP_KEY_Scan();
    }

    /* ---- 按键状态变化时：串口打印 + 点灯 ---- */
    keys = read_keys();
    if (keys != last_keys)
    {
        BSP_UART_Printf("[key ] state = %u%u%u  (K1 K2 K3)\r\n",
                        (unsigned)((keys >> 0) & 1U),
                        (unsigned)((keys >> 1) & 1U),
                        (unsigned)((keys >> 2) & 1U));
        last_keys = keys;
    }

    if (BSP_KEY_WasClicked(BSP_KEY_1))
    {
        for (uint8_t i = 0; i < LED_NUM; i++) BSP_LED_SetPercent(i, 100U);
        BSP_UART_Printf("[led ] K1 -> all on\r\n");
    }
    if (BSP_KEY_WasClicked(BSP_KEY_2))
    {
        BSP_LED_AllOff();
        BSP_LED_SetPercent(1, 100U);
        BSP_UART_Printf("[led ] K2 -> middle on\r\n");
    }
    if (BSP_KEY_WasClicked(BSP_KEY_3))
    {
        BSP_LED_AllOff();
        BSP_UART_Printf("[led ] K3 -> all off\r\n");
    }

    /* ---- 心跳：证明程序还在跑（也方便你判断是"死机"还是"没输出"） ---- */
    if (now - last_heartbeat_tick >= CFG_SELFTEST_HEARTBEAT_MS)
    {
        last_heartbeat_tick += CFG_SELFTEST_HEARTBEAT_MS;
        BSP_UART_Printf("[tick] %lu ms  keys=%u%u%u  oled=%s\r\n",
                        (unsigned long)now,
                        (unsigned)((keys >> 0) & 1U),
                        (unsigned)((keys >> 1) & 1U),
                        (unsigned)((keys >> 2) & 1U),
                        oled_ok ? "OK" : "FAIL");
    }

    /* ---- 屏幕：每 100ms 刷一次按键状态（屏没接就跳过） ---- */
    if (oled_ok && (now - last_refresh_tick >= CFG_OLED_REFRESH_MS))
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
}
