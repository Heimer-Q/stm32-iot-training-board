/**
  ******************************************************************************
  * @file    app_light.c
  * @brief   01 灯效例程：呼吸灯 / 流水灯 / 常亮，按键切换
  *
  * == 新人导读 ================================================================
  * 1. 这套例程演示"用代码控制灯的亮法"：
  *      呼吸灯 = 亮度按三角波来回滑（0→100→0，约 3 秒一个来回）
  *      流水灯 = 亮的那颗每隔 200ms 往后挪一格
  *      常亮   = 三颗 100% 亮度
  *
  * 2. 按键（板上 K1 / K3）：
  *      K1 单击 = 换下一种灯效（关→呼吸→流水→常亮→关……）
  *      K3 单击 = 开/关（关的时候按一下回到呼吸）
  *
  * 3. 结构：一个"相位计数器 phase 每 10ms 加一"，灯效函数按 phase
  *    算出这一瞬间的亮度——这是嵌入式做动画的经典套路（别用 delay 卡死）。
  ******************************************************************************
  */

#include "app_light.h"
#include "bsp_led.h"
#include "bsp_key.h"

typedef enum { LIGHT_MODE_OFF = 0, LIGHT_MODE_BREATH, LIGHT_MODE_FLOW, LIGHT_MODE_ON, LIGHT_MODE_NUM } LightMode;

static LightMode mode = LIGHT_MODE_BREATH;
static uint32_t  last_tick;      /* 上次推进相位的时刻 */
static uint32_t  phase;          /* 相位：每 10ms +1，灯效都从它算 */

/* 呼吸：把 phase 切成 300 步一个来回，用"三角波"算亮度 */
static void breath_update(void)
{
    /* 三角波：0 → 100 → 0，约 3 秒一个来回 */
    uint32_t step = phase % 300U;
    uint8_t  percent = (uint8_t)((step < 150U) ? (step * 100U / 150U)
                                               : ((300U - step) * 100U / 150U));
    for (uint8_t i = 0; i < LED_NUM; i++)
    {
        BSP_LED_SetPercent(i, percent);
    }
}

/* 流水：每 20 拍（200ms）把"亮的那颗"往后挪一格 */
static void flow_update(void)
{
    uint8_t pos = (uint8_t)((phase / 20U) % LED_NUM);
    for (uint8_t i = 0; i < LED_NUM; i++)
    {
        BSP_LED_SetPercent(i, (i == pos) ? 100U : 0U);
    }
}

/* 初始化：开 LED、开按键，从"呼吸"开始 */
void APP_Light_Init(void)
{
    BSP_LED_Init();
    BSP_KEY_Init();
    mode      = LIGHT_MODE_BREATH;
    phase     = 0U;
    last_tick = HAL_GetTick();
}

/* 主循环任务：推进相位 + 处理按键 + 按当前灯效输出 */
void APP_Light_Process(void)
{
    /* 灯效相位：每 10ms 推进一步（按键由中断里的状态机负责，这里不再扫描） */
    if (HAL_GetTick() - last_tick >= BSP_KEY_TICK_MS)
    {
        last_tick += BSP_KEY_TICK_MS;
        phase++;
    }

    /* K1 短按：换灯效；K3 短按：开/关（每个键只取一次事件，if/else 分流） */
    if (BSP_KEY_GetEvent(BSP_KEY_1) == BSP_KEY_EVENT_CLICK)
    {
        mode = (LightMode)((mode + 1U) % LIGHT_MODE_NUM);
        BSP_LED_AllOff();
        phase = 0U;
    }
    if (BSP_KEY_GetEvent(BSP_KEY_3) == BSP_KEY_EVENT_CLICK)
    {
        mode = (mode == LIGHT_MODE_OFF) ? LIGHT_MODE_BREATH : LIGHT_MODE_OFF;
        BSP_LED_AllOff();
    }

    switch (mode)
    {
        case LIGHT_MODE_BREATH: breath_update();          break;
        case LIGHT_MODE_FLOW:   flow_update();            break;
        case LIGHT_MODE_ON:
            for (uint8_t i = 0; i < LED_NUM; i++) BSP_LED_SetPercent(i, 100U);
            break;
        default:                BSP_LED_AllOff();         break;
    }
}
