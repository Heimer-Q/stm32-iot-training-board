#include "bsp_oled.h"

/* 本板 OLED 挂在硬件 I2C1 上（PB6/PB7），地址 7 位 0x3C
   —— 铁头山羊的驱动用 8 位写法 0x78（0x3C<<1），HAL 也收 8 位写法，不用换算 */
#define BSP_OLED_I2C      (&hi2c1)
#define BSP_OLED_I2C_TO   200U   /* ms；100kHz 下发 1KB 显存约需 92ms，留一倍余量 */

OLED_TypeDef g_oled;

static int s_last_error = 0;

/* 驱动要求的写回调：返回 0 = 成功，非 0 = 失败 */
static int OLED_I2C_Write(uint8_t addr, const uint8_t *pdata, uint16_t size)
{
    HAL_StatusTypeDef st;

    st = HAL_I2C_Master_Transmit(BSP_OLED_I2C, (uint16_t)addr,
                                 (uint8_t *)pdata, size, BSP_OLED_I2C_TO);

    return (st == HAL_OK) ? 0 : -1;
}

int BSP_OLED_Init(void)
{
    OLED_InitTypeDef init;
    int ret;

    init.i2c_write_cb = OLED_I2C_Write;

    ret = OLED_Init(&g_oled, &init);
    if (ret != 0)
    {
        /* -2 = malloc 失败：MDK-ARM/startup_stm32f103xb.s 的 Heap_Size 必须 >= 0x800 */
        s_last_error = ret;
        return ret;
    }

    OLED_SetFont(&g_oled, &default_font);
    OLED_Clear(&g_oled);

    ret = OLED_SendBuffer(&g_oled);
    if (ret != 0)
    {
        s_last_error = ret;
        return ret;
    }

    s_last_error = 0;
    return 0;
}

int BSP_OLED_LastError(void)
{
    return s_last_error;
}

void BSP_OLED_Refresh(void)
{
    (void)OLED_SendBuffer(&g_oled);
}
