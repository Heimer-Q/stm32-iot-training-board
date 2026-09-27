/**
  ******************************************************************************
  * @file    bsp_beep.h
  * @brief   板载蜂鸣器：PB1(TIM3_CH4) → 1k → S8050 低边开关 → J4 插座
  *
  * 硬件（2026-09-27 定的方案）：
  *   3V3 ──┬────────────── J4.1(+)
  *         │
  *      [D4 1N4148]（反并续流）
  *         │
  *         └────────────── J4.2/3/4(−) = BEEP_N
  *                            │
  *                    C ┌─────┴─────┐
  *   PB1 ─[R4 1k]─ B ──┤ Q1 S8050  │
  *                    E └─────┬─────┘
  *                          GND
  *
  * 两种蜂鸣器都能插，靠 BEEP_TYPE 切换（改完重新编译即可）：
  *   无源（BEEP_TYPE_PASSIVE，推荐）：必须给方波——改频率就是改音调，能点歌
  *   有源（BEEP_TYPE_ACTIVE）      ：给直流就响，只有一种音调
  *
  * 注意：TIM3 的 PRESCALER 与三个 LED 共用。改频率时会顺带改变 LED 的 PWM
  *       频率，但占空比不变 → 亮度不变（仍在几百 Hz 以上，肉眼看不出闪）。
  ******************************************************************************
  */

#ifndef __BSP_BEEP_H
#define __BSP_BEEP_H

#include "board.h"

/* ---- 蜂鸣器类型（板子上插的是哪种就选哪个） ---- */
#define BEEP_TYPE_ACTIVE    0U   /* 有源：写高电平就响，只能响/停 */
#define BEEP_TYPE_PASSIVE   1U   /* 无源：给方波才响，可变速变调（默认） */
#define BEEP_TYPE           BEEP_TYPE_PASSIVE

/* 默认提示音频率（无源用；有源忽略频率） */
#define BEEP_DEFAULT_HZ     2000U

void     BSP_BEEP_Init(void);
void     BSP_BEEP_On(void);                              /* 开：无源按默认音调，有源给直流 */
void     BSP_BEEP_Off(void);                             /* 关：把 TIM3 还给 LED 的 1kHz */
void     BSP_BEEP_Tone(uint16_t hz);                     /* 无源：指定音调；有源：等同 On */
void     BSP_BEEP_PlayTone(uint16_t hz, uint16_t ms);    /* 响 ms 毫秒后自动停（非阻塞） */
void     BSP_BEEP_Beep(uint16_t ms);                     /* 短鸣一声（默认音调） */
void     BSP_BEEP_Task(void);                            /* 主循环里调：到点自动停 */
uint8_t  BSP_BEEP_IsBusy(void);                          /* 1 = 还在响 */

#endif /* __BSP_BEEP_H */
