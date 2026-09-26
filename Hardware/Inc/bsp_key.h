/**
  ******************************************************************************
  * @file    bsp_key.h
  * @brief   3 个按键：4 状态机 + 事件输出（参照《按键状态机》文档）
  *
  * 状态流转（每 10ms 走一拍）：
  *   状态0 等待按下 → 状态1 按下确认（防抖）→ 状态2 按下中（长按 / 连续长按）
  *                                          ↘ 松手 → 状态3 双击判断 → 单击
  * 产生的事件（取走即清，天然防重复处理）：
  *   CLICK 短按（松手后等 15 拍 = 150ms，没有第二下才报 —— 严格照文档）
  *   DOUBLE 双击（150ms 内再按下）/ LONG 长按（40 拍 = 400ms）
  *   LONG_REPEAT 长按持续（之后每 200ms 一次，调时间用）
  *
  * 用法：
  *   ① 在 SysTick 中断里调 BSP_KEY_TickIsr()（1ms 进一次，内部 10 分频）
  *   ② 主循环里 switch (BSP_KEY_GetEvent(BSP_KEY_1)) { ... }
  ******************************************************************************
  */

#ifndef __BSP_KEY_H
#define __BSP_KEY_H

#include "board.h"

#define BSP_KEY_NUM             3U

/* ---- 阈值（单位：10ms 拍，与《按键状态机》一致） ---- */
#define BSP_KEY_DEBOUNCE_TICKS  2U    /* 按下确认：连续 2 拍为按下（20ms） */
#define BSP_KEY_LONG_TICKS      40U   /* 400ms 算长按 */
#define BSP_KEY_CLICK_MAX_TICKS 70U   /* 松手时按下时间 < 700ms 才算短按 */
#define BSP_KEY_DOUBLE_TICKS    15U   /* 松手后 150ms 内再按 = 双击（照《按键状态机》文档） */
#define BSP_KEY_REPEAT_TICKS    20U   /* 长按之后每 200ms 触发一次（调时间用） */

#define BSP_KEY_TICK_MS         10U   /* 状态机节拍 */

typedef enum { BSP_KEY_1 = 0, BSP_KEY_2 = 1, BSP_KEY_3 = 2 } BSP_KeyId;

typedef enum
{
    BSP_KEY_EVENT_NONE = 0,
    BSP_KEY_EVENT_CLICK,          /* 短按（双击窗口结束后产生） */
    BSP_KEY_EVENT_DOUBLE,         /* 双击 */
    BSP_KEY_EVENT_LONG,           /* 长按（首次） */
    BSP_KEY_EVENT_LONG_REPEAT     /* 长按持续：之后每 200ms 一次 */
} BSP_KEY_Event;

void           BSP_KEY_Init(void);
void           BSP_KEY_TickIsr(void);                      /* SysTick(1ms) 中断里调 */
uint8_t        BSP_KEY_IsPressed(BSP_KeyId id);            /* 已消抖的当前按下状态 */
BSP_KEY_Event  BSP_KEY_GetEvent(BSP_KeyId id);             /* 取事件，取走即清 */

#endif /* __BSP_KEY_H */
