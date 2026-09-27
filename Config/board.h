#ifndef __BOARD_H
#define __BOARD_H

#include "main.h"
#include "tim.h"
#include "i2c.h"
#include "usart.h"
#include "adc.h"
#include "rtc.h"
#include "gpio.h"   /* 外设句柄（htim3 / hi2c1 / huart2 ...）在各模块头文件里声明 */

/* ===== 引脚映射（与 Training.ioc 一致，改硬件时只改这里） ===== */

/* LED：TIM3 三通道 PWM —— PA6 / PA7 / PB0 */
#define LED_TIM          (&htim3)
#define LED1_CHANNEL     TIM_CHANNEL_1
#define LED2_CHANNEL     TIM_CHANNEL_2
#define LED3_CHANNEL     TIM_CHANNEL_3
#define LED_NUM          3U
#define LED_PWM_PERIOD   1000U   /* 对应 .ioc 的 Period = 1000-1 */

/* 按键：PB12 / PB8 / PB9，内部上拉，按下为低（PB13/PB14/PB15 留给 SPI2） */
#define KEY1_PORT   GPIOB
#define KEY1_PIN    GPIO_PIN_12
#define KEY2_PORT   GPIOB
#define KEY2_PIN    GPIO_PIN_8
#define KEY3_PORT   GPIOB
#define KEY3_PIN    GPIO_PIN_9

/* 蜂鸣器：PB1 = TIM3_CH4 → J4 第 3 针（三脚模块的 I/O）
   模块自带驱动三极管 + 低电平触发（PB1 拉低才响）；板上没有 Q1/R4/D4
   同一个 TIM3 的前三个通道给 LED，第四通道给蜂鸣器 */
#define BEEP_TIM          (&htim3)
#define BEEP_CHANNEL      TIM_CHANNEL_4
#define BEEP_PWM_PERIOD   1000U   /* 与 .ioc 的 Period = 1000-1 一致（ARR+1） */

/* 传感器：光敏 PA0 = ADC1_IN0，热敏 PA1 = ADC1_IN1 */
#define LIGHT_ADC_CHANNEL   ADC_CHANNEL_0
#define TEMP_ADC_CHANNEL    ADC_CHANNEL_1

/* OLED：I2C1（PB6/PB7），模块地址 0x3C */
#define OLED_I2C        (&hi2c1)
#define OLED_I2C_ADDR   (0x3C << 1)

/* ESP8266：USART2（PA2/PA3） */
#define ESP_UART        (&huart2)

/* 调试串口：USART1（PA9/PA10） */
#define DEBUG_UART      (&huart1)

#endif /* __BOARD_H */
