#include "bsp_key.h"

static GPIO_TypeDef * const key_port[BSP_KEY_NUM] = {KEY1_PORT, KEY2_PORT, KEY3_PORT};
static const uint16_t       key_pin [BSP_KEY_NUM] = {KEY1_PIN,  KEY2_PIN,  KEY3_PIN};

static uint8_t stable[BSP_KEY_NUM];      /* 已确认的电平：0=按下 */
static uint8_t counter[BSP_KEY_NUM];
static uint8_t clicked[BSP_KEY_NUM];
static uint8_t long_pressed[BSP_KEY_NUM];   /* 本次 Scan 是否刚达到长按阈值 */
static uint8_t long_reported[BSP_KEY_NUM];  /* 这次按住期间是否已经报过长按 */
static uint32_t press_tick[BSP_KEY_NUM];    /* 按下时刻，用于算按了多久 */

void BSP_KEY_Init(void)
{
    for (uint8_t i = 0; i < BSP_KEY_NUM; i++)
    {
        stable[i]  = 1U;   /* 上拉，未按下为高 */
        counter[i] = 0U;
        clicked[i] = 0U;
        long_pressed[i]  = 0U;
        long_reported[i] = 0U;
        press_tick[i]    = 0U;
    }
}

void BSP_KEY_Scan(void)
{
    uint32_t now = HAL_GetTick();

    for (uint8_t i = 0; i < BSP_KEY_NUM; i++)
    {
        clicked[i]      = 0U;
        long_pressed[i] = 0U;

        uint8_t raw = (HAL_GPIO_ReadPin(key_port[i], key_pin[i]) == GPIO_PIN_RESET) ? 0U : 1U;

        if (raw == stable[i])
        {
            counter[i] = 0U;
        }
        else if (++counter[i] >= BSP_KEY_DEBOUNCE)
        {
            counter[i] = 0U;
            stable[i]  = raw;
            if (stable[i] == 0U)    /* 下降沿 = 按下 */
            {
                press_tick[i]    = now;
                long_reported[i] = 0U;
            }
            else                    /* 上升沿 = 松手 */
            {
                if (!long_reported[i])
                {
                    clicked[i] = 1U;    /* 没到长按阈值就松手 → 短按 */
                }
                long_reported[i] = 0U;
            }
        }

        /* 按住不放：到达阈值时报一次长按 */
        if ((stable[i] == 0U) && (!long_reported[i]) && ((now - press_tick[i]) >= BSP_KEY_LONG_MS))
        {
            long_reported[i] = 1U;
            long_pressed[i]  = 1U;
        }
    }
}

uint8_t BSP_KEY_IsPressed(BSP_KeyId id)
{
    return (id < BSP_KEY_NUM && stable[id] == 0U) ? 1U : 0U;
}

uint8_t BSP_KEY_WasClicked(BSP_KeyId id)
{
    return (id < BSP_KEY_NUM) ? clicked[id] : 0U;
}

uint8_t BSP_KEY_WasLongPressed(BSP_KeyId id)
{
    return (id < BSP_KEY_NUM) ? long_pressed[id] : 0U;
}
