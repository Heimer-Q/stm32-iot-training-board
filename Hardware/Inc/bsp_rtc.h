/**
  ******************************************************************************
  * @file    bsp_rtc.h
  * @brief   内部 RTC（LSE 32.768kHz）读取
  *
  * 说明：培训板没有纽扣电池，断电时间会丢；上电时若发现时间明显不合理，
  *       就用"编译时刻"给 RTC 设一次，屏幕上至少能看到会走的钟。
  ******************************************************************************
  */

#ifndef __BSP_RTC_H
#define __BSP_RTC_H

#include "board.h"

void    BSP_RTC_Init(void);
uint8_t BSP_RTC_Valid(void);
void    BSP_RTC_Get(uint8_t *hour, uint8_t *minute, uint8_t *second);

#endif /* __BSP_RTC_H */
