/**
  ******************************************************************************
  * @file    app_beep.c
  * @brief   09 蜂鸣器例程：三个键发单音 + 曲库播放（曲库数据在 App/Src/app_songs.c）
  *
  * 按键：
  *   K1 / K2 / K3 单击：发一个单音（音高与长短见下面三行常量）
  *   K2 双击：发一个更高的音
  *   K1 长按：从**当前曲子的开头**播放
  *   K2 长按：切到下一首（停在待播，不自动播）
  *   K3 长按：停止
  *
  * 注意：按键事件"取走即清"，所以**一个键只能取一次事件**，然后用 if/else 分流。
  *       （老版本先取一次判长按、再取一次判单击，单击事件被前一次吃掉了 → 单击不发音。）
  ****************************************************************************** */

#include "app_beep.h"
#include "app_config.h"
#include "app_songs.h"
#include "bsp_beep.h"
#include "bsp_key.h"
#include "bsp_uart.h"

void APP_Beep_Init(void)
{
    BSP_BEEP_Init();
    BSP_KEY_Init();
    BSP_UART_Init();

    BSP_UART_Printf("\r\n=== 09 BEEP DEMO ===\r\n");
    BSP_UART_Printf("[song] %u songs in library, first = %s\r\n",
                    (unsigned)g_song_num, g_songs[0].name);
    BSP_UART_Printf("[tips] K1 hold=play  K2 hold=next  K3 hold=stop\r\n");

    BSP_BEEP_Beep(120U);        /* 上电"滴"一声：能响就说明蜂鸣器这一路是通的 */
}

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
