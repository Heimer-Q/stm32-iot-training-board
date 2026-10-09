#ifndef __APP_HWCHECK_H
#define __APP_HWCHECK_H

#include "board.h"

/* 最简硬件自检（DEMO_ID = DEMO_HWCHECK）：上电自动跑，不需要按任何键 */
void APP_HWCheck_Init(void);
void APP_HWCheck_Process(void);

#endif /* __APP_HWCHECK_H */
