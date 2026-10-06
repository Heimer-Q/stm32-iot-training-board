/**
  ******************************************************************************
  * @file    app_beep.c
  * @brief   06 蜂鸣器例程：按键发单音 + 长按播整首曲子（曲库在 app_songs.c）
  *
  * == 新人导读 ================================================================
  * 1. 按键功能（板上 K1/K2/K3）：
  *      单击 = 发三个不同音高的单音（do / mi / sol，听音高）
  *      K1 长按 = 从当前曲子的开头播放
  *      K2 长按 = 切到下一首；K2 双击 = 发一个更高的音
  *      K3 长按 = 停止
  *
  * 2. 一个重要的编程规则（务必记住）：
  *    按键事件是"取走即清"的——同一个键在一个循环里【只能取一次事件】，
  *    然后用 if/else 分流。如果先取一次判长按、再取一次判单击，
  *    第二次取到的一定是"空"，单击就永远不触发——这个坑真踩过，
  *    见《修改记录》2026-10-06 那条"单击事件被吃"。
  ******************************************************************************
  */

#include "app_beep.h"
#include "app_config.h"
#include "app_songs.h"
#include "bsp_beep.h"
#include "bsp_key.h"
#include "bsp_uart.h"

/* 初始化：开蜂鸣器、开按键、打个启动横幅，最后"滴"一声自检 */
void APP_Beep_Init(void)
{
    BSP_BEEP_Init();
    BSP_KEY_Init();
    BSP_UART_Init();

    BSP_UART_Printf("\r\n=== 06 BEEP DEMO ===\r\n");
    BSP_UART_Printf("[song] %u songs in library, first = %s\r\n",
                    (unsigned)g_song_num, g_songs[0].name);
    BSP_UART_Printf("[tips] K1 hold=play  K2 hold=next  K3 hold=stop\r\n");

    BSP_BEEP_Beep(120U);        /* 上电"滴"一声：能响就说明蜂鸣器这一路是通的 */
}

/* 主循环任务：喂两个"到点收尾"的函数 + 处理按键 */
void APP_Beep_Process(void)
{
    BSP_KEY_Event ev;

    BSP_BEEP_Task();            /* 到点自动停（不阻塞主循环） */
    Songs_Task();               /* 曲库播放：上一个音放完，就推下一个 */

    ev = BSP_KEY_GetEvent(BSP_KEY_1);
    if      (ev == BSP_KEY_EVENT_LONG)  { Songs_Play(Songs_Index()); }
    else if (ev == BSP_KEY_EVENT_CLICK) { BSP_BEEP_PlayTone(523U, 200U); }

    ev = BSP_KEY_GetEvent(BSP_KEY_2);
    if      (ev == BSP_KEY_EVENT_LONG)  { Songs_Next(); }
    else if (ev == BSP_KEY_EVENT_DOUBLE){ BSP_BEEP_PlayTone(880U, 400U); }
    else if (ev == BSP_KEY_EVENT_CLICK) { BSP_BEEP_PlayTone(659U, 200U); }

    ev = BSP_KEY_GetEvent(BSP_KEY_3);
    if      (ev == BSP_KEY_EVENT_LONG)  { Songs_Stop(); }
    else if (ev == BSP_KEY_EVENT_CLICK) { BSP_BEEP_PlayTone(784U, 200U); }
}
