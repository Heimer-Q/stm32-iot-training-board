/**
  ******************************************************************************
  * @file    app_config.h
  * @brief   例程总开关 + 学生要改的参数（全工程只有这一处需要学生动）
  *
  * 用法：改下面 DEMO_ID 这一行，重新编译下载，就是另一个例程。
  ******************************************************************************
  */

#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

/* ============================== 例程编号 ============================== */
#define DEMO_SELFTEST     0    /* 00 板子自检：两个画面 + 按键控灯            */
#define DEMO_LIGHT        1    /* 01c/03 灯效：呼吸灯、流水灯（按键切换）     */
#define DEMO_FINAL        99   /* 99 全功能：自检→对时→显示→光控灯→上报      */

/* ★ 学生改这一行 ★ */
#define DEMO_ID           DEMO_SELFTEST

/* ============================== 可调参数 ============================== */

/* 按键扫描周期（ms），不要小于 5，太密会重复触发 */
#define CFG_KEY_SCAN_MS        10U

/* OLED 刷新周期（ms）：只在数据变化时重画，避免闪烁 */
#define CFG_OLED_REFRESH_MS    100U

/* 呼吸灯一个来回的周期（ms）——作业里"改一个数字"就改这里 */
#define CFG_BREATH_PERIOD_MS   3000U

/* ====================== 温度分档（热敏原始码 → 高/中/低） ======================
 * 上电后看串口打印的 rawT，用手捂热 / 吹冷风各看一个数，再改这两个阈值 */
#define CFG_TEMP_LOW_MAX       1200U   /* rawT < 1200            → LOW  */
#define CFG_TEMP_MID_MAX       2600U   /* 1200 ≤ rawT < 2600     → MID  */
                                        /* rawT ≥ 2600            → HIGH */

/* ====================== 光控灯（K1 长按进入）的映射区间 ======================
 * 光照 ≤ DARK_MIN 时灯最亮，≥ BRIGHT_MAX 时灯最暗（越暗越亮） */
#define CFG_LAMP_ADC_DARK_MIN    10U
#define CFG_LAMP_ADC_BRIGHT_MAX  80U

/* 串口调试波特率（USART1，接板上 DEBUG 排针） */
#define CFG_DEBUG_BAUD         115200U

#endif /* __APP_CONFIG_H */
