#include "bsp_key.h"

typedef enum
{
    KS_IDLE = 0,        /* 状态0：等待按下 */
    KS_CONFIRM,         /* 状态1：按下确认（防抖） */
    KS_HOLD,            /* 状态2：按下中（长按 / 连续长按） */
    KS_DOUBLE_WAIT      /* 状态3：双击判断 */
} KeyState;

typedef struct
{
    KeyState state;
    uint8_t  tick;          /* 当前状态内的拍数 */
    uint8_t  pressed;       /* 已确认按下 */
    uint8_t  long_fired;    /* 这次按住是否已经报过长按 */
    uint8_t  event;         /* 待取事件（BSP_KEY_Event） */
} KeyCtx;

static GPIO_TypeDef * const key_port[BSP_KEY_NUM] = {KEY1_PORT, KEY2_PORT, KEY3_PORT};
static const uint16_t       key_pin [BSP_KEY_NUM] = {KEY1_PIN,  KEY2_PIN,  KEY3_PIN};

static KeyCtx  key[BSP_KEY_NUM];
static uint8_t tick_div;        /* 1ms → 10ms 的分频计数 */

static uint8_t key_down(uint8_t i)
{
    return (HAL_GPIO_ReadPin(key_port[i], key_pin[i]) == GPIO_PIN_RESET) ? 1U : 0U;
}

void BSP_KEY_Init(void)
{
    for (uint8_t i = 0U; i < BSP_KEY_NUM; i++)
    {
        key[i].state      = KS_IDLE;
        key[i].tick       = 0U;
        key[i].pressed    = 0U;
        key[i].long_fired = 0U;
        key[i].event      = BSP_KEY_EVENT_NONE;
    }
    tick_div = 0U;
}

/* 一拍（10ms）跑一次状态机 —— 相当于《按键状态机》里 TIM4 中断里的那段 */
static void key_tick(void)
{
    for (uint8_t i = 0U; i < BSP_KEY_NUM; i++)
    {
        uint8_t down = key_down(i);

        switch (key[i].state)
        {
        /* ---------------- 状态0：等待按下 ---------------- */
        case KS_IDLE:
            if (down)
            {
                key[i].state = KS_CONFIRM;
                key[i].tick  = 0U;
            }
            break;

        /* ---------------- 状态1：按下确认 ---------------- */
        case KS_CONFIRM:
            if (down)
            {
                if (++key[i].tick >= BSP_KEY_DEBOUNCE_TICKS)
                {
                    key[i].state      = KS_HOLD;
                    key[i].tick       = 0U;
                    key[i].pressed    = 1U;
                    key[i].long_fired = 0U;
                }
            }
            else
            {
                key[i].state = KS_IDLE;      /* 抖动，当没按过 */
                key[i].tick  = 0U;
            }
            break;

        /* ---------------- 状态2：按下中（长按检测） ---------------- */
        case KS_HOLD:
            if (down)
            {
                key[i].tick++;

                if ((!key[i].long_fired) && (key[i].tick >= BSP_KEY_LONG_TICKS))
                {
                    key[i].event      = BSP_KEY_EVENT_LONG;
                    key[i].long_fired = 1U;
                    key[i].tick       = 0U;      /* 重置计时 → 支持连续长按 */
                }
                else if (key[i].long_fired && (key[i].tick >= BSP_KEY_REPEAT_TICKS))
                {
                    key[i].event = BSP_KEY_EVENT_LONG_REPEAT;
                    key[i].tick  = 0U;
                }
            }
            else
            {
                key[i].pressed = 0U;

                if (key[i].long_fired)
                {
                    key[i].state = KS_IDLE;      /* 长按后松手：不再产生单击 */
                    key[i].tick  = 0U;
                }
                else if (key[i].tick < BSP_KEY_CLICK_MAX_TICKS)
                {
                    key[i].state = KS_DOUBLE_WAIT;
                    key[i].tick  = 0U;
                }
                else
                {
                    key[i].state = KS_IDLE;      /* 按太久又没到长按阈值：丢掉 */
                    key[i].tick  = 0U;
                }
            }
            break;

        /* ---------------- 状态3：双击判断 ---------------- */
        case KS_DOUBLE_WAIT:
            if (down)
            {
                key[i].event = BSP_KEY_EVENT_DOUBLE;
                key[i].state = KS_IDLE;
                key[i].tick  = 0U;
            }
            else if (++key[i].tick >= BSP_KEY_DOUBLE_TICKS)
            {
                key[i].event = BSP_KEY_EVENT_CLICK;   /* 等够 150ms 没第二下 → 单击 */
                key[i].state = KS_IDLE;
                key[i].tick  = 0U;
            }
            break;

        default:
            key[i].state = KS_IDLE;
            break;
        }
    }
}

/* SysTick 是 1ms 一次；这里 10 分频后跑状态机 */
void BSP_KEY_TickIsr(void)
{
    if (++tick_div >= BSP_KEY_TICK_MS)
    {
        tick_div = 0U;
        key_tick();
    }
}

uint8_t BSP_KEY_IsPressed(BSP_KeyId id)
{
    return (id < BSP_KEY_NUM) ? key[id].pressed : 0U;
}

BSP_KEY_Event BSP_KEY_GetEvent(BSP_KeyId id)
{
    BSP_KEY_Event e;

    if (id >= BSP_KEY_NUM)
    {
        return BSP_KEY_EVENT_NONE;
    }

    e = (BSP_KEY_Event)key[id].event;
    key[id].event = BSP_KEY_EVENT_NONE;      /* 事件触发标记：取走就清，防止重复处理 */
    return e;
}
