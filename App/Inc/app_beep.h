#ifndef __APP_BEEP_H
#define __APP_BEEP_H

#include "board.h"

/* 06 蜂鸣器例程（第 5 次课 PWM 那节的延伸）
   K1 短按 = 低音 do；K2 短按 = mi；K3 短按 = sol
   K1 长按 = 播一小段《小星星》；K3 长按 = 停
   板子上插无源蜂鸣器才听得出音调；有源只会一种声（把 bsp_beep.h 的 BEEP_TYPE 改掉即可） */
void APP_Beep_Init(void);
void APP_Beep_Process(void);

#endif /* __APP_BEEP_H */
