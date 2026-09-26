#include "app_main.h"
#include "app_config.h"
#include "app_selftest.h"
#include "app_light.h"

void APP_Main_Init(void)
{
#if   (DEMO_ID == DEMO_SELFTEST)
    APP_SelfTest_Init();
#elif (DEMO_ID == DEMO_LIGHT)
    APP_Light_Init();
#elif (DEMO_ID == DEMO_FINAL)
    APP_SelfTest_Init();          /* 结课整合：后续把时钟、光控、上报一并挂进来 */
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
#elif (DEMO_ID == DEMO_FINAL)
    APP_SelfTest_Process();
#endif
}
