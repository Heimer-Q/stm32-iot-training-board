# 物联网协会 2026 培训板 · HAL 工程模板

> 面向 2026 级新生培训，配套硬件见 `../../../物联网协会2026培训板资源与引脚分配表.md`
> 生成工具：STM32CubeMX 6.15.0 + STM32Cube FW_F1 V1.8.5 + Keil MDK 5.38

## 硬件映射

| 功能 | MCU 引脚 | 外设 |
|---|---|---|
| LED1 / LED2 / LED3 | PA6 / PA7 / PB0 | TIM3_CH1 / CH2 / CH3（PWM） |
| 按键 KEY1 / KEY2 / KEY3 | PB12 / PB8 / PB9 | GPIO 输入，内部上拉（PB13/14/15 留给 SPI2 拓展排针） |
| 光敏 AO / 热敏 AO | PA0 / PA1 | ADC1_IN0 / ADC1_IN1 |
| OLED SCL / SDA | PB6 / PB7 | I2C1（从地址 0x3C） |
| ESP8266 TXD / RXD | PA3 / PA2 | USART2（115200） |
| 调试串口 | PA9 / PA10 | USART1（115200） |
| 下载调试 | PA13 / PA14 | SWD |
| RTC | PC14 / PC15 | LSE 32.768kHz |

时钟：HSE 8MHz × PLL9 = 72MHz 主频；ADC 时钟 12MHz；TIM3 PWM 1kHz（预分频 72、周期 1000）。

## 目录约定

| 目录 | 归属 | 规则 |
|---|---|---|
| `Core/` | CubeMX | 只在 `USER CODE BEGIN/END` 区改 |
| `Drivers/` | ST | 不动 |
| `Hardware/` | 我们（BSP 层） | 一个外设一个模块，只管"这块板子怎么用" |
| `App/` | 我们（APP 层） | 业务逻辑：自检、灯效、环境读数、时钟、上报 |
| `Config/` | 我们 | `board.h` 引脚宏 + `app_config.h` 例程开关与学生可调参数 |
| `MDK-ARM/` | Keil | 工程文件，每次 CubeMX 生成后检查分组与头文件路径 |

### 现有模块

| 模块 | 位置 | 干什么 |
|---|---|---|
| `bsp_led` | `Hardware/` | TIM3 三路 PWM，按百分比设亮度 |
| `bsp_key` | `Hardware/` | 三个按键扫描（10ms 周期）+ 消抖，短按事件 |
| `oled` + `oled_default_font` | `Hardware/` | **铁头山羊原版 OLED 库**（BDF 字库、Unicode 支持） |
| `bsp_oled` | `Hardware/` | 把上面那套库接到本板硬件 I2C1，提供 `BSP_OLED_Init()` |
| `app_main` | `App/` | 按 `DEMO_ID` 分发到对应例程 |
| `app_selftest` | `App/` | 00 板子自检：OLED 逐项报状态 + 按键验灯 |
| `app_light` | `App/` | 01c/03 灯效：呼吸灯、流水灯（按键切换） |

## 例程怎么切换（DEMO_ID）

全工程只有一个开关：`Config/app_config.h` 里的 `DEMO_ID`。改一行、重新编译下载，就是另一个例程。

| 编号 | 宏 | 例程 |
|---|---|---|
| 0 | `DEMO_SELFTEST` | 板子自检 |
| 1 | `DEMO_LIGHT` | 呼吸灯 / 流水灯 |
| 99 | `DEMO_FINAL` | 结课整合（全功能，逐步挂进时钟、光控、上报） |

学生要调的数字（呼吸周期、按键扫描周期、刷屏周期、调试波特率、以后的热点名/上报周期）全部集中在 `Config/app_config.h`。

## 和铁头山羊代码的关系

我们的课程让学生看铁头山羊的视频，所以**能抄就抄，函数名一个不改**：

- `Hardware/Inc/oled.h`、`oled_font.h`、`oled_default_font.h`、`Hardware/Src/oled.c` 是**铁头山羊原版文件**（保留作者注释），用法就是 `OLED_Init / OLED_Printf / OLED_SetCursor / OLED_DrawString / OLED_SendBuffer`。
- 他们那套库原本在**标准库版** `BreathingLED\my_lib` 里，搬进 HAL 工程只需要改**一行**：把 `#include "stm32f10x.h"` 换成 `#include "main.h"`（HAL 工程的设备头文件由 `main.h → stm32f1xx_hal.h` 提供；留着 `stm32f10x.h` 会和 CMSIS 的 IRQn 定义撞车，报 30 个重复声明）。
- 以后继续搬 `button.c`、`delay.c`、`usart.c`、`i2c.c`、`si2c.c`、`spi.c` 时，同样是"函数名照抄 + 这一行头文件改掉"，底层用 HAL 实现。

### 两个必须记住的坑

1. **堆**：`oled.c` 用 `malloc` 分配 1025 字节显存，`MDK-ARM/startup_stm32f103xb.s` 的 `Heap_Size` 已从 `0x200`（512B）改成 `0x800`（2KB）。**CubeMX 重新生成代码会把它改回 0x200**，生成后必须检查这一行，否则 `OLED_Init` 返回 -2、屏幕完全不亮。
2. **Flash 预算**：默认 5x8 字库占约 5.9KB RO-data；当前整工程 `Code 13464 / RO-data 6304 / RW 44 / ZI 3580`（约 20KB）。若再加 8x16 英文 + 中文子集，注意别超 Keil MDK-Lite 的 32KB 限制。

## 生成与编译

1. 用 STM32CubeMX 打开 `Training.ioc`，确认固件包指向本机 F1 包（Project Manager → Code Generator 里选 `copy only necessary library files`）；
2. 点 **GENERATE CODE**；
3. 用 Keil 打开 `MDK-ARM/Training.uvprojx`，编译（F7）后用 ST-LINK 下载（F8）。

> 注意：路径里若含中文，CubeMX/Keil 偶发异常；给学生用的时候建议整体复制到 `D:\Training\` 这类纯英文路径。

## 待办

- [x] 用 CubeMX 生成代码并编译验证（2026-09-22）
- [x] 添加 Hardware/App 模块与 Keil 分组（2026-09-22）
- [x] 按键改脚 PB8/PB9、移植 OLED 库、DEMO_ID 与自检骨架（2026-09-26）
- [ ] 字库：8x16 英文 + 中文子集（格式照铁头山羊的 BDF，生成脚本待写）
- [ ] `bsp_adc`（光敏百分比 + NTC 查表）、`bsp_uart`（printf 重定向）、按键长按连加
- [ ] 自检补 ADC 一项；例程 01/02/03/04/05/06/99 逐个补全
