#include "bsp_uart.h"

#include <stdarg.h>
#include <stdio.h>

#define BSP_UART_TO_MS   100U      /* 单次发送超时（ms），115200 下发 128 字节约 11ms */
#define BSP_UART_BUF     128U      /* printf 单条最长 127 字符，够用且不占太多栈 */

void BSP_UART_Init(void)
{
    /* 串口本身已由 CubeMX 的 MX_USART1_UART_Init() 配好（115200-8-N-1），
       这里只把接收中断打开，方便以后接"串口发命令"的玩法 */
    __HAL_UART_CLEAR_OREFLAG(&huart1);
}

int BSP_UART_Send(const uint8_t *data, uint16_t size)
{
    if (size == 0U)
    {
        return 0;
    }
    return (HAL_UART_Transmit(&huart1, (uint8_t *)data, size, BSP_UART_TO_MS) == HAL_OK) ? 0 : -1;
}

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
        n = (int)sizeof(buf) - 1;      /* 截断，不越界 */
    }
    return BSP_UART_Send((const uint8_t *)buf, (uint16_t)n);
}

int BSP_UART_ReadByte(uint8_t *out)
{
    if (out == 0)
    {
        return 0;
    }
    return (HAL_UART_Receive(&huart1, out, 1U, 0U) == HAL_OK) ? 1 : 0;
}
