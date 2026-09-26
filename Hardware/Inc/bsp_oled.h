/**
  ******************************************************************************
  * @file    bsp_oled.h
  * @brief   OLED 绑定层：把铁头山羊的 oled.c 接到本板硬件 I2C1（PB6/PB7）
  *
  * 说明：
  *   - 显示相关的 API 全部沿用铁头山羊那一套（OLED_Init / OLED_Printf /
  *     OLED_SetCursor / OLED_SendBuffer ...），学生看视频时的名字在课上能找到。
  *   - 本层只干两件事：① 告诉驱动“I2C 数据往哪发”；② 挡住初始化失败的原因。
  ******************************************************************************
  */

#ifndef __BSP_OLED_H
#define __BSP_OLED_H

#include "board.h"
#include "oled.h"

/* 全局显示对象：应用层直接 OLED_Printf(&g_oled, "TIME %02d:%02d", h, m); */
extern OLED_TypeDef g_oled;

/* @返回值：0 = 成功；-1 = I2C 通信失败；-2 = 显存分配失败（堆不够，见修改记录） */
int  BSP_OLED_Init(void);
int  BSP_OLED_LastError(void);

/* 把显存推给屏幕；改完画面后调一次 */
void BSP_OLED_Refresh(void);

#endif /* __BSP_OLED_H */
