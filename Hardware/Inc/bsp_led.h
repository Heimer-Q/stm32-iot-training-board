#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "board.h"

void BSP_LED_Init(void);
void BSP_LED_SetPercent(uint8_t index, uint8_t percent);   /* index 0..2, percent 0..100 */
void BSP_LED_AllOff(void);

#endif /* __BSP_LED_H */
