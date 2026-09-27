/**
  ******************************************************************************
  * @file    app_beep.c
  * @brief   06 蜂鸣器例程：同一个 TIM3，第四通道从"亮度"变成"音调"
  ******************************************************************************
  */

#include "app_beep.h"
#include "app_config.h"
#include "bsp_beep.h"
#include "bsp_key.h"

/* 《小星星》第一句（简谱 1155665 4433221），单位 Hz */
static const uint16_t melody[] = {
    523, 523, 784, 784, 880, 880, 784,
    698, 698, 659, 659, 587, 587, 523
};
#define MELODY_LEN   ((uint8_t)(sizeof(melody) / sizeof(melody[0])))
#define MELODY_NOTE_MS   260U

static uint8_t  melody_run;
static uint8_t  melody_idx;

void APP_Beep_Init(void)
{
    BSP_BEEP_Init();
    BSP_KEY_Init();

    melody_run = 0U;
    melody_idx = 0U;

    BSP_BEEP_Beep(120U);        /* 上电"滴"一声：能响就说明 Q1/D4/J4 这一路是通的 */
}

void APP_Beep_Process(void)
{
    BSP_KEY_Event ev;

    BSP_BEEP_Task();            /* 到点自动停（不阻塞主循环） */

    /* 长按 K1：从头播一段；长按 K3：停 */
    if (BSP_KEY_GetEvent(BSP_KEY_1) == BSP_KEY_EVENT_LONG)
    {
        melody_run = 1U;
        melody_idx = 0U;
    }
    if (BSP_KEY_GetEvent(BSP_KEY_3) == BSP_KEY_EVENT_LONG)
    {
        melody_run = 0U;
        BSP_BEEP_Off();
    }

    /* 三个按键各发一个音：改这两个数组里的数字就是改音高和长短 */
    ev = BSP_KEY_GetEvent(BSP_KEY_1);
    if (ev == BSP_KEY_EVENT_CLICK)  { BSP_BEEP_PlayTone(523U, 200U); }

    ev = BSP_KEY_GetEvent(BSP_KEY_2);
    if (ev == BSP_KEY_EVENT_CLICK)  { BSP_BEEP_PlayTone(659U, 200U); }
    if (ev == BSP_KEY_EVENT_DOUBLE) { BSP_BEEP_PlayTone(880U, 400U); }

    ev = BSP_KEY_GetEvent(BSP_KEY_3);
    if (ev == BSP_KEY_EVENT_CLICK)  { BSP_BEEP_PlayTone(784U, 200U); }

    /* 旋律推进：上一声自然结束后，再放下一个音 */
    if ((melody_run != 0U) && (BSP_BEEP_IsBusy() == 0U))
    {
        if (melody_idx >= MELODY_LEN)
        {
            melody_run = 0U;    /* 播完收工 */
        }
        else
        {
            BSP_BEEP_PlayTone(melody[melody_idx], MELODY_NOTE_MS);
            melody_idx++;
        }
    }
}
