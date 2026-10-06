#include "bsp_oled.h"

/* 字体：8x16 英文由 oled.c 在初始化时设为默认；这里只带学生自己生成的那份。
   注意：字库头文件里是"实体定义"（不是 static），**同一个字库只能被一个 .c 包含**，
        否则链接会报 L6200E: Symbol ... multiply defined。 */
#include "oled_font_user.h"

/* 8×16 英文字库定义在 oled.c 里（那边只能被一个 .c 包含，这里只做外部声明） */
extern const Font_TypeDef font_ascii8x16;

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

    /* 一整屏 1024 字节，100kHz 要传 92ms，画面刷新会明显拖住主循环；
       SSD1306 支持 400kHz，提到 400kHz 后一屏约 23ms（实测杜邦线也稳）。
       若某批模块在 400kHz 下报 -1，改回 100000U 即可。 */
    hi2c1.Init.ClockSpeed = 400000U;
    (void)HAL_I2C_Init(&hi2c1);

    init.i2c_write_cb = OLED_I2C_Write;

    ret = OLED_Init(&g_oled, &init);
    if (ret != 0)
    {
        /* -2 = malloc 失败：MDK-ARM/startup_stm32f103xb.s 的 Heap_Size 必须 >= 0x800 */
        s_last_error = ret;
        return ret;
    }

    /* 画笔/画刷（这里容易搞反，记牢）：
       - PenColor   = WHITE(点亮)：负责画字的"笔画"；
       - Brush      = BLACK(熄灭)：负责字格背景，也就是把上一次的字擦掉；
       - Brush 用 WHITE 会把整个字格点亮 → 满屏白块（花屏）；
       - Brush 用 TRANSPARENT（驱动默认）不擦旧像素 → 数字叠在一起。 */
    OLED_SetPen(&g_oled, PEN_COLOR_WHITE, 1);
    OLED_SetBrush(&g_oled, BRUSH_BLACK);

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

void BSP_OLED_ShowUserText(int16_t x, int16_t baseline_y)
{
    const Font_TypeDef *keep = g_oled.Font;

    OLED_SetFont(&g_oled, &font_user);
    OLED_SetCursor(&g_oled, x, baseline_y);
    OLED_DrawString(&g_oled, USER_FONT_TEXT);
    OLED_SetFont(&g_oled, keep);
}

void BSP_OLED_ShowUserLine(uint8_t idx, int16_t x, int16_t baseline_y)
{
    const Font_TypeDef *keep = g_oled.Font;
    const char *text;

    switch (idx)
    {
        case 1U:  text = USER_TEXT_2; break;
        case 2U:  text = USER_TEXT_3; break;
        default:  text = USER_TEXT_1; break;
    }

    OLED_SetFont(&g_oled, &font_user);
    OLED_SetCursor(&g_oled, x, baseline_y);
    OLED_DrawString(&g_oled, text);
    OLED_SetFont(&g_oled, keep);
}

void BSP_OLED_ShowUserLineCenter(uint8_t idx, int16_t baseline_y)
{
    const Font_TypeDef *keep = g_oled.Font;
    const char *text;
    uint16_t w;

    switch (idx)
    {
        case 1U:  text = USER_TEXT_2; break;
        case 2U:  text = USER_TEXT_3; break;
        default:  text = USER_TEXT_1; break;
    }

    OLED_SetFont(&g_oled, &font_user);
    w = OLED_GetStrWidth(&g_oled, text);          /* 中文一行 16 像素/字 */
    OLED_SetCursor(&g_oled, (int16_t)((128 - (int16_t)w) / 2), baseline_y);
    OLED_DrawString(&g_oled, text);
    OLED_SetFont(&g_oled, keep);
}

void BSP_OLED_ShowUserLineWithText(uint8_t idx, const char *tail, int16_t baseline_y)
{
    const Font_TypeDef *keep = g_oled.Font;
    const char *cn;
    uint16_t w_cn, w_tail = 0U, total;
    int16_t  x;

    switch (idx)
    {
        case 1U:  cn = USER_TEXT_2; break;
        case 2U:  cn = USER_TEXT_3; break;
        default:  cn = USER_TEXT_1; break;
    }

    OLED_SetFont(&g_oled, &font_user);
    w_cn = OLED_GetStrWidth(&g_oled, cn);

    if ((tail != 0) && (tail[0] != '\0'))
    {
        OLED_SetFont(&g_oled, &font_ascii8x16);
        w_tail = OLED_GetStrWidth(&g_oled, tail);
    }

    total = (uint16_t)(w_cn + w_tail);
    x     = (int16_t)((128 - (int16_t)total) / 2);

    OLED_SetFont(&g_oled, &font_user);
    OLED_SetCursor(&g_oled, x, baseline_y);
    OLED_DrawString(&g_oled, cn);

    if (w_tail != 0U)
    {
        OLED_SetFont(&g_oled, &font_ascii8x16);
        OLED_SetCursor(&g_oled, (int16_t)(x + (int16_t)w_cn), baseline_y);
        OLED_DrawString(&g_oled, tail);
    }

    OLED_SetFont(&g_oled, keep);
}

void BSP_OLED_DrawTextInverse(int16_t baseline_y, const char *text)
{
    const Font_TypeDef *keep = g_oled.Font;
    uint16_t w = OLED_GetStrWidth(&g_oled, text);
    int16_t  x = (int16_t)((128 - (int16_t)w) / 2);

    /* 反白＝把默认的"白笔 + 黑刷"反过来：
       黑笔把字形像素熄灭，白刷把每个字格填白 → 白底黑字，且光带只包住文字本身 */
    OLED_SetCursor(&g_oled, x, baseline_y);
    OLED_SetPen(&g_oled, PEN_COLOR_BLACK, 1);
    OLED_SetBrush(&g_oled, BRUSH_WHITE);
    OLED_DrawString(&g_oled, text);

    /* 恢复默认：白笔 + 黑刷（不恢复的话，后面所有文字都会跟着反白） */
    OLED_SetPen(&g_oled, PEN_COLOR_WHITE, 1);
    OLED_SetBrush(&g_oled, BRUSH_BLACK);
    OLED_SetFont(&g_oled, keep);
}

void BSP_OLED_DrawTextInverseAt(int16_t x, int16_t baseline_y, const char *text)
{
    /* 与 BSP_OLED_DrawTextInverse 相同（白底黑字、光带只包文字），但**不居中**：
       由调用者指定起始列 x——时间页用它只反白"选中的那两个数字"。 */
    OLED_SetCursor(&g_oled, x, baseline_y);
    OLED_SetPen(&g_oled, PEN_COLOR_BLACK, 1);
    OLED_SetBrush(&g_oled, BRUSH_WHITE);
    OLED_DrawString(&g_oled, text);

    /* 恢复默认：白笔 + 黑刷（不恢复的话后面所有文字都会跟着反白） */
    OLED_SetPen(&g_oled, PEN_COLOR_WHITE, 1);
    OLED_SetBrush(&g_oled, BRUSH_BLACK);
}
