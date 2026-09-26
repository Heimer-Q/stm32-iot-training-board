#ifndef __BSP_KEY_H
#define __BSP_KEY_H

#include "board.h"

#define BSP_KEY_NUM     3U
#define BSP_KEY_SCAN_MS 10U    /* Scan 调用周期 */
#define BSP_KEY_DEBOUNCE 2U    /* 连续 2 次读到同一电平即确认（约 20ms） */
#define BSP_KEY_LONG_MS 600U   /* 按住超过这个时间算长按 */

typedef enum { BSP_KEY_1 = 0, BSP_KEY_2 = 1, BSP_KEY_3 = 2 } BSP_KeyId;

void    BSP_KEY_Init(void);
void    BSP_KEY_Scan(void);                     /* 每 10ms 调一次 */
uint8_t BSP_KEY_IsPressed(BSP_KeyId id);        /* 当前是否按下 */
uint8_t BSP_KEY_WasClicked(BSP_KeyId id);       /* 短按事件（松手时产生，按住不超 BSP_KEY_LONG_MS） */
uint8_t BSP_KEY_WasLongPressed(BSP_KeyId id);   /* 长按事件（按住超过阈值时报一次，松手不重复） */

#endif /* __BSP_KEY_H */
