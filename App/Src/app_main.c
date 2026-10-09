/**
  ******************************************************************************
  * @file    app_main.c
  * @brief   例程总入口：根据 Config/app_config.h 里的 DEMO_ID 决定跑哪套程序
  *
  * == 新人导读 ================================================================
  * 整个工程有 5 套例程：00 自检 / 01 灯效 / 03 最简自检 / 06 蜂鸣器 / 99 全功能。
  * 换例程只有一步：打开 Config/app_config.h，改 DEMO_ID 这一行，重新编译下载。
  *
  * 这个文件就是"换台开关"的接线处：它用 #if 在【编译阶段】选中一套例程，
  * 没被选中的例程根本不会编译进固件——这样烧到板子里的只有你要的那套。
  *
  * 谁在调用它？
  *   Core/Src/main.c 的主循环 while(1) 里，会不停调用 APP_Main_Process()，
  *   你的例程 Process 就在那里被一遍遍执行。
  ******************************************************************************
  */

#include "app_main.h"
#include "app_config.h"
#include "app_selftest.h"
#include "app_hwcheck.h"
#include "app_light.h"
#include "app_beep.h"
#include "app_demo.h"

/* 上电初始化：只跑一次。根据 DEMO_ID 选择对应例程的 Init */
void APP_Main_Init(void)
{
#if   (DEMO_ID == DEMO_SELFTEST)
    APP_SelfTest_Init();
#elif (DEMO_ID == DEMO_LIGHT)
    APP_Light_Init();
#elif (DEMO_ID == DEMO_BEEP)
    APP_Beep_Init();              /* 06 蜂鸣器：按键发音 + 长按播《小星星》 */
#elif (DEMO_ID == DEMO_HWCHECK)
    APP_HWCheck_Init();           /* 03 最简自检：上电自动跑，不用按键 */
#elif (DEMO_ID == DEMO_FINAL)
    APP_Demo_Init();              /* 99 全功能：六页面 + 三模式 + 时间设置 + 曲库音乐 */
#else
    /* DEMO_ID 填了不认识的编号时的兜底：三个灯全亮，
       一眼就能看出"例程编号写错了"，不会板子毫无反应让你一头雾水 */
    BSP_LED_Init();
    for (uint8_t i = 0; i < LED_NUM; i++) BSP_LED_SetPercent(i, 100U);
#endif
}

/* 主循环任务：被 main.c 的 while(1) 反复调用，例程的主要逻辑都在各自 Process 里 */
void APP_Main_Process(void)
{
#if   (DEMO_ID == DEMO_SELFTEST)
    APP_SelfTest_Process();
#elif (DEMO_ID == DEMO_LIGHT)
    APP_Light_Process();
#elif (DEMO_ID == DEMO_BEEP)
    APP_Beep_Process();
#elif (DEMO_ID == DEMO_HWCHECK)
    APP_HWCheck_Process();
#elif (DEMO_ID == DEMO_FINAL)
    APP_Demo_Process();
#endif
}
