#ifndef __APP_DEMO_H
#define __APP_DEMO_H

#include "board.h"

/* 全功能演示（DEMO_FINAL）：
   3 个画面（时钟/图片/状态）× 3 种模式（手动/光控/热控）+ 时间设置 + 锁屏 */
void APP_Demo_Init(void);
void APP_Demo_Process(void);

#endif /* __APP_DEMO_H */
