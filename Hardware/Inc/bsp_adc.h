/**
  ******************************************************************************
  * @file    bsp_adc.h
  * @brief   两路模拟量：光敏（PA0 / ADC1_IN0）、热敏（PA1 / ADC1_IN1）
  *
  * 用法：
  *   BSP_ADC_Init();                       // 上电一次
  *   BSP_ADC_Process();                    // 每 100ms 调一次（内部采样 + 8 点滑动平均）
  *   BSP_ADC_LightPercent();               // 0—100，越大越亮
  *   BSP_ADC_GetRaw(BSP_ADC_CH_TEMP);      // 热敏的滤波后原始码，APP 自己判高低
  ******************************************************************************
  */

#ifndef __BSP_ADC_H
#define __BSP_ADC_H

#include "board.h"

#define BSP_ADC_CH_LIGHT   0U
#define BSP_ADC_CH_TEMP    1U
#define BSP_ADC_CH_NUM     2U

void     BSP_ADC_Init(void);
void     BSP_ADC_Process(void);
uint16_t BSP_ADC_GetRaw(uint8_t ch);        /* 滤波后的 12 位原始码 0—4095 */
uint8_t  BSP_ADC_LightPercent(void);        /* 光敏百分比 0—100 */

#endif /* __BSP_ADC_H */
