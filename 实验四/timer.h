/*
 * timer.h  --  系统时基 (Timer0, 1ms 中断)
 *
 * 节拍说明:
 *   - 1 ms 累计 1000 次产生 g_one_second_flag, 用于温度采集 / EEPROM 保存 / 串口上传
 *   - 1 ms 累计 50 次产生 g_50ms_tick, 用于按键扫描节拍
 *   - 1 ms 累计 100 次产生 g_alarm_high_tick (高温报警 5Hz)
 *   - 1 ms 累计 300 次产生 g_alarm_low_tick  (低温报警 ~1.67Hz)
 *
 * Timer1 留给 UART 波特率发生器 (uart.c 中初始化).
 */

#ifndef __TIMER_H__
#define __TIMER_H__

#include "config.h"

extern volatile bit g_50ms_tick;        /* 每 50ms 置 1, 主循环消费后清零 */
extern volatile bit g_alarm_high_tick;  /* 每 100ms 置 1 */
extern volatile bit g_alarm_low_tick;   /* 每 300ms 置 1 */
extern volatile u16 g_ms_counter;       /* 0..999 ms 计数 */

void Timer0_Init(void);

#endif
