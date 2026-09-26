#include "app_light.h"
#include "bsp_led.h"
#include "bsp_key.h"

typedef enum { LIGHT_MODE_OFF = 0, LIGHT_MODE_BREATH, LIGHT_MODE_FLOW, LIGHT_MODE_ON, LIGHT_MODE_NUM } LightMode;

static LightMode mode = LIGHT_MODE_BREATH;
static uint32_t  last_tick;
static uint32_t  phase;

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

static void flow_update(void)
{
    uint8_t pos = (uint8_t)((phase / 20U) % LED_NUM);
    for (uint8_t i = 0; i < LED_NUM; i++)
    {
        BSP_LED_SetPercent(i, (i == pos) ? 100U : 0U);
    }
}

void APP_Light_Init(void)
{
    BSP_LED_Init();
    BSP_KEY_Init();
    mode      = LIGHT_MODE_BREATH;
    phase     = 0U;
    last_tick = HAL_GetTick();
}

void APP_Light_Process(void)
{
    /* 灯效相位：每 10ms 推进一步（按键由中断里的状态机负责，这里不再扫描） */
    if (HAL_GetTick() - last_tick >= BSP_KEY_TICK_MS)
    {
        last_tick += BSP_KEY_TICK_MS;
        phase++;
    }

    /* K1 短按：换灯效；K3 短按：开/关 */
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
