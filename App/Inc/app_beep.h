#ifndef __APP_BEEP_H
#define __APP_BEEP_H

#include "board.h"

/* 06 蜂鸣器例程（第 5 次课 PWM 那节的延伸）
   K1 短按 = 低音 do；K2 短按 = mi；K3 短按 = sol
   K1 长按 = 播一小段《小星星》；K3 长按 = 停
   板上插的是三脚蜂鸣器模块（自带驱动、低电平触发）：
   无源模块听得出音调（本课的重点）；换成有源模块就只能一种声
   —— 在 Hardware/Inc/bsp_beep.h 里把 BEEP_MODE 改成 BEEP_MODE_DC 即可 */
void APP_Beep_Init(void);
void APP_Beep_Process(void);

#endif /* __APP_BEEP_H */
