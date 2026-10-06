/**
  ******************************************************************************
  * @file    bsp_key.c
  * @brief   三个按键的"状态机"驱动：把物理的按下/松开，翻译成单击/双击/长按
  *
  * == 新人导读 ================================================================
  * 1. 为什么要写状态机，不能直接读引脚吗？
  *    不能，有两个现实问题：
  *      ① 抖动：机械按键按下的瞬间，电平会"哆嗦"几十毫秒（忽高忽低），
  *         直接读会把一次按下误判成好几次；
  *      ② 手势：上层想要的是"单击 / 双击 / 长按"这种高级信息，
  *         需要记录"按了多久、松手后有没有马上再按一下"。
  *    状态机就是每 10ms 检查一次按键，按规则在这几个状态之间跳转。
  *
  * 2. 四个状态在干什么（每 10ms 走一拍）：
  *       IDLE        空闲，等按键按下
  *       CONFIRM     确认按压：连续 2 拍（20ms）都是按下，才当成"真的按下"——这就是消抖
  *       HOLD        按住中：累计到 40 拍（400ms）就报"长按"，之后每 20 拍报一次
  *                   "长按持续"（调时间时按住不放就靠它）；
  *                   松手时：报过长按→结束；没到长按→进入双击判断
  *       DOUBLE_WAIT 松手后等 15 拍（150ms）：期间又按下=双击；
  *                   等满还没再按=单击
  *
  * 3. 一个必须知道的规则：事件"取走即清"
  *    上层用 BSP_KEY_GetEvent() 取事件，取一次就清零——天然防止同一次按键
  *    被执行两遍。所以：同一个键一个循环里只取一次，用 if/else 分流。
  *    （06 例程曾因为"一个键取两次"导致单击失灵，就是踩了这个坑。）
  ******************************************************************************
  */

#include "bsp_key.h"

/* 状态机的四个状态（对应上面导读里的说明） */
typedef enum
{
    KS_IDLE = 0,        /* 状态0：等待按下 */
    KS_CONFIRM,         /* 状态1：按下确认（防抖） */
    KS_HOLD,            /* 状态2：按下中（长按 / 连续长按） */
    KS_DOUBLE_WAIT      /* 状态3：双击判断 */
} KeyState;

/* 每个按键自己的"小账本"：记录它现在处于什么状态、走到第几拍、
   有没有报过长按、有没有待取走的事件 */
typedef struct
{
    KeyState state;         /* 当前状态 */
    uint8_t  tick;          /* 在当前状态里已经走了几拍（每拍 10ms） */
    uint8_t  pressed;       /* 已确认按住（给"按住不放"的逻辑用，比如时间设置） */
    uint8_t  long_fired;    /* 这一次按住是否已经报过长按 */
    uint8_t  event;         /* 待取事件（BSP_KEY_Event），被取走就清 */
} KeyCtx;

/* 按键 → 引脚 的对应表（K1/K2/K3 在哪根引脚，由 Config/board.h 定义） */
static GPIO_TypeDef * const key_port[BSP_KEY_NUM] = {KEY1_PORT, KEY2_PORT, KEY3_PORT};
static const uint16_t       key_pin [BSP_KEY_NUM] = {KEY1_PIN,  KEY2_PIN,  KEY3_PIN};

static KeyCtx  key[BSP_KEY_NUM];
static uint8_t tick_div;        /* 1ms → 10ms 的分频计数 */

/* 读一个按键的原始电平。
   引脚配了"内部上拉"：松开时是高电平，按下时被开关接到地=低电平。
   所以"读到低电平"就表示"按下"。 */
static uint8_t key_down(uint8_t i)
{
    return (HAL_GPIO_ReadPin(key_port[i], key_pin[i]) == GPIO_PIN_RESET) ? 1U : 0U;
}

/* 初始化：清空三个按键的状态账本 */
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

/* 一拍（10ms）跑一次状态机——三个按键轮流走一遍 */
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
                key[i].state = KS_CONFIRM;   /* 检测到按下，去"确认"状态防抖 */
                key[i].tick  = 0U;
            }
            break;

        /* ---------------- 状态1：按下确认（消抖） ---------------- */
        case KS_CONFIRM:
            if (down)
            {
                if (++key[i].tick >= BSP_KEY_DEBOUNCE_TICKS)   /* 连续 2 拍都按下 */
                {
                    key[i].state      = KS_HOLD;              /* 确认为真按下 */
                    key[i].tick       = 0U;
                    key[i].pressed    = 1U;
                    key[i].long_fired = 0U;                    /* 新一轮按住，长按还没报过 */
                }
            }
            else
            {
                key[i].state = KS_IDLE;      /* 只抖了一下就松了：当没按过 */
                key[i].tick  = 0U;
            }
            break;

        /* ---------------- 状态2：按住中（长按检测） ---------------- */
        case KS_HOLD:
            if (down)
            {
                key[i].tick++;

                if ((!key[i].long_fired) && (key[i].tick >= BSP_KEY_LONG_TICKS))
                {
                    /* 按住满 400ms：报"长按"（只报一次） */
                    key[i].event      = BSP_KEY_EVENT_LONG;
                    key[i].long_fired = 1U;
                    key[i].tick       = 0U;      /* 重新计时 → 支持"长按持续" */
                }
                else if (key[i].long_fired && (key[i].tick >= BSP_KEY_REPEAT_TICKS))
                {
                    /* 长按之后（每 200ms 一次）：报"长按持续"，给调时间/调阈值用 */
                    key[i].event = BSP_KEY_EVENT_LONG_REPEAT;
                    key[i].tick  = 0U;
                }
            }
            else
            {
                key[i].pressed = 0U;

                if (key[i].long_fired)
                {
                    key[i].state = KS_IDLE;      /* 长按过后松手：不再产生单击 */
                    key[i].tick  = 0U;
                }
                else if (key[i].tick < BSP_KEY_CLICK_MAX_TICKS)
                {
                    /* 没到长按就松手：先进"双击判断"，
                       等 15 拍（150ms）没有第二下，才报"单击" */
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
                key[i].event = BSP_KEY_EVENT_DOUBLE;   /* 150ms 内又按下=双击 */
                key[i].state = KS_IDLE;
                key[i].tick  = 0U;
            }
            else if (++key[i].tick >= BSP_KEY_DOUBLE_TICKS)
            {
                key[i].event = BSP_KEY_EVENT_CLICK;    /* 等满 150ms 没第二下=单击 */
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

/* SysTick 每 1ms 中断一次；这里 10 分频 → 状态机每 10ms 拍一拍。
   （放在中断里跑，主循环再忙也不会漏掉按键） */
void BSP_KEY_TickIsr(void)
{
    if (++tick_div >= BSP_KEY_TICK_MS)
    {
        tick_div = 0U;
        key_tick();
    }
}

/* 查询某个键此刻是否被按住（已消抖）——时间设置里"松手前吞事件"用它 */
uint8_t BSP_KEY_IsPressed(BSP_KeyId id)
{
    return (id < BSP_KEY_NUM) ? key[id].pressed : 0U;
}

/* 取走一个事件：取走即清，同一次按键不会被取第二遍 */
BSP_KEY_Event BSP_KEY_GetEvent(BSP_KeyId id)
{
    BSP_KEY_Event e;

    if (id >= BSP_KEY_NUM)
    {
        return BSP_KEY_EVENT_NONE;
    }

    e = (BSP_KEY_Event)key[id].event;
    key[id].event = BSP_KEY_EVENT_NONE;      /* 取走就清，防止重复处理 */
    return e;
}
