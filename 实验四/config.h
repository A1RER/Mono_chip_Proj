/*
 * config.h  --  实验五: DS18B20 温度传感器 -- 集中硬件/类型/全局变量定义
 *
 * 平台: 普中 HC6800-ES V2.0 (STC89C52RC), 晶振 11.0592 MHz
 *
 * === 引脚分配 (与实验要求一致) ===
 *   LCD1602 : RS=P2^6  RW=P2^5  EN=P2^7  D0-D7=P0
 *             [实验要求建议 4 位模式 D4-D7=P0^4-P0^7,
 *              本工程沿用既有实验三的 8 位模式以保持兼容,
 *              在 Proteus / 普中开发板上均可直接运行]
 *   DS18B20 : DQ = P3^7
 *   AT24C08 : SCL=P2^1  SDA=P2^0  (软件 I2C)
 *   KEY1    : P3^2  (板上 K3, 低有效, 短按: 5 页面循环切换)
 *   KEY2    : P3^3  (板上 K4, 低有效, 短按 +0.5C, 长按 -0.5C, 仅 SET 页生效)
 *
 *   说明: 板上 K1=P3^0(RXD), K2=P3^1(TXD) 与 UART 复用, 启用串口后无法
 *   作为按键, 故实际只剩 K3/K4 两个独立键. 为对应老师"K1/K2/K3"的三键
 *   设计, 这里采用"K2 长按代替 K3"的折中方案; 若硬件可加按键, 可在
 *   P3^4 接一颗按键到 GND, 把 -0.5 拆出来.
 *   BUZZER  : P1^5  (低电平驱动, 可在 BUZZER_ACTIVE_LOW 修改)
 *   LED_H   : P1^0  (高温报警灯, 低电平点亮)
 *   LED_L   : P1^1  (低温报警灯, 低电平点亮)
 *   UART    : RXD=P3^0  TXD=P3^1, 9600 bps
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <REG52.H>

/* === 类型别名 (Keil C51 兼容) === */
typedef unsigned char u8;
typedef unsigned int  u16;
typedef signed   int  s16;
typedef signed   long s32;
typedef unsigned long u32;

/* === 系统常量 === */
#define FOSC                11059200UL     /* 晶振频率 */
#define UART_BAUD           9600UL          /* 默认波特率 */

/* === LCD1602 控制脚 (8 位模式) === */
sbit LCD_RS = P2^6;
sbit LCD_RW = P2^5;
sbit LCD_EN = P2^7;
#define LCD_DATA_PORT  P0

/* === DS18B20 === */
sbit DS18B20_DQ     = P3^7;

/* === 软件 I2C (AT24C08) === */
sbit I2C_SCL = P2^1;
sbit I2C_SDA = P2^0;

/* === 独立按键 (低有效) ===
 * 板上 K1=P3^0(RXD)/K2=P3^1(TXD) 被 UART 占用, 仅 K3/K4 可用.
 */
sbit KEY1 = P3^2;       /* 板上 K3: 翻页 */
sbit KEY2 = P3^3;       /* 板上 K4: 短按 +0.5 / 长按 -0.5 (仅 SET 页) */

/* === 蜂鸣器 === */
sbit BUZZER = P1^5;
#define BUZZER_ACTIVE_LOW  1     /* 1=低电平响, 0=高电平响 */
#define BUZZER_PASSIVE     1     /* 1=无源蜂鸣器, 用软件方波发声; 0=有源蜂鸣器 */

/* === LED === */
sbit LED_HIGH = P2^2;            /* LED module D3: high-temperature alarm */
sbit LED_LOW  = P2^3;            /* LED module D4: low-temperature alarm */
#define LED_ACTIVE_LOW     1     /* 1=低电平点亮 */

/* === 默认参数 === */
#define DEF_TEMP_HIGH_X10       300         /* 30.0 C */
#define DEF_TEMP_LOW_X10        200         /* 20.0 C */
#define DEF_UPLOAD_INTERVAL     1           /* 1 s */

/* 限值范围, 防止误调到不合理值 */
#define LIMIT_MAX_X10           1250        /* 上限不允许超过 125.0 C */
#define LIMIT_MIN_X10          (-550)       /* 下限不允许低于 -55.0 C */

/* === LCD 页面编号 (K1 短按按此顺序循环) === */
#define LCD_PAGE_TIME_TEMP      0           /* 1) TIME + TEMP */
#define LCD_PAGE_LIMIT          1           /* 2) H / L 显示 */
#define LCD_PAGE_SET_HIGH       2           /* 3) 修改上限 (K2 +0.5 / K3 -0.5) */
#define LCD_PAGE_SET_LOW        3           /* 4) 修改下限 (K2 +0.5 / K3 -0.5) */
#define LCD_PAGE_STATE          4           /* 5) 采集状态 + 上传频率 */
#define LCD_PAGE_COUNT          5

/* === 报警状态 === */
#define ALARM_NORMAL            0
#define ALARM_HIGH              1
#define ALARM_LOW               2

/* === 全局变量 (定义在 main.c, 其他模块 extern 使用) === */
extern bit  g_collect_enable;       /* 采集使能, 0=stop, 1=run */
extern bit  g_one_second_flag;      /* 1Hz 节拍, 由 Timer0 置位 */
extern bit  g_lcd_update_flag;      /* 显示需要刷新 */
extern bit  g_uart_cmd_ready;       /* 收到完整串口命令 */

extern s16  g_temp_x10;             /* 当前温度 x10 */
extern s16  g_temp_high_x10;        /* 高温阈值 x10 */
extern s16  g_temp_low_x10;         /* 低温阈值 x10 */

extern u8   g_hour;
extern u8   g_min;
extern u8   g_sec;

extern u8   g_upload_interval;      /* 1..8 秒, 默认 1 */
extern u8   g_alarm_state;          /* ALARM_NORMAL/HIGH/LOW */
extern u8   g_lcd_page;             /* 当前 LCD 页面 LCD_PAGE_* */

#endif /* __CONFIG_H__ */
