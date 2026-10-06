/**
  ******************************************************************************
  * @file    bsp_uart.c
  * @brief   调试串口（USART1，接板上 DEBUG 排针）——给电脑"打印消息"用的
  *
  * == 新人导读 ================================================================
  * 1. 它解决什么问题？
  *    单片机没有屏幕给你看中间结果。调试串口就像"printf 到电脑"：
  *        BSP_UART_Printf("[tick] lgt=%d%%\r\n", light);
  *    电脑上用串口助手（115200 波特率）就能看到这一行——调代码全靠它。
  *
  * 2. 发送是"阻塞"的（等发完才返回）：
  *    115200 波特率下 128 字节约 11ms，对 200ms 的刷新节拍来说完全够用，
  *    所以这里选了最简单的"发完再走"，没有用中断/DMA——先跑通，再谈优化。
  *
  * 3. 接线：板上 DEBUG 排针 PA9(TX)/PA10(RX)/GND，接 CH340 之类的 USB 转串口。
  ******************************************************************************
  */

#include "bsp_uart.h"

#include <stdarg.h>
#include <stdio.h>

#define BSP_UART_TO_MS   100U      /* 单次发送超时（ms），115200 下发 128 字节约 11ms */
#define BSP_UART_BUF     128U      /* printf 单条最长 127 字符，够用且不占太多栈 */

/* 初始化：串口本体由 CubeMX 生成代码配好（115200-8-N-1），
   这里只清一次溢出标志，上电干净起步 */
void BSP_UART_Init(void)
{
    __HAL_UART_CLEAR_OREFLAG(&huart1);
}

/* 发一段原始字节（0 = 成功，-1 = 超时/失败）。
   超时是"保命"：外设万一异常，最多等 100ms 就放弃，不会卡死整个程序 */
int BSP_UART_Send(const uint8_t *data, uint16_t size)
{
    if (size == 0U)
    {
        return 0;
    }
    return (HAL_UART_Transmit(&huart1, (uint8_t *)data, size, BSP_UART_TO_MS) == HAL_OK) ? 0 : -1;
}

/* 像 printf 一样的格式化打印：
   先用 vsnprintf 把参数拼进缓冲区（自动防越界），再整段发出去 */
int BSP_UART_Printf(const char *fmt, ...)
{
    char    buf[BSP_UART_BUF];
    va_list ap;
    int     n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n <= 0)
    {
        return -1;
    }
    if (n > (int)sizeof(buf) - 1)
    {
        n = (int)sizeof(buf) - 1;      /* 超长内容截断，绝不越界写缓冲区 */
    }
    return BSP_UART_Send((const uint8_t *)buf, (uint16_t)n);
}

/* 读一个字节（非阻塞：0 表示这次没读到）。留给"电脑发命令给板子"的扩展用 */
int BSP_UART_ReadByte(uint8_t *out)
{
    if (out == 0)
    {
        return 0;
    }
    return (HAL_UART_Receive(&huart1, out, 1U, 0U) == HAL_OK) ? 1 : 0;
}
