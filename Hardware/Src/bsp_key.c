#include "bsp_key.h"

static GPIO_TypeDef * const key_port[BSP_KEY_NUM] = {KEY1_PORT, KEY2_PORT, KEY3_PORT};
static const uint16_t       key_pin [BSP_KEY_NUM] = {KEY1_PIN,  KEY2_PIN,  KEY3_PIN};

static uint8_t stable[BSP_KEY_NUM];      /* 已确认的电平：0=按下 */
static uint8_t counter[BSP_KEY_NUM];
static uint8_t clicked[BSP_KEY_NUM];

void BSP_KEY_Init(void)
{
    for (uint8_t i = 0; i < BSP_KEY_NUM; i++)
    {
        stable[i]  = 1U;   /* 上拉，未按下为高 */
        counter[i] = 0U;
        clicked[i] = 0U;
    }
}

void BSP_KEY_Scan(void)
{
    for (uint8_t i = 0; i < BSP_KEY_NUM; i++)
    {
        clicked[i] = 0U;
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
                clicked[i] = 1U;
            }
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
