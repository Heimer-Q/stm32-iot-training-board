/**
  ******************************************************************************
  * @file    bsp_beep.h
  * @brief   板载蜂鸣器：PB1(TIM3_CH4) → J4 三针插座（插三脚蜂鸣器模块）
  *
  * 硬件（2026-09-27 方案 A 定稿）：
  *   J4 第 1 针 ── 3V3   ┐
  *   J4 第 2 针 ── GND   ├─ 插三脚蜂鸣器模块（模块丝印 VCC / I/O / GND）
  *   J4 第 3 针 ── PB1   ┘
  *
  *   模块**自带驱动三极管**（板上没有 Q1 / R4 / D4），而且买到的这批是
  *   **低电平触发**：I/O 拉低才响、拉高静音——所以下面 BEEP_TRIG 默认是 LOW。
  *
  * 两个宏按实际模块改（改完重新编译即可）：
  *   BEEP_TRIG  ：触发极性（LOW = 低电平触发；买到高电平触发的模块就改 HIGH）
  *   BEEP_MODE  ：模块里是无源还是有源（TONE = 无源，必须给方波才能变调；DC = 有源，给电平就响）
  *
  * 注意：TIM3 的 PRESCALER 与三个 LED 共用。改频率时会顺带改变 LED 的 PWM
  *       频率，但占空比不变 → 亮度不变（仍在几百 Hz 以上，肉眼看不出闪）。
  ******************************************************************************
  */

#ifndef __BSP_BEEP_H
#define __BSP_BEEP_H

#include "board.h"

/* ---- 触发极性（板子上插的模块是哪种就选哪个） ---- */
#define BEEP_TRIG_LOW       1U   /* 低电平触发：I/O 拉低才响（当前这批模块，默认） */
#define BEEP_TRIG_HIGH      0U   /* 高电平触发 */
#define BEEP_TRIG           BEEP_TRIG_LOW

/* ---- 模块里装的是无源还是有源蜂鸣器 ---- */
#define BEEP_MODE_TONE      0U   /* 无源：必须给方波才响，改频率能变调（默认，卖家标"无源模块"） */
#define BEEP_MODE_DC        1U   /* 有源：给持续电平就响，只有一种音调 */
#define BEEP_MODE           BEEP_MODE_TONE

/* 默认提示音频率（无源用；有源忽略频率） */
#define BEEP_DEFAULT_HZ     2000U

void     BSP_BEEP_Init(void);                            /* 初始化并**先静音**（避免上电就叫） */
void     BSP_BEEP_On(void);                              /* 开：无源按默认音调，有源给持续电平 */
void     BSP_BEEP_Off(void);                             /* 关（静音）：把 TIM3 还给 LED 的 1kHz */
void     BSP_BEEP_Tone(uint16_t hz);                     /* 指定音调（方波）；给 0 = 静音 */
void     BSP_BEEP_PlayTone(uint16_t hz, uint16_t ms);    /* 响 ms 毫秒后自动停（非阻塞） */
void     BSP_BEEP_Beep(uint16_t ms);                     /* 短鸣一声（默认音调） */
void     BSP_BEEP_Task(void);                            /* 主循环里调：到点自动停 */
uint8_t  BSP_BEEP_IsBusy(void);                          /* 1 = 还在响 */

#endif /* __BSP_BEEP_H */
