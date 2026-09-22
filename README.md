# 物联网协会 2026 培训板 · HAL 工程模板

> 面向 2026 级新生培训，配套硬件见 `../../../物联网协会2026培训板资源与引脚分配表.md`
> 生成工具：STM32CubeMX 6.15.0 + STM32Cube FW_F1 V1.8.5 + Keil MDK 5.38

## 硬件映射

| 功能 | MCU 引脚 | 外设 |
|---|---|---|
| LED1 / LED2 / LED3 | PA6 / PA7 / PB0 | TIM3_CH1 / CH2 / CH3（PWM） |
| 按键 KEY1 / KEY2 / KEY3 | PB12 / PB13 / PB14 | GPIO 输入，内部上拉 |
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
| `BSP/` | 我们 | 一个外设一个模块：OLED、按键、LED、ADC、ESP、RTC |
| `APP/` | 我们 | 业务逻辑：灯效、状态机、环境读数、时钟、上报 |
| `Config/` | 我们 | `board.h` 引脚宏 + `params.h` 可调参数（学生只改这里） |
| `MDK-ARM/` | Keil | 工程文件，每次 CubeMX 生成后检查 BSP/APP 分组与头文件路径 |

## 生成与编译

1. 用 STM32CubeMX 打开 `Training.ioc`，确认固件包指向本机 F1 包（Project Manager → Code Generator 里选 `copy only necessary library files`）；
2. 点 **GENERATE CODE**；
3. 用 Keil 打开 `MDK-ARM/Training.uvprojx`，编译（F7）后用 ST-LINK 下载（F8）。

> 注意：路径里若含中文，CubeMX/Keil 偶发异常；给学生用的时候建议整体复制到 `D:\Training\` 这类纯英文路径。

## 待办

- [ ] 用 CubeMX 生成代码（本文件为手写 .ioc，需生成一次验证）
- [ ] 添加 BSP/APP 模块与 Keil 分组
