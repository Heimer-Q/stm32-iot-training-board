#include "app_main.h"
#include "app_config.h"
#include "app_selftest.h"
#include "app_light.h"
#include "app_beep.h"
#include "app_demo.h"

void APP_Main_Init(void)
{
#if   (DEMO_ID == DEMO_SELFTEST)
    APP_SelfTest_Init();
#elif (DEMO_ID == DEMO_LIGHT)
    APP_Light_Init();
#elif (DEMO_ID == DEMO_BEEP)
    APP_Beep_Init();              /* 06 蜂鸣器：按键发音 + 长按播《小星星》 */
#elif (DEMO_ID == DEMO_FINAL)
    APP_Demo_Init();              /* 全功能：3 画面 × 3 模式 + 时间设置 + 锁屏 */
#else
    BSP_LED_Init();
    for (uint8_t i = 0; i < LED_NUM; i++) BSP_LED_SetPercent(i, 100U);   /* 未知编号：三个灯全亮 */
#endif
}

void APP_Main_Process(void)
{
#if   (DEMO_ID == DEMO_SELFTEST)
    APP_SelfTest_Process();
#elif (DEMO_ID == DEMO_LIGHT)
    APP_Light_Process();
#elif (DEMO_ID == DEMO_BEEP)
    APP_Beep_Process();
#elif (DEMO_ID == DEMO_FINAL)
    APP_Demo_Process();
#endif
}
