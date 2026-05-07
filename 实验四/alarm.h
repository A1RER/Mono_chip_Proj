/*
 * alarm.h  --  报警逻辑 (蜂鸣器 + LED)
 *
 * 高温报警 (g_temp_x10 > g_temp_high_x10):
 *   蜂鸣器 100ms 翻转一次  (5 Hz, 急促)
 *   LED_HIGH (P1^0) 100ms 翻转一次
 * 低温报警 (g_temp_x10 < g_temp_low_x10):
 *   蜂鸣器 300ms 翻转一次  (~1.67 Hz, 缓慢)
 *   LED_LOW (P1^1) 300ms 翻转一次
 * 范围内:
 *   蜂鸣器关闭, LED 全灭.
 *
 * 设计: Alarm_UpdateState() 每秒判定一次状态; Alarm_Task() 每次主循环调用,
 *       根据节拍标志非阻塞地翻转蜂鸣器 / LED.
 */

#ifndef __ALARM_H__
#define __ALARM_H__

#include "config.h"

void Alarm_Init        (void);
void Alarm_UpdateState (s16 cur_x10, s16 low_x10, s16 high_x10);
void Alarm_Task        (void);

#endif
