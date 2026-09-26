/**
  ******************************************************************************
  * @file    bsp_uart.h
  * @brief   调试串口（USART1 / PA9-PA10 / 板上 DEBUG 排针）
  *
  * 对应铁头山羊的 usart.c：他那套是 My_USART_SendString / My_USART_Printf，
  * 这里保持同样的意思，只是把 USART 句柄固定成本板的 huart1，学生不用传参数。
  ******************************************************************************
  */

#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "board.h"

void BSP_UART_Init(void);

/* 发送：返回 0 = 成功，-1 = 失败 */
int  BSP_UART_Send(const uint8_t *data, uint16_t size);

/* 格式化打印（相当于铁头山羊的 My_USART_Printf），自动补 \r\n 由调用者自己写 */
int  BSP_UART_Printf(const char *fmt, ...);

/* 非阻塞收一个字节：返回 1 = 收到，0 = 暂时没数据 */
int  BSP_UART_ReadByte(uint8_t *out);

#endif /* __BSP_UART_H */
