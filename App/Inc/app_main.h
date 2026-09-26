#ifndef __APP_MAIN_H
#define __APP_MAIN_H

#include "board.h"

/* 主循环唯一入口：按 Config/app_config.h 里的 DEMO_ID 分发到对应例程 */
void APP_Main_Init(void);
void APP_Main_Process(void);

#endif /* __APP_MAIN_H */
